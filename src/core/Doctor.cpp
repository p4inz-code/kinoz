// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/Doctor.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Updater.h"
#include <QDateTime>
#include <QSet>
#include <functional>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QSaveFile>
#include <QSslSocket>
#include <QStorageInfo>
#include <QTcpSocket>
#include <QTextStream>
#include <mpv/client.h>
#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace Doctor {
namespace {
bool validJson(const QString& path, bool* exists = nullptr) {
    QFile f(path);
    if (exists) *exists = f.exists();
    if (!f.open(QIODevice::ReadOnly)) return false;
    return QJsonDocument::fromJson(f.readAll()).isObject();
}
qint64 dirSize(const QString& dir, const QStringList& filters = {}) {
    qint64 n = 0;
    QDirIterator it(dir, filters, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); n += it.fileInfo().size(); }
    return n;
}
QString sizeText(qint64 b) { return QLocale().formattedDataSize(b, 1); }

// Starts the engine without a window or sound, reads its version, shuts it down. Proves libmpv loads and initialises.
Check engineCheck() {
    Check c; c.name = QObject::tr("Playback engine");
    mpv_handle* h = mpv_create();
    if (!h) { c.state = 2; c.detail = QObject::tr("couldn't be created"); return c; }
    mpv_set_option_string(h, "vo", "null"); mpv_set_option_string(h, "ao", "null"); mpv_set_option_string(h, "idle", "yes");
    mpv_set_option_string(h, "config", "no"); mpv_set_option_string(h, "terminal", "no");
    const int rc = mpv_initialize(h);
    if (rc < 0) { c.state = 2; c.detail = QObject::tr("failed to start: %1").arg(QString::fromUtf8(mpv_error_string(rc))); mpv_terminate_destroy(h); return c; }
    auto prop = [h](const char* n) { char* s = mpv_get_property_string(h, n); const QString v = s ? QString::fromUtf8(s) : QString(); if (s) mpv_free(s); return v; };
    c.detail = QObject::tr("libmpv %1 starts fine").arg(prop("mpv-version").remove(QStringLiteral("mpv ")).section(' ', 0, 0));
    mpv_terminate_destroy(h);
    return c;
}
}

QVector<Check> runCore(const QString& dataDir, bool network, bool engine, bool basics) {
    QVector<Check> out;
    auto add = [&](const QString& n, const QString& d, int st, bool fixable = false, const QString& fix = {}) { out.push_back({n, d, st, fixable, fix}); };
    if (engine) { Check e = engineCheck(); out.push_back(e); }
    if (basics) {
        QDir().mkpath(dataDir);
        QFile probe(dataDir + "/.doctor");
        const bool writable = probe.open(QIODevice::WriteOnly); probe.close(); probe.remove();
        add(QObject::tr("Data folder"), writable ? QDir::toNativeSeparators(dataDir) : QObject::tr("can't write here (%1)").arg(QDir::toNativeSeparators(dataDir)), writable ? 0 : 2);
        const qint64 freeB = QStorageInfo(dataDir).bytesAvailable();
        add(QObject::tr("Disk space"), sizeText(freeB) + QObject::tr(" free"), freeB > 300ll * 1024 * 1024 ? 0 : 1);
    }
    // secure connections, which the update check needs
    {
        const bool ssl = QSslSocket::supportsSsl();
        add(QObject::tr("Secure connections"), ssl ? QObject::tr("available (%1)").arg(QSslSocket::activeBackend()) : QObject::tr("not available, so updates can't be checked"), ssl ? 0 : 1);
    }
    // the files kinoz keeps: damaged ones are restored from the copy made at the previous start, or set aside
    {
        int bad = 0, restorable = 0;
        QStringList names;
        for (const char* n : {"settings.json", "resume.json", "marks.json", "music.json"}) {
            const QString p = dataDir + "/" + QString::fromLatin1(n);
            bool exists = false;
            if (validJson(p, &exists) || !exists) continue;
            ++bad; names << QString::fromLatin1(n);
            if (validJson(p + ".bak")) ++restorable;
        }
        if (!bad) add(QObject::tr("Settings files"), QObject::tr("healthy"), 0);
        else add(QObject::tr("Settings files"), QObject::tr("%1 damaged (%2); %3 can be restored from a backup").arg(bad).arg(names.join(", ")).arg(restorable), 1, true, "restoreSettings");
    }
    // crash reports and the crash streak
    {
        QStringList reports;
        for (const QFileInfo& fi : QDir(dataDir + "/logs").entryInfoList({"crash.*", "*.abort.txt", "*.terminate.txt"}, QDir::Files)) if (fi.size() > 0) reports << fi.fileName();
        double streak = 0;
        QFile sf(dataDir + "/settings.json");
        if (sf.open(QIODevice::ReadOnly)) streak = QJsonDocument::fromJson(sf.readAll()).object().value("crashStreak").toDouble();
        if (reports.isEmpty() && streak < 1) add(QObject::tr("Crashes"), QObject::tr("none recorded"), 0);
        else add(QObject::tr("Crashes"), QObject::tr("%1 crash report(s) in the logs folder, %2 abnormal exit(s) in a row").arg(reports.size()).arg(int(streak)), streak >= 2 ? 2 : 1, true, "clearCrash");
    }
    // the updater: what happened last, and files an interrupted update may have left
    {
        QFile sf(dataDir + "/settings.json");
        QJsonObject s; if (sf.open(QIODevice::ReadOnly)) s = QJsonDocument::fromJson(sf.readAll()).object();
        const QString last = s.value("updateLast").toString();
        const qint64 at = qint64(s.value("updateLastAt").toDouble());
        const QString when = at > 0 ? QDateTime::fromSecsSinceEpoch(at).toString(QStringLiteral("yyyy-MM-dd HH:mm")) : QObject::tr("never");
        const bool failed = last.contains(QLatin1String("failed"));
        add(QObject::tr("Updates"), QObject::tr("version %1, last check %2%3").arg(QStringLiteral(APP_VERSION), when, last.isEmpty() ? QString() : QStringLiteral(": ") + last), failed ? 1 : 0);
        const QString stage = dataDir + "/tmp/update";
        const qint64 leftover = dirSize(stage);
        const bool pending = QFile::exists(stage + "/pending.json");
        const bool failedVer = s.contains("updateFailedVersion");
        if (leftover == 0 && !failedVer) add(QObject::tr("Update files"), QObject::tr("nothing left over"), 0);
        else add(QObject::tr("Update files"), pending ? QObject::tr("an update (%1) is downloaded and waits for the next start").arg(sizeText(leftover))
                                                      : failedVer ? QObject::tr("an update could not be installed; %1 left over").arg(sizeText(leftover)) : QObject::tr("%1 of unfinished download left over").arg(sizeText(leftover)),
                 pending ? 0 : 1, !pending, "clearUpdate");
    }
    // caches and temporary files
    {
        const qint64 thumbs = dirSize(dataDir + "/cache/thumbs"), logs = dirSize(dataDir + "/logs"), tmp = dirSize(dataDir + "/tmp") - dirSize(dataDir + "/tmp/update");
        add(QObject::tr("Thumbnail cache"), sizeText(thumbs), thumbs > 800ll * 1024 * 1024 ? 1 : 0, thumbs > 800ll * 1024 * 1024, "clearThumbs");
        add(QObject::tr("Logs"), sizeText(logs), logs > 50ll * 1024 * 1024 ? 1 : 0, logs > 50ll * 1024 * 1024, "clearLogs");
        if (tmp > 100ll * 1024 * 1024) add(QObject::tr("Temporary files"), sizeText(tmp), 1, true, "clearTemp");
    }
    if (network) {                                                           // only a connection test: nothing is sent
        QTcpSocket s;
        s.connectToHost(QStringLiteral("github.com"), 443);
        const bool ok = s.waitForConnected(4000);
        add(QObject::tr("Update server"), ok ? QObject::tr("github.com is reachable") : QObject::tr("github.com can't be reached; updates need it"), ok ? 0 : 1);
        s.abort();
    }
    return out;
}

void applyCoreFix(const QString& id, const QString& dataDir) {
    Log::info("doctor", QStringLiteral("fix: %1").arg(id));
    auto wipeFiles = [](const QString& dir, const QStringList& filters, const QString& keep = {}) {
        for (const QFileInfo& fi : QDir(dir).entryInfoList(filters, QDir::Files)) if (fi.fileName() != keep) QFile::remove(fi.absoluteFilePath());
    };
    auto editSettings = [&](const std::function<void(QJsonObject&)>& fn) {
        const QString p = dataDir + "/settings.json";
        QFile f(p);
        QJsonObject o;
        if (f.open(QIODevice::ReadOnly)) { o = QJsonDocument::fromJson(f.readAll()).object(); f.close(); }
        fn(o);
        QSaveFile out(p);
        if (out.open(QIODevice::WriteOnly)) { out.write(QJsonDocument(o).toJson(QJsonDocument::Compact)); out.commit(); }
    };
    if (id == "restoreSettings") {
        for (const char* n : {"settings.json", "resume.json", "marks.json", "music.json"}) {
            const QString p = dataDir + "/" + QString::fromLatin1(n);
            bool exists = false;
            if (validJson(p, &exists) || !exists) continue;
            QFile::remove(p + ".corrupt"); QFile::copy(p, p + ".corrupt");
            QFile::remove(p);
            if (validJson(p + ".bak")) QFile::copy(p + ".bak", p);
        }
    } else if (id == "clearCrash") {
        wipeFiles(dataDir + "/logs", {"crash.*", "*.abort.txt", "*.terminate.txt"});
        editSettings([](QJsonObject& o) { o.remove("crashStreak"); });
    } else if (id == "clearUpdate") {
        wipeFiles(dataDir + "/tmp/update", {"*"});
        editSettings([](QJsonObject& o) { o.remove("updateFailedVersion"); o.insert("updateAttempts", 0); });
    } else if (id == "clearThumbs") wipeFiles(dataDir + "/cache/thumbs", {"*.jpg"});
    else if (id == "clearLogs") wipeFiles(dataDir + "/logs", {"*"}, QStringLiteral("app.log"));
    else if (id == "clearTemp") {
        for (const QFileInfo& fi : QDir(dataDir + "/tmp").entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot))
            if (fi.fileName() != QLatin1String("update")) { if (fi.isDir()) QDir(fi.absoluteFilePath()).removeRecursively(); else QFile::remove(fi.absoluteFilePath()); }
    }
}

QString textReport(const QVector<Check>& checks) {
    QString r;
    int warn = 0, bad = 0;
    for (const Check& c : checks) {
        const char* mark = c.state == 0 ? "OK   " : c.state == 1 ? "WARN " : "FAIL ";
        r += QStringLiteral("%1%2: %3%4\n").arg(QLatin1String(mark), c.name, c.detail, c.fixable && c.state > 0 ? QStringLiteral("  [fixable]") : QString());
        warn += c.state == 1; bad += c.state == 2;
    }
    r += QStringLiteral("\n%1 checks, %2 warning(s), %3 problem(s)\n").arg(checks.size()).arg(warn).arg(bad);
    return r;
}

int runTextMode(const QStringList& args) {
#ifdef Q_OS_WIN
    if (AttachConsole(ATTACH_PARENT_PROCESS)) { FILE* f = nullptr; freopen_s(&f, "CONOUT$", "w", stdout); }
#endif
    const QString data = Paths::dataDir();
    QVector<Check> checks = runCore(data, true, true, true);
    QTextStream out(stdout);
    out << "kinoz " << APP_VERSION << " health check (" << QDir::toNativeSeparators(data) << ")\n\n" << textReport(checks);
    if (args.contains(QStringLiteral("--fix"))) {
        int fixed = 0;
        QSet<QString> done;
        for (const Check& c : checks) if (c.state > 0 && c.fixable && !done.contains(c.fix)) { applyCoreFix(c.fix, data); done.insert(c.fix); ++fixed; out << "fixed: " << c.name << "\n"; }
        if (!fixed) out << "nothing to fix\n";
    } else {
        for (const Check& c : checks) if (c.state > 0 && c.fixable) { out << "Run `kinoz --doctor --fix` (or --doctor-text --fix) to repair the fixable items.\n"; break; }
    }
    out.flush();
    QFile rep(data + "/logs/doctor.txt");
    if (rep.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) rep.write(textReport(checks).toUtf8());
    int bad = 0; for (const Check& c : checks) bad += c.state == 2;
    return bad ? 1 : 0;
}
}
