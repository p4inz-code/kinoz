#include "core/Updater.h"
#include "core/JsonStore.h"
#include "core/Paths.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QTimer>

static const char* kRepo = "p4inz-code/kinoz";

Updater::Updater(JsonStore* settings, QObject* parent) : QObject(parent), m_set(settings) {}

QNetworkAccessManager* Updater::net() {
    if (!m_net) {
        m_net = new QNetworkAccessManager(this);
        m_net->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);
    }
    return m_net;
}

QString Updater::current() { return QStringLiteral(APP_VERSION); }

bool Updater::isNewer(const QString& a, const QString& b) {
    auto parts = [](QString v) {
        if (v.startsWith('v', Qt::CaseInsensitive)) v.remove(0, 1);
        QList<int> out;
        for (const QString& p : v.split('.')) out << p.section(QRegularExpression("[^0-9]"), 0, 0).toInt();
        while (out.size() < 3) out << 0;
        return out;
    };
    const auto x = parts(a), y = parts(b);
    for (int i = 0; i < qMax(x.size(), y.size()); ++i) {
        const int xi = i < x.size() ? x[i] : 0, yi = i < y.size() ? y[i] : 0;
        if (xi != yi) return xi > yi;
    }
    return false;
}

bool Updater::urlAllowed(const QUrl& u) {
    const QString prefix = QStringLiteral("/%1/releases/").arg(QString::fromLatin1(kRepo));
    return u.scheme() == "https" && u.host() == "github.com" && u.path().startsWith(prefix);
}

void Updater::setState(State s, const QString& err, const QString& info) {
    m_state = s; m_error = err; m_info = info;
    emit stateChanged();
}

QString Updater::stagingDir() { return Paths::tmpDir() + QStringLiteral("/update"); }

int Updater::mode() const {
    if (m_set->has("updateMode")) return qBound(0, int(m_set->num("updateMode", Automatic)), 2);
    return m_set->flag("autoUpdate", true) ? Automatic : Off;       // older settings files had a plain on/off switch
}

void Updater::setMode(int m) {
    m = qBound(0, m, 2);
    m_set->set("updateMode", m);
    m_set->set("autoUpdate", m != Off);
    if (m == Off) discardStaged();                                  // opting out also cancels a staged update
}

void Updater::checkAutoIfDue() {
    if (mode() == Off) return;
    const qint64 last = qint64(m_set->num("lastUpdateCheck", 0));
    if (QDateTime::currentSecsSinceEpoch() - last < 24 * 3600) return;
    checkNow(false);
}

void Updater::discardStaged() {
    QDir d(stagingDir());
    for (const QFileInfo& fi : d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) QFile::remove(fi.absoluteFilePath());
}

// Remember a verified download so it can be installed at the next start if the user doesn't restart now.
void Updater::stage() {
    QJsonObject o;
    o["version"] = m_latest; o["file"] = m_file; o["sha"] = m_sha; o["name"] = m_assetName;
    QFile f(stagingDir() + QStringLiteral("/pending.json"));
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    m_set->set("updateAttempts", 0);
}

bool Updater::applyPendingOnStartup(JsonStore* settings, bool dryRun) {
#ifndef Q_OS_WIN
    Q_UNUSED(settings) Q_UNUSED(dryRun)
    return false;
#else
    QFile pf(stagingDir() + QStringLiteral("/pending.json"));
    if (!pf.open(QIODevice::ReadOnly)) return false;
    const QJsonObject o = QJsonDocument::fromJson(pf.readAll()).object();
    pf.close();
    const QString version = o.value("version").toString(), file = o.value("file").toString(), sha = o.value("sha").toString().toLower();
    if (!isNewer(version, current())) {                              // already installed (or stale): tidy up
        discardStaged();
        settings->set("updateAttempts", 0);
        return false;
    }
    const int m = settings->has("updateMode") ? int(settings->num("updateMode", Automatic)) : (settings->flag("autoUpdate", true) ? int(Automatic) : int(Off));
    if (m == Off) { discardStaged(); return false; }                  // the user opted out since downloading
    const int attempts = int(settings->num("updateAttempts", 0));
    if (attempts >= 2) {                                              // two tries were not enough: never loop, tell the user
        discardStaged();
        settings->set("updateAttempts", 0);
        settings->set("updateFailedVersion", version);
        return false;
    }
    // The file must be inside the staging folder and still match the checksum it was verified with.
    const QFileInfo fi(file);
    QFile f(file);
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (fi.absolutePath() != QFileInfo(stagingDir()).absoluteFilePath() || sha.size() != 64 || !f.open(QIODevice::ReadOnly) || !h.addData(&f)
        || QString::fromLatin1(h.result().toHex()).toLower() != sha) {
        discardStaged();
        return false;
    }
    f.close();
    settings->set("updateAttempts", attempts + 1);
    settings->flush();                                                // the counter must survive the restart the installer causes
    if (dryRun) return true;
    if (!QProcess::startDetached(file, {QStringLiteral("/SILENT"), QStringLiteral("/SUPPRESSMSGBOXES"), QStringLiteral("/NORESTART"), QStringLiteral("/CLOSEAPPLICATIONS")})) return false;
    return true;
#endif
}

void Updater::checkNow(bool manual) {
    if (m_state == Checking || m_state == Downloading) return;
    m_manual = manual;
    setState(Checking, {}, tr("Checking…"));
    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QString::fromLatin1(kRepo))));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", QByteArray("kinoz/") + APP_VERSION);
    req.setTransferTimeout(15000);
    m_reply = net()->get(req);
    connect(m_reply, &QNetworkReply::finished, this, [this, r = m_reply] { onChecked(r); });
}

void Updater::onChecked(QNetworkReply* r) {
    r->deleteLater();
    m_reply = nullptr;
    m_set->set("lastUpdateCheck", double(QDateTime::currentSecsSinceEpoch()));
    const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    if (http == 404) { setState(UpToDate, {}, tr("You're up to date (no releases published yet).")); return; }
    if (r->error() != QNetworkReply::NoError) {
        setState(Failed, tr("Couldn't reach the update server. Check your connection."), {});
        return;
    }
    const QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
    QString tag = o.value("tag_name").toString();
    if (tag.isEmpty()) { setState(Failed, tr("The update server sent an unexpected answer."), {}); return; }
    if (tag.startsWith('v', Qt::CaseInsensitive)) tag.remove(0, 1);
    if (!isNewer(tag, current())) { setState(UpToDate, {}, tr("You're up to date (version %1).").arg(current())); return; }
    m_latest = tag;
    m_notes = o.value("body").toString();
    m_page = QUrl(o.value("html_url").toString());
    m_assetUrl = QUrl(); m_directUrl = QUrl(); m_assetName.clear(); m_sha.clear();
#ifdef Q_OS_WIN
    static const QRegularExpression pat("^kinoz-Setup-.*\\.exe$");
#elif defined(Q_OS_MACOS)
    static const QRegularExpression pat("^kinoz-.*\\.dmg$");
#else
    static const QRegularExpression pat("^kinoz-.*\\.AppImage$");
#endif
    QString shaUrl;
    for (const auto& v : o.value("assets").toArray()) {
        const QJsonObject a = v.toObject();
        const QString name = a.value("name").toString();
        if (pat.match(name).hasMatch()) {
            m_assetName = name; m_assetUrl = QUrl(a.value("browser_download_url").toString());
            const QString dg = a.value("digest").toString();
            if (dg.startsWith("sha256:")) m_sha = dg.mid(7).toLower();
        }
    }
    for (const auto& v : o.value("assets").toArray()) {
        const QJsonObject a = v.toObject();
        if (!m_assetName.isEmpty() && a.value("name").toString() == m_assetName + ".sha256") shaUrl = a.value("browser_download_url").toString();
    }
    if (urlAllowed(m_assetUrl)) m_directUrl = m_assetUrl;                // for the "download it yourself" link
    if (m_assetName.isEmpty() || !urlAllowed(m_assetUrl)) {
        setState(Available, {}, tr("Version %1 is available. Open the release page to download it.").arg(m_latest));
        m_assetUrl = QUrl();
        if (!m_manual) emit updateFound(m_latest);
        return;
    }
    if (m_sha.isEmpty() && !shaUrl.isEmpty() && urlAllowed(QUrl(shaUrl))) {
        QNetworkRequest req{QUrl(shaUrl)};
        req.setRawHeader("User-Agent", QByteArray("kinoz/") + APP_VERSION);
        req.setTransferTimeout(15000);
        auto* rr = net()->get(req);
        connect(rr, &QNetworkReply::finished, this, [this, rr] {
            rr->deleteLater();
            const QString txt = QString::fromUtf8(rr->readAll()).trimmed();
            const auto m = QRegularExpression("([0-9a-fA-F]{64})").match(txt);
            if (rr->error() == QNetworkReply::NoError && m.hasMatch()) m_sha = m.captured(1).toLower();
            finishAvailable();
        });
        return;
    }
    finishAvailable();
}

void Updater::finishAvailable() {
    if (m_sha.size() != 64) {
        // Without a published checksum we refuse to run anything automatically.
        m_assetUrl = QUrl();
        setState(Available, {}, tr("Version %1 is available. Open the release page to download it.").arg(m_latest));
    } else {
        setState(Available, {}, tr("Version %1 is available.").arg(m_latest));
    }
    if (m_manual) return;
#ifdef Q_OS_WIN
    if (mode() == Automatic && m_assetUrl.isValid()) { download(true); return; }    // background download; the prompt comes when it is verified
#endif
    emit updateFound(m_latest);
}

void Updater::download(bool automatic) {
    if (m_state != Available) return;
    m_auto = automatic;
#ifndef Q_OS_WIN
    if (!automatic) QDesktopServices::openUrl(m_page);   // macOS / Linux: open the release page; the user installs it themselves
    return;
#endif
    if (!m_assetUrl.isValid() || !urlAllowed(m_assetUrl)) { QDesktopServices::openUrl(m_page); return; }
    QDir().mkpath(Paths::tmpDir() + "/update");
    m_file = stagingDir() + "/" + QFileInfo(m_assetName).fileName();          // a file name only, never a path from the network
    delete m_out;
    m_out = new QFile(m_file + ".part", this);
    if (!m_out->open(QIODevice::WriteOnly | QIODevice::Truncate)) { setState(Failed, tr("Couldn't write the update to disk."), {}); return; }
    m_received = 0; m_total = 0;
    setState(Downloading, {}, tr("Downloading version %1…").arg(m_latest));
    QNetworkRequest req(m_assetUrl);
    req.setRawHeader("User-Agent", QByteArray("kinoz/") + APP_VERSION);
    req.setTransferTimeout(30000);
    m_reply = net()->get(req);
    connect(m_reply, &QNetworkReply::readyRead, this, [this] { if (m_reply && m_out) m_out->write(m_reply->readAll()); });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 tot) {
        m_received = got; m_total = tot; emit progress(got, tot);
        if ((tot > 600ll * 1024 * 1024 || got > 600ll * 1024 * 1024) && m_reply) m_reply->abort();   // sanity cap: an installer is never this big
    });
    connect(m_reply, &QNetworkReply::finished, this, &Updater::onDownloaded);
}

void Updater::onDownloaded() {
    QNetworkReply* r = m_reply;
    m_reply = nullptr;
    if (!r) return;
    r->deleteLater();
    if (m_out) { m_out->write(r->readAll()); m_out->close(); }
    if (r->error() != QNetworkReply::NoError) { QFile::remove(m_file + ".part"); setState(Failed, tr("The download was interrupted. Try again."), {}); return; }
    QFile f(m_file + ".part");
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!f.open(QIODevice::ReadOnly) || !h.addData(&f)) { setState(Failed, tr("Couldn't verify the download."), {}); return; }
    f.close();
    if (QString::fromLatin1(h.result().toHex()).toLower() != m_sha) {
        QFile::remove(m_file + ".part");
        setState(Failed, tr("The download didn't match its published checksum, so it was discarded."), {});
        return;
    }
    QFile::remove(m_file);
    if (!QFile::rename(m_file + ".part", m_file)) { setState(Failed, tr("Couldn't save the update."), {}); return; }
    stage();                                                         // survives a restart: installed at the next start if the user says "Later"
    setState(Ready, {}, tr("Version %1 is downloaded and verified. It installs when you restart, or the next time you open kinoz.").arg(m_latest));
    emit readyToInstall(m_latest, m_auto);
}

bool Updater::installAndQuit() {
    if (m_state != Ready) return false;
#ifdef Q_OS_WIN
    if (!QProcess::startDetached(m_file, {QStringLiteral("/SILENT"), QStringLiteral("/SUPPRESSMSGBOXES"), QStringLiteral("/NORESTART"), QStringLiteral("/CLOSEAPPLICATIONS")})) {
        setState(Failed, tr("Couldn't start the installer."), {});
        return false;
    }
    QTimer::singleShot(400, qApp, &QCoreApplication::quit);
    return true;
#else
    return false;
#endif
}
