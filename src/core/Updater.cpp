// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/Updater.h"
#include "core/JsonStore.h"
#include "core/Log.h"
#include "core/Paths.h"
#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSslConfiguration>
#include <QStorageInfo>
#include <QSysInfo>
#include <QTimer>

static const char* kRepo = "p4inz-code/kinoz";

Updater::Updater(JsonStore* settings, QObject* parent) : QObject(parent), m_set(settings) {}

QNetworkAccessManager* Updater::net() {
    if (!m_net) {
        m_net = new QNetworkAccessManager(this);
        m_net->setRedirectPolicy(QNetworkRequest::NoLessSafeRedirectPolicy);     // https never falls back to http; guard() also keeps every hop on GitHub
        QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
        ssl.setProtocol(QSsl::TlsV1_2OrLater);
        QSslConfiguration::setDefaultConfiguration(ssl);
    }
    return m_net;
}

QString Updater::current() {
    static const QString fake = qEnvironmentVariable("KINOZ_FAKE_VERSION");        // for testing the update path against the real release
    return fake.isEmpty() ? QStringLiteral(APP_VERSION) : fake;
}

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

bool Updater::redirectAllowed(const QUrl& u) {
    const QString h = u.host().toLower();
    const bool ok = h == QLatin1String("github.com") || h == QLatin1String("api.github.com") || h.endsWith(QLatin1String(".github.com")) || h == QLatin1String("githubusercontent.com") || h.endsWith(QLatin1String(".githubusercontent.com"));
    return u.scheme() == QLatin1String("https") && ok && u.userInfo().isEmpty() && (u.port() == -1 || u.port() == 443);
}

// Every request the updater makes: TLS 1.2 or newer, and a redirect to anywhere but GitHub is cut off (the checksum is checked anyway; this keeps the request itself on GitHub).
void Updater::guard(QNetworkReply* r) {
    connect(r, &QNetworkReply::redirected, r, [r](const QUrl& to) {
        if (!redirectAllowed(to)) { Log::warn("updater", QStringLiteral("blocked a redirect to %1").arg(to.host())); r->abort(); }
    });
}

bool Updater::isSkipped(JsonStore* settings, const QString& version) { return !version.isEmpty() && settings->str("skippedVersion") == version; }

QString Updater::tagFromLocation(const QUrl& u) {
    static const QRegularExpression re(QStringLiteral("^/%1/releases/tag/(v?[0-9]+\\.[0-9]+\\.[0-9]+)$").arg(QRegularExpression::escape(QString::fromLatin1(kRepo))));
    const auto m = re.match(u.path());
    return u.scheme() == QLatin1String("https") && u.host() == QLatin1String("github.com") && m.hasMatch() ? m.captured(1) : QString();
}

bool Updater::shouldRetryDownload(int http, int networkError, int tries) {
    if (tries >= 3) return false;
    return (http == 0 && networkError != int(QNetworkReply::OperationCanceledError)) || http >= 500;
}

// The release file for this system: Windows setup, Apple Silicon or Intel dmg, x86_64 or ARM AppImage (by the CPU kinoz was built for).
QString Updater::assetNameFor(const QString& v) {
#ifdef Q_OS_WIN
    return QStringLiteral("kinoz-Setup-%1.exe").arg(v);
#elif defined(Q_OS_MACOS)
    return QStringLiteral("kinoz-%1-%2.dmg").arg(v, QSysInfo::buildCpuArchitecture() == QLatin1String("arm64") ? QStringLiteral("macos") : QStringLiteral("macos-intel"));
#else
    return QStringLiteral("kinoz-%1-%2.AppImage").arg(v, QSysInfo::buildCpuArchitecture() == QLatin1String("arm64") ? QStringLiteral("aarch64") : QStringLiteral("x86_64"));
#endif
}

#ifdef Q_OS_MACOS
static QString macBundlePath() {
    QDir d(QCoreApplication::applicationDirPath());                      // .../kinoz.app/Contents/MacOS
    if (!d.cdUp() || !d.cdUp()) return {};
    return d.absolutePath().endsWith(QLatin1String(".app")) ? d.absolutePath() : QString();
}
#endif

QString Updater::unsupportedReason() {
#ifdef Q_OS_WIN
    return {};
#elif defined(Q_OS_MACOS)
    const QString b = macBundlePath();
    if (b.isEmpty()) return QObject::tr("kinoz isn't running from an app bundle");
    if (!QFileInfo(QFileInfo(b).absolutePath()).isWritable()) return QObject::tr("the folder %1 is read-only for you").arg(QFileInfo(b).absolutePath());
    return {};
#else
    const QString f = qEnvironmentVariable("APPIMAGE");
    if (f.isEmpty() || !QFile::exists(f)) return QObject::tr("this copy of kinoz isn't running as an AppImage");
    if (!QFileInfo(f).isWritable() || !QFileInfo(QFileInfo(f).absolutePath()).isWritable()) return QObject::tr("the AppImage's folder is read-only for you");
    return {};
#endif
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
    Log::info("updater", QStringLiteral("mode set to %1").arg(m));
    if (m == Off) discardStaged();                                  // opting out also cancels a staged update
}

void Updater::checkAutoIfDue() {
    if (mode() == Off) return;
    const qint64 last = qint64(m_set->num("lastUpdateCheck", 0));
    if (QDateTime::currentSecsSinceEpoch() - last < 24 * 3600) return;
    checkNow(false);
}

void Updater::startBackground() {
    checkAutoIfDue();
    if (m_periodic) return;
    m_periodic = new QTimer(this);
    m_periodic->setInterval(6 * 3600 * 1000);
    connect(m_periodic, &QTimer::timeout, this, &Updater::checkAutoIfDue);        // checkAutoIfDue itself waits until a day has passed
    m_periodic->start();
}

void Updater::skipLatest() {
    if (m_state != Available || m_latest.isEmpty()) return;
    m_set->set("skippedVersion", m_latest);
    Log::info("updater", QStringLiteral("version %1 skipped by the user").arg(m_latest));
    setState(UpToDate, {}, tr("Version %1 skipped. A newer one will still be offered.").arg(m_latest));
}

void Updater::discardStaged() {
    QDir d(stagingDir());
    for (const QFileInfo& fi : d.entryInfoList(QDir::Files | QDir::NoDotAndDotDot)) QFile::remove(fi.absoluteFilePath());
}

// Remember a verified download so it can be installed at the next start if the user doesn't restart now.
void Updater::stage() {
    QJsonObject o;
    o["version"] = m_latest; o["file"] = m_file; o["sha"] = m_sha; o["name"] = m_assetName;
    QSaveFile f(stagingDir() + QStringLiteral("/pending.json"));
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(o).toJson(QJsonDocument::Compact)); f.commit(); }
    m_set->set("updateAttempts", 0);
}

// Hand over to the new version. Returns true when it was started; the caller then quits at once.
bool Updater::launchApply(const QString& file) {
#ifdef Q_OS_WIN
    return QProcess::startDetached(file, {QStringLiteral("/SILENT"), QStringLiteral("/SUPPRESSMSGBOXES"), QStringLiteral("/NORESTART"), QStringLiteral("/CLOSEAPPLICATIONS")});
#elif defined(Q_OS_MACOS)
    const QString bundle = macBundlePath();
    if (bundle.isEmpty()) return false;
    const QString script = stagingDir() + QStringLiteral("/apply-mac.sh");
    QFile s(script);
    if (!s.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    // Waits for kinoz to quit, mounts the disk image, swaps the bundle (keeping the old one until the new one is in place), reopens kinoz.
    s.write("#!/bin/sh\n"
            "PID=\"$1\"; DMG=\"$2\"; APP=\"$3\"; LOG=\"$4\"\n"
            "exec >>\"$LOG\" 2>&1\n"
            "echo \"update helper started\"\n"
            "i=0; while kill -0 \"$PID\" 2>/dev/null && [ $i -lt 100 ]; do sleep 0.3; i=$((i+1)); done\n"
            "MNT=$(mktemp -d /tmp/kinoz-update.XXXXXX) || exit 1\n"
            "hdiutil attach -nobrowse -readonly -mountpoint \"$MNT\" \"$DMG\" || { echo \"attach failed\"; open \"$APP\"; exit 1; }\n"
            "SRC=$(ls -d \"$MNT\"/*.app 2>/dev/null | head -1)\n"
            "if [ -n \"$SRC\" ] && rm -rf \"$APP.new\" && ditto \"$SRC\" \"$APP.new\"; then\n"
            "  rm -rf \"$APP.old\"; mv \"$APP\" \"$APP.old\" && mv \"$APP.new\" \"$APP\" && rm -rf \"$APP.old\" && echo \"replaced\" || { echo \"swap failed\"; [ -d \"$APP.old\" ] && [ ! -d \"$APP\" ] && mv \"$APP.old\" \"$APP\"; }\n"
            "else echo \"copy failed\"; fi\n"
            "hdiutil detach \"$MNT\" -quiet; rmdir \"$MNT\" 2>/dev/null\n"
            "open \"$APP\"\n");
    s.close();
    QFile::setPermissions(script, QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner);
    return QProcess::startDetached(QStringLiteral("/bin/sh"), {script, QString::number(QCoreApplication::applicationPid()), file, bundle, Paths::logsDir() + QStringLiteral("/update-helper.log")});
#else
    const QString target = qEnvironmentVariable("APPIMAGE");
    if (target.isEmpty()) return false;
    const QString fresh = target + QStringLiteral(".new"), prev = target + QStringLiteral(".previous");
    QFile::remove(fresh);
    if (!QFile::copy(file, fresh)) { Log::error("updater", QStringLiteral("couldn't copy the update next to the AppImage")); return false; }
    QFile::setPermissions(fresh, QFile::permissions(fresh) | QFileDevice::ExeOwner | QFileDevice::ExeGroup | QFileDevice::ExeOther);
    QFile::remove(prev);
    if (!QFile::rename(target, prev)) { QFile::remove(fresh); Log::error("updater", QStringLiteral("couldn't move the old AppImage aside")); return false; }
    if (!QFile::rename(fresh, target)) { QFile::rename(prev, target); QFile::remove(fresh); Log::error("updater", QStringLiteral("couldn't put the new AppImage in place; old one restored")); return false; }
    return QProcess::startDetached(target, {});
#endif
}

bool Updater::applyPendingOnStartup(JsonStore* settings, bool dryRun) {
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
        Log::warn("updater", QStringLiteral("gave up applying %1 after two attempts").arg(version));
        return false;
    }
    // The file must be inside the staging folder and still match the checksum it was verified with.
    const QFileInfo fi(file);
    QFile f(file);
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (fi.absolutePath() != QFileInfo(stagingDir()).absoluteFilePath() || sha.size() != 64 || !f.open(QIODevice::ReadOnly) || !h.addData(&f)
        || QString::fromLatin1(h.result().toHex()).toLower() != sha) {
        Log::warn("updater", QStringLiteral("staged update for %1 failed its checksum or is misplaced; discarded").arg(version));
        discardStaged();
        return false;
    }
    f.close();
    if (!dryRun && !unsupportedReason().isEmpty()) { Log::warn("updater", QStringLiteral("can't apply %1 here: %2").arg(version, unsupportedReason())); discardStaged(); return false; }
    settings->set("updateAttempts", attempts + 1);
    settings->flush();                                                // the counter must survive the restart the update causes
    Log::info("updater", QStringLiteral("applying %1 at start (attempt %2)").arg(version).arg(attempts + 1));
    if (dryRun) return true;
    return launchApply(file);
}

void Updater::checkNow(bool manual) {
    if (m_state == Checking || m_state == Downloading) return;
    if (m_state == Ready) return;                      // a verified update is already waiting: don't check (or download) again
    m_manual = manual;
    Log::info("updater", QStringLiteral("check started (%1)").arg(manual ? "manual" : "automatic"));
    setState(Checking, {}, tr("Checking…"));
    QNetworkRequest req(QUrl(QStringLiteral("https://api.github.com/repos/%1/releases/latest").arg(QString::fromLatin1(kRepo))));
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setRawHeader("User-Agent", QByteArray("kinoz/") + APP_VERSION);
    req.setTransferTimeout(15000);
    m_reply = net()->get(req);
    guard(m_reply);
    connect(m_reply, &QNetworkReply::finished, this, [this, r = m_reply] { onChecked(r); });
}

void Updater::checkFailed(const QString& why) {
    Log::warn("updater", QStringLiteral("check failed: %1 (failure %2)").arg(why).arg(m_checkFails + 1));
    m_set->set("updateLast", QStringLiteral("check failed: ") + why);
    m_set->set("updateLastAt", double(QDateTime::currentSecsSinceEpoch()));
    setState(Failed, tr("Couldn't reach the update server. Check your connection and try again."), {});
    // A failed check must not count as "checked today": try again in an hour, then every six hours.
    if (!m_manual && mode() != Off) {
        if (!m_retry) { m_retry = new QTimer(this); m_retry->setSingleShot(true); connect(m_retry, &QTimer::timeout, this, [this] { if (mode() != Off) checkNow(false); }); }
        m_retry->start(m_checkFails < 1 ? 3600 * 1000 : 6 * 3600 * 1000);
        ++m_checkFails;
    }
}

// GitHub's API allows few requests per hour per address (shared networks hit it). The plain releases page has no such limit:
// it redirects to the tag of the newest release, and the file names follow from that tag.
void Updater::checkViaRedirect() {
    Log::info("updater", QStringLiteral("API refused (rate limit); reading the releases page instead"));
    QNetworkRequest req(QUrl(QStringLiteral("https://github.com/%1/releases/latest").arg(QString::fromLatin1(kRepo))));
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    req.setRawHeader("User-Agent", QByteArray("kinoz/") + APP_VERSION);
    req.setTransferTimeout(15000);
    m_reply = net()->get(req);
    connect(m_reply, &QNetworkReply::finished, this, [this, r = m_reply] {
        r->deleteLater();
        m_reply = nullptr;
        const QUrl to = r->url().resolved(r->header(QNetworkRequest::LocationHeader).toUrl());
        const QString rawTag = tagFromLocation(to);
        if (rawTag.isEmpty()) { checkFailed(QStringLiteral("releases page gave no tag (HTTP %1)").arg(r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt())); return; }
        m_set->set("lastUpdateCheck", double(QDateTime::currentSecsSinceEpoch()));
        m_checkFails = 0;
        QString tag = rawTag;
        if (tag.startsWith('v', Qt::CaseInsensitive)) tag.remove(0, 1);
        m_set->set("updateLastAt", double(QDateTime::currentSecsSinceEpoch()));
        if (!isNewer(tag, current()) || (!m_manual && isSkipped(m_set, tag))) {
            m_set->set("updateLast", QStringLiteral("up to date (via releases page)"));
            setState(UpToDate, {}, tr("You're up to date (version %1).").arg(current()));
            return;
        }
        m_set->set("updateLast", QStringLiteral("found %1 (via releases page)").arg(tag));
        m_latest = tag; m_notes.clear(); m_page = to; m_sha.clear(); m_assetSize = 0;
        m_assetName = assetNameFor(tag);
        const QString base = QStringLiteral("https://github.com/%1/releases/download/%2/%3").arg(QString::fromLatin1(kRepo), rawTag, m_assetName);
        m_assetUrl = QUrl(base);
        afterAssetChosen(base + QStringLiteral(".sha256"));
    });
}

void Updater::onChecked(QNetworkReply* r) {
    r->deleteLater();
    m_reply = nullptr;
    const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    Log::info("updater", QStringLiteral("check answered: HTTP %1, network error %2").arg(http).arg(int(r->error())));
    if (http == 403 || http == 429) { checkViaRedirect(); return; }                  // rate limited: use the page without a limit
    if (http == 404) {
        m_set->set("lastUpdateCheck", double(QDateTime::currentSecsSinceEpoch()));
        m_set->set("updateLast", QStringLiteral("no releases published")); m_set->set("updateLastAt", double(QDateTime::currentSecsSinceEpoch()));
        setState(UpToDate, {}, tr("You're up to date (no releases published yet)."));
        return;
    }
    if (r->error() != QNetworkReply::NoError) { checkFailed(QStringLiteral("network error %1").arg(int(r->error()))); return; }
    const QJsonObject o = QJsonDocument::fromJson(r->readAll()).object();
    QString tag = o.value("tag_name").toString();
    if (tag.isEmpty()) { checkFailed(QStringLiteral("unexpected answer")); return; }
    m_set->set("lastUpdateCheck", double(QDateTime::currentSecsSinceEpoch()));        // only a real answer counts as "checked"
    m_set->set("updateLastAt", double(QDateTime::currentSecsSinceEpoch()));
    m_checkFails = 0;
    if (tag.startsWith('v', Qt::CaseInsensitive)) tag.remove(0, 1);
    if (!isNewer(tag, current()) || (!m_manual && isSkipped(m_set, tag))) {
        m_set->set("updateLast", QStringLiteral("up to date (latest %1)").arg(tag));
        Log::info("updater", QStringLiteral("no update (latest %1, running %2%3)").arg(tag, current(), isSkipped(m_set, tag) ? ", skipped by the user" : ""));
        setState(UpToDate, {}, tr("You're up to date (version %1).").arg(current()));
        return;
    }
    m_set->set("updateLast", QStringLiteral("found %1").arg(tag));
    m_latest = tag;
    m_notes = o.value("body").toString();
    m_page = QUrl(o.value("html_url").toString());
    m_assetUrl = QUrl(); m_assetName.clear(); m_sha.clear(); m_assetSize = 0;
    const QString want = assetNameFor(tag);                  // exactly the file for this system and version (Intel and ARM builds have their own names)
    QString shaUrl;
    for (const auto& v : o.value("assets").toArray()) {
        const QJsonObject a = v.toObject();
        const QString name = a.value("name").toString();
        if (name == want) {
            m_assetName = name; m_assetUrl = QUrl(a.value("browser_download_url").toString());
            m_assetSize = qint64(a.value("size").toDouble());
            const QString dg = a.value("digest").toString();
            if (dg.startsWith("sha256:")) m_sha = dg.mid(7).toLower();
        }
    }
    for (const auto& v : o.value("assets").toArray()) {
        const QJsonObject a = v.toObject();
        if (!m_assetName.isEmpty() && a.value("name").toString() == m_assetName + ".sha256") shaUrl = a.value("browser_download_url").toString();
    }
    afterAssetChosen(shaUrl);
}

void Updater::afterAssetChosen(const QString& shaUrl) {
    if (m_assetName.isEmpty() || !urlAllowed(m_assetUrl)) {
        Log::warn("updater", QStringLiteral("version %1 has no build for this system").arg(m_latest));
        m_assetUrl = QUrl();
        setState(Available, {}, tr("Version %1 is out, but there is no build for this system yet.").arg(m_latest));
        return;
    }
    const QString why = unsupportedReason();
    if (!why.isEmpty()) {
        Log::warn("updater", QStringLiteral("version %1 found but this copy can't update itself: %2").arg(m_latest, why));
        m_assetUrl = QUrl();
        setState(Available, {}, tr("Version %1 is out. This copy of kinoz can't update itself: %2.").arg(m_latest, why));
        return;
    }
    if (m_sha.isEmpty() && !shaUrl.isEmpty() && urlAllowed(QUrl(shaUrl))) {
        QNetworkRequest req{QUrl(shaUrl)};
        req.setRawHeader("User-Agent", QByteArray("kinoz/") + APP_VERSION);
        req.setTransferTimeout(15000);
        auto* rr = net()->get(req);
        guard(rr);
        connect(rr, &QNetworkReply::finished, this, [this, rr] {
            rr->deleteLater();
            const QString txt = QString::fromUtf8(rr->readAll()).trimmed();
            const auto m = QRegularExpression("([0-9a-fA-F]{64})").match(txt);
            if (rr->error() == QNetworkReply::NoError && m.hasMatch()) m_sha = m.captured(1).toLower();
            else Log::warn("updater", QStringLiteral("checksum file not available (network error %1)").arg(int(rr->error())));
            finishAvailable();
        });
        return;
    }
    finishAvailable();
}

void Updater::finishAvailable() {
    if (m_sha.size() != 64) {
        // Without a published checksum we refuse to run anything: say so, in the app, and let the next check try again.
        Log::warn("updater", QStringLiteral("no checksum for %1; refusing to install").arg(m_latest));
        m_assetUrl = QUrl();
        m_set->set("lastUpdateCheck", 0.0);
        setState(Failed, tr("Version %1 is out, but its checksum couldn't be fetched, so kinoz won't install it yet. Try again in a moment.").arg(m_latest), {});
        return;
    }
    Log::info("updater", QStringLiteral("version %1 is available (%2, %3 bytes)").arg(m_latest, m_assetName).arg(m_assetSize));
    setState(Available, {}, tr("Version %1 is available.").arg(m_latest));
    if (m_manual) return;
    if (mode() == Automatic && m_assetUrl.isValid()) { download(true); return; }    // background download; the prompt comes when it is verified
    emit updateFound(m_latest);
}

void Updater::download(bool automatic) {
    if (m_state != Available) return;
    m_auto = automatic;
    if (!m_assetUrl.isValid() || !urlAllowed(m_assetUrl)) { setState(Failed, tr("There is no update file for this system."), {}); return; }
    m_dlTries = 0; m_noResume = false;
    if (!m_dlRetry) { m_dlRetry = new QTimer(this); m_dlRetry->setSingleShot(true); connect(m_dlRetry, &QTimer::timeout, this, &Updater::startDownload); }
    startDownload();
}

void Updater::failDownload(const QString& why) {
    Log::warn("updater", QStringLiteral("download failed: %1").arg(why));
    m_set->set("updateLast", QStringLiteral("download failed: ") + why);
    m_set->set("lastUpdateCheck", 0.0);                                  // the next due-check (a few hours at most) tries again
    setState(Failed, why, {});
}

// Starts (or, after a dropped connection, continues) the download. A partial file is kept and continued with an HTTP Range request.
void Updater::startDownload() {
    if (!m_assetUrl.isValid()) return;
    QDir().mkpath(stagingDir());
    m_file = stagingDir() + "/" + QFileInfo(m_assetName).fileName();          // a file name only, never a path from the network
    if (m_assetSize > 0) {                                                    // the update plus room to keep a copy while it is checked
        const QStorageInfo si(stagingDir());
        const qint64 need = m_assetSize * 2 + 50ll * 1024 * 1024;
        if (si.isValid() && si.bytesAvailable() > 0 && si.bytesAvailable() < need) {
            failDownload(tr("Not enough free disk space for the update (about %1 MB needed).").arg(need / (1024 * 1024)));
            return;
        }
    }
    const QString part = m_file + ".part";
    m_resumeFrom = 0;
    if (!m_noResume) {
        const qint64 have = QFileInfo(part).size();
        if (have > 0 && have < 600ll * 1024 * 1024 && (m_assetSize <= 0 || have < m_assetSize)) m_resumeFrom = have;
    }
    delete m_out;
    m_out = new QFile(part, this);
    if (!m_out->open(m_resumeFrom > 0 ? (QIODevice::WriteOnly | QIODevice::Append) : (QIODevice::WriteOnly | QIODevice::Truncate))) { failDownload(tr("Couldn't write the update to disk.")); return; }
    m_received = m_resumeFrom; m_total = 0; m_statusChecked = false;
    Log::info("updater", QStringLiteral("downloading %1 (resume from %2, try %3)").arg(m_assetName).arg(m_resumeFrom).arg(m_dlTries + 1));
    setState(Downloading, {}, tr("Downloading version %1…").arg(m_latest));
    QNetworkRequest req(m_assetUrl);
    req.setRawHeader("User-Agent", QByteArray("kinoz/") + APP_VERSION);
    if (m_resumeFrom > 0) req.setRawHeader("Range", "bytes=" + QByteArray::number(m_resumeFrom) + "-");
    req.setTransferTimeout(30000);
    m_reply = net()->get(req);
    guard(m_reply);
    connect(m_reply, &QNetworkReply::readyRead, this, [this, r = m_reply] {
        if (!m_reply || !m_out) return;
        if (!m_statusChecked) {
            m_statusChecked = true;
            if (m_resumeFrom > 0 && r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) { m_out->resize(0); m_resumeFrom = 0; }   // the server ignored Range: it sends everything again
        }
        m_out->write(m_reply->readAll());
    });
    connect(m_reply, &QNetworkReply::downloadProgress, this, [this](qint64 got, qint64 tot) {
        m_received = got + m_resumeFrom; m_total = tot > 0 ? tot + m_resumeFrom : 0; emit progress(m_received, m_total);
        if ((m_total > 600ll * 1024 * 1024 || m_received > 600ll * 1024 * 1024) && m_reply) m_reply->abort();   // sanity cap: an installer is never this big
    });
    connect(m_reply, &QNetworkReply::finished, this, &Updater::onDownloaded);
}

void Updater::onDownloaded() {
    QNetworkReply* r = m_reply;
    m_reply = nullptr;
    if (!r) return;
    r->deleteLater();
    if (m_out) { m_out->write(r->readAll()); m_out->close(); }
    const int http = r->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const QString part = m_file + ".part";
    if (r->error() != QNetworkReply::NoError) {
        Log::warn("updater", QStringLiteral("download error: HTTP %1, network error %2").arg(http).arg(int(r->error())));
        if (http == 416) { QFile::remove(part); m_noResume = true; startDownload(); return; }       // the partial file doesn't fit the server's file: start over
        if (shouldRetryDownload(http, int(r->error()), m_dlTries)) {                                   // dropped connection: keep what we have and continue
            static const int wait[] = {5000, 30000, 120000};
            setState(Downloading, {}, tr("Connection lost. Trying again…"));
            m_dlRetry->start(wait[qBound(0, m_dlTries, 2)]);
            ++m_dlTries;
            return;
        }
        if (http == 0 && r->error() != QNetworkReply::OperationCanceledError) { failDownload(tr("The download was interrupted. Try again; it continues where it stopped.")); return; }   // keep the partial file
        QFile::remove(part);
        failDownload(tr("The download failed. Try again."));
        return;
    }
    QFile f(part);
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!f.open(QIODevice::ReadOnly) || !h.addData(&f)) { failDownload(tr("Couldn't verify the download.")); return; }
    f.close();
    if (QString::fromLatin1(h.result().toHex()).toLower() != m_sha) {
        QFile::remove(part);
        if (m_resumeFrom > 0 && !m_noResume) { Log::warn("updater", QStringLiteral("checksum mismatch after resuming; downloading cleanly once")); m_noResume = true; startDownload(); return; }   // a continued file may be wrong: one clean download before giving up
        failDownload(tr("The download didn't match its published checksum, so it was discarded."));
        return;
    }
    QFile::remove(m_file);
    if (!QFile::rename(part, m_file)) { failDownload(tr("Couldn't save the update.")); return; }
    stage();                                                         // survives a restart: applied at the next start if the user says "Later"
    Log::info("updater", QStringLiteral("version %1 downloaded and verified").arg(m_latest));
    m_set->set("updateLast", QStringLiteral("%1 downloaded and verified").arg(m_latest));
    setState(Ready, {}, tr("Version %1 is downloaded and verified: SHA-256 %2…%3 matches the one published with the release. Restart to finish; kinoz brings you back to where you were.").arg(m_latest, m_sha.left(6), m_sha.right(6)));
    emit readyToInstall(m_latest, m_auto);
}

bool Updater::installAndQuit() {
    if (m_state != Ready) return false;
    Log::info("updater", QStringLiteral("restart requested to apply %1").arg(m_latest));
    emit aboutToRestart(m_latest);                                   // synchronous: the app writes where you were before the process goes away
    if (!launchApply(m_file)) {
        Log::error("updater", QStringLiteral("couldn't start applying %1").arg(m_latest));
        setState(Failed, tr("Couldn't start the update. Try again."), {});
        return false;
    }
    QTimer::singleShot(400, qApp, &QCoreApplication::quit);
    return true;
}
