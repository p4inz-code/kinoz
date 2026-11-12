// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "installer/Installer.h"

#ifndef Q_OS_WIN
namespace Installer {
bool wanted(const QStringList&) { return false; }
int run(const QStringList&) { return 0; }
void registerDefaultApp(const QString&) {}
}
#else

#include "media/MediaKind.h"
#include "ui/Icons.h"
#include "ui/Overlays.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QScreen>
#include <QSet>
#include <QStandardPaths>
#include <QStorageInfo>
#include <QTextStream>
#include <QThread>
#include <QTime>
#include <QTimer>
#include <QUrl>
#include <QVariantAnimation>
#include <QWindow>
#include <cmath>
#include <functional>

#include <windows.h>
#include <shlobj.h>
#include <tlhelp32.h>

namespace {
const QString kApp = QStringLiteral("kinoz");
const QString kUninstallKey = QStringLiteral("Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\kinoz");
const QString kProgId = QStringLiteral("kinoz.Media");
const QString kVersion = QStringLiteral(APP_VERSION);

// ---------------------------------------------------------------- log (so a failed setup can always be explained)
QString logPath() { return QDir::tempPath() + "/kinoz-setup.log"; }
void logLine(const QString& s) {
    QFile f(logPath());
    if (f.open(QIODevice::Append | QIODevice::Text)) { QTextStream t(&f); t << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "  " << s << '\n'; }
}

// ---------------------------------------------------------------- registry (current user only)
std::wstring ws(const QString& s) { return s.toStdWString(); }
bool regSet(const QString& sub, const QString& name, const QString& value) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, ws(sub).c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    const std::wstring v = ws(value), n = ws(name);
    const LONG r = RegSetValueExW(k, name.isEmpty() ? nullptr : n.c_str(), 0, REG_SZ, reinterpret_cast<const BYTE*>(v.c_str()), DWORD((v.size() + 1) * sizeof(wchar_t)));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
bool regSetDword(const QString& sub, const QString& name, DWORD value) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, ws(sub).c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    const LONG r = RegSetValueExW(k, ws(name).c_str(), 0, REG_DWORD, reinterpret_cast<const BYTE*>(&value), sizeof(value));
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
bool regSetNone(const QString& sub, const QString& name) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, ws(sub).c_str(), 0, nullptr, 0, KEY_SET_VALUE, nullptr, &k, nullptr) != ERROR_SUCCESS) return false;
    const LONG r = RegSetValueExW(k, ws(name).c_str(), 0, REG_NONE, nullptr, 0);
    RegCloseKey(k);
    return r == ERROR_SUCCESS;
}
void regDeleteTree(const QString& sub) { RegDeleteTreeW(HKEY_CURRENT_USER, ws(sub).c_str()); }
void regDeleteValue(const QString& sub, const QString& name) {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, ws(sub).c_str(), 0, KEY_SET_VALUE, &k) != ERROR_SUCCESS) return;
    RegDeleteValueW(k, ws(name).c_str());
    RegCloseKey(k);
}
QString regGetString(const QString& sub, const QString& name) {
    wchar_t buf[1024]; DWORD sz = sizeof(buf);
    if (RegGetValueW(HKEY_CURRENT_USER, ws(sub).c_str(), ws(name).c_str(), RRF_RT_REG_SZ, nullptr, buf, &sz) != ERROR_SUCCESS) return {};
    return QString::fromWCharArray(buf);
}
bool regExists(const QString& sub) { HKEY k; if (RegOpenKeyExW(HKEY_CURRENT_USER, ws(sub).c_str(), 0, KEY_READ, &k) != ERROR_SUCCESS) return false; RegCloseKey(k); return true; }

QStringList mediaExtensions() {
    QStringList l;
    for (const auto* set : {&Media::videoExt(), &Media::audioExt(), &Media::imageExt(), &Media::gifExt()}) for (const QString& e : *set) l << e;
    l.removeDuplicates(); l.sort();
    return l;
}

QString installedDir() {
    const QString d = regGetString(kUninstallKey, QStringLiteral("InstallLocation"));
    return (!d.isEmpty() && QFile::exists(d + "/kinoz.exe")) ? QDir::toNativeSeparators(d) : QString();
}
QString installedVersion() { return regGetString(kUninstallKey, QStringLiteral("DisplayVersion")); }

bool versionNewer(const QString& a, const QString& b) {      // is a newer than b
    auto parts = [](const QString& v) { QList<int> l; for (const QString& p : v.split('.')) l << p.toInt(); while (l.size() < 3) l << 0; return l; };
    const auto x = parts(a), y = parts(b);
    for (int i = 0; i < 3; ++i) if (x[i] != y[i]) return x[i] > y[i];
    return false;
}

// Prefer a fixed drive other than C: (so nothing lands on the system drive); otherwise the normal per-user folder.
QString defaultInstallDir() {
    const QString prev = installedDir();
    if (!prev.isEmpty()) return prev;
    QString best; qint64 bestFree = 2ll * 1024 * 1024 * 1024;
    for (const QStorageInfo& v : QStorageInfo::mountedVolumes()) {
        if (!v.isValid() || !v.isReady() || v.isReadOnly()) continue;
        const QString root = v.rootPath();
        if (root.startsWith("C", Qt::CaseInsensitive)) continue;
        if (GetDriveTypeW(ws(QDir::toNativeSeparators(root)).c_str()) != DRIVE_FIXED) continue;
        if (v.bytesAvailable() > bestFree) { bestFree = v.bytesAvailable(); best = root; }
    }
    if (!best.isEmpty()) return QDir::toNativeSeparators(QDir::cleanPath(best + "/Programs/kinoz"));
    return QDir::toNativeSeparators(QDir::cleanPath(qEnvironmentVariable("LOCALAPPDATA") + "/Programs/kinoz"));
}

// Ask a running kinoz that lives in `dir` to close (gracefully first), so its files can be replaced.
void closeRunning(const QString& dir) {
    const QString exe = QDir::toNativeSeparators(QDir::cleanPath(dir + "/kinoz.exe")).toLower();
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32W pe{sizeof(pe)};
    QVector<DWORD> pids;
    if (Process32FirstW(snap, &pe)) do {
        if (_wcsicmp(pe.szExeFile, L"kinoz.exe") != 0 || pe.th32ProcessID == GetCurrentProcessId()) continue;
        HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
        if (!h) continue;
        wchar_t path[MAX_PATH]; DWORD n = MAX_PATH;
        if (QueryFullProcessImageNameW(h, 0, path, &n) && QString::fromWCharArray(path).toLower() == exe) pids << pe.th32ProcessID;
        CloseHandle(h);
    } while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    for (DWORD pid : pids) {
        struct Ctx { DWORD pid; } ctx{pid};
        EnumWindows([](HWND w, LPARAM l) -> BOOL { DWORD p = 0; GetWindowThreadProcessId(w, &p); if (p == reinterpret_cast<Ctx*>(l)->pid) PostMessageW(w, WM_CLOSE, 0, 0); return TRUE; }, reinterpret_cast<LPARAM>(&ctx));
        HANDLE h = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE, FALSE, pid);
        if (!h) continue;
        if (WaitForSingleObject(h, 6000) != WAIT_OBJECT_0) { logLine("kinoz did not close in time; ending it"); TerminateProcess(h, 0); WaitForSingleObject(h, 2000); }
        CloseHandle(h);
    }
}

struct Options {
    QString dest;
    bool rightClick = true, openWith = true, desktop = false, launch = true, silent = false;
    bool deleteData = false;           // uninstall only
};

using Progress = std::function<void(qint64 done, qint64 total, const QString& stage, const QString& file)>;

QString startMenuLink() { return QStandardPaths::writableLocation(QStandardPaths::ApplicationsLocation) + "/kinoz.lnk"; }
QString desktopLink() { return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation) + "/kinoz.lnk"; }

const QString kCapKey = QStringLiteral("Software\\kinoz\\Capabilities");

// Makes kinoz appear in Settings > Apps > Default apps (the only way Windows lets an app be chosen as a default).
void registerCapabilities(const QString& exe) {
    const QString cmd = QStringLiteral("\"%1\" \"%2\"").arg(QDir::toNativeSeparators(exe), QStringLiteral("%1"));
    const QString icon = QDir::toNativeSeparators(exe) + ",0";
    const QString classes = QStringLiteral("Software\\Classes\\");
    regSet(classes + kProgId, "", QStringLiteral("kinoz media file"));
    regSet(classes + kProgId + "\\DefaultIcon", "", icon);
    regSet(classes + kProgId + "\\shell\\open\\command", "", cmd);
    for (const QString& e : mediaExtensions()) regSetNone(classes + "." + e + "\\OpenWithProgids", kProgId);
    regSet(kCapKey, "ApplicationName", kApp);
    regSet(kCapKey, "ApplicationDescription", QStringLiteral("Fast, calm media player for video, audio, images and GIFs"));
    for (const QString& e : mediaExtensions()) regSet(kCapKey + "\\FileAssociations", "." + e, kProgId);
    regSet(QStringLiteral("Software\\RegisteredApplications"), kApp, kCapKey);
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void registerShell(const Options& o, const QString& exe) {
    const QString cmd = QStringLiteral("\"%1\" \"%2\"").arg(QDir::toNativeSeparators(exe), QStringLiteral("%1"));
    const QString icon = QDir::toNativeSeparators(exe) + ",0";
    const QString classes = QStringLiteral("Software\\Classes\\");
    regSet(classes + "Applications\\kinoz.exe", "FriendlyAppName", kApp);
    regSet(classes + "Applications\\kinoz.exe\\shell\\open\\command", "", cmd);
    if (o.openWith) registerCapabilities(exe);
    if (o.rightClick) {
        for (const QString& e : mediaExtensions()) {
            const QString k = classes + "SystemFileAssociations\\." + e + "\\shell\\kinoz";
            regSet(k, "MUIVerb", QStringLiteral("Play with kinoz")); regSet(k, "Icon", icon); regSet(k + "\\command", "", cmd);
        }
        const QString d = classes + "Directory\\shell\\kinoz";
        regSet(d, "MUIVerb", QStringLiteral("Open folder in kinoz")); regSet(d, "Icon", icon); regSet(d + "\\command", "", cmd);
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void unregisterShell() {
    const QString classes = QStringLiteral("Software\\Classes\\");
    regDeleteTree(classes + "Applications\\kinoz.exe");
    regDeleteTree(classes + kProgId);
    regDeleteTree(QStringLiteral("Software\\kinoz"));
    regDeleteValue(QStringLiteral("Software\\RegisteredApplications"), kApp);
    regDeleteTree(classes + "Directory\\shell\\kinoz");
    for (const QString& e : mediaExtensions()) {
        regDeleteValue(classes + "." + e + "\\OpenWithProgids", kProgId);
        regDeleteTree(classes + "SystemFileAssociations\\." + e + "\\shell\\kinoz");
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

void writeUninstallEntry(const QString& dest, const QString& version, qint64 totalBytes) {
    const QString exe = QDir::toNativeSeparators(dest + "/kinoz.exe");
    regSet(kUninstallKey, "DisplayName", kApp);
    regSet(kUninstallKey, "DisplayVersion", version);
    regSet(kUninstallKey, "Publisher", QStringLiteral("P4inz Interactive Labs"));
    regSet(kUninstallKey, "InstallLocation", QDir::toNativeSeparators(dest));
    regSet(kUninstallKey, "DisplayIcon", exe + ",0");
    regSet(kUninstallKey, "UninstallString", QStringLiteral("\"%1\" --uninstall").arg(exe));
    regSet(kUninstallKey, "QuietUninstallString", QStringLiteral("\"%1\" --uninstall --silent").arg(exe));
    regSet(kUninstallKey, "URLInfoAbout", QStringLiteral("https://github.com/p4inz-code/kinoz"));
    regSet(kUninstallKey, "HelpLink", QStringLiteral("https://github.com/p4inz-code/kinoz/issues"));
    if (totalBytes > 0) regSetDword(kUninstallKey, "EstimatedSize", DWORD(totalBytes / 1024));
    regSetDword(kUninstallKey, "NoModify", 1);
    regSetDword(kUninstallKey, "NoRepair", 1);
}

bool vcRuntimeInstalled() {
    DWORD installed = 0, sz = sizeof(installed);
    return RegGetValueW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\x64", L"Installed", RRF_RT_REG_DWORD, nullptr, &installed, &sz) == ERROR_SUCCESS && installed == 1;
}

// ---------------------------------------------------------------- manifest: every file has a SHA-256, written at build time
struct ManifestFile { QString path; qint64 size; QString sha; };
QVector<ManifestFile> readManifest(const QString& dir, QString* version = nullptr) {
    QVector<ManifestFile> v;
    QFile f(dir + "/manifest.json");
    if (!f.open(QIODevice::ReadOnly)) return v;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (version) *version = o.value("version").toString();
    for (const auto& e : o.value("files").toArray()) {
        const auto a = e.toObject();
        const QString p = a.value("p").toString();
        if (p.isEmpty() || p.startsWith('/') || p.startsWith('\\') || p.contains(':') || p.contains(QStringLiteral(".."))) continue;   // never write outside the install folder
        v.push_back({p, qint64(a.value("s").toDouble()), a.value("h").toString().toLower()});
    }
    return v;
}
QString fileSha256(const QString& path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash h(QCryptographicHash::Sha256);
    if (!h.addData(&f)) return {};
    return QString::fromLatin1(h.result().toHex());
}

// ---------------------------------------------------------------- the Doctor
struct Check { QString name, detail; int state = 0; bool fixable = false; QString fix; };   // state: 0 ok, 1 warning, 2 problem

QVector<Check> runDoctor(const QString& dest, const QString& payload) {
    QVector<Check> out;
    auto add = [&](const QString& n, const QString& d, int st, bool fixable = false, const QString& fix = {}) { out.push_back({n, d, st, fixable, fix}); };
    const QString exe = dest + "/kinoz.exe";
    if (dest.isEmpty() || !QFile::exists(exe)) { add(QObject::tr("Install folder"), QObject::tr("kinoz.exe not found"), 2, !payload.isEmpty(), "files"); return out; }
    add(QObject::tr("Install folder"), QDir::toNativeSeparators(dest), 0);

    const auto man = readManifest(dest);
    if (man.isEmpty()) add(QObject::tr("Program files"), QObject::tr("no file list to compare with"), 1, !payload.isEmpty(), "files");
    else {
        int missing = 0, changed = 0;
        for (const auto& f : man) {
            const QString p = dest + "/" + f.path;
            if (!QFile::exists(p)) { ++missing; continue; }
            if (QFileInfo(p).size() != f.size || fileSha256(p) != f.sha) ++changed;
        }
        if (!missing && !changed) add(QObject::tr("Program files"), QObject::tr("%1 files verified").arg(man.size()), 0);
        else add(QObject::tr("Program files"), QObject::tr("%1 missing, %2 changed").arg(missing).arg(changed), 2, !payload.isEmpty(), "files");
    }
    const bool vc = vcRuntimeInstalled();
    add(QObject::tr("Microsoft runtime"), vc ? QObject::tr("installed") : QObject::tr("missing"), vc ? 0 : 2, QFile::exists(payload + "/vc_redist.x64.exe"), "vc");

    const bool lnk = QFile::exists(startMenuLink());
    add(QObject::tr("Start Menu shortcut"), lnk ? QObject::tr("present") : QObject::tr("missing"), lnk ? 0 : 1, true, "shortcut");
    const bool entry = regGetString(kUninstallKey, "InstallLocation").compare(QDir::toNativeSeparators(dest), Qt::CaseInsensitive) == 0;
    add(QObject::tr("Settings → Apps entry"), entry ? QObject::tr("present") : QObject::tr("missing or pointing elsewhere"), entry ? 0 : 1, true, "entry");
    const QString verb = regGetString("Software\\Classes\\SystemFileAssociations\\.mp4\\shell\\kinoz\\command", "");
    const QString dverb = regGetString("Software\\Classes\\Directory\\shell\\kinoz\\command", "");
    const QString nd = QDir::toNativeSeparators(dest);
    const bool rc = verb.contains(nd, Qt::CaseInsensitive) && dverb.contains(nd, Qt::CaseInsensitive);
    add(QObject::tr("Right-click menu"), rc ? QObject::tr("“Play with kinoz” is set up") : QObject::tr("not set up (or old location)"), rc ? 0 : 1, true, "shell");
    const bool ow = regExists("Software\\Classes\\" + kProgId) && regExists(kCapKey);
    add(QObject::tr("“Open with” list"), ow ? QObject::tr("present") : QObject::tr("not set up"), ow ? 0 : 1, true, "shell");

    const QString data = dest + "/data";
    QDir().mkpath(data);
    QFile probe(data + "/.doctor");
    const bool writable = probe.open(QIODevice::WriteOnly); probe.close(); probe.remove();
    add(QObject::tr("Data folder"), writable ? QDir::toNativeSeparators(data) : QObject::tr("can't write here"), writable ? 0 : 2);
    int bad = 0;
    for (const char* n : {"settings.json", "resume.json"}) {
        QFile f(data + "/" + QString::fromLatin1(n));
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) continue;
        QJsonParseError pe; QJsonDocument::fromJson(f.readAll(), &pe);
        if (pe.error != QJsonParseError::NoError) ++bad;
    }
    add(QObject::tr("Settings files"), bad ? QObject::tr("%1 damaged (will be set aside)").arg(bad) : QObject::tr("healthy"), bad ? 1 : 0, bad > 0, "json");
    const qint64 freeB = QStorageInfo(dest).bytesAvailable();
    add(QObject::tr("Disk space"), QLocale().formattedDataSize(freeB) + QObject::tr(" free"), freeB > 300ll * 1024 * 1024 ? 0 : 1);
    return out;
}

QString copyOne(const QString& from, const QString& to) {
    QDir().mkpath(QFileInfo(to).absolutePath());
    QFile::remove(to);
    return QFile::copy(from, to) ? QString() : QObject::tr("Couldn't write %1").arg(QDir::toNativeSeparators(to));
}

void applyFixes(const QVector<Check>& checks, const QString& dest, const QString& payload) {
    QSet<QString> fixes;
    for (const auto& c : checks) if (c.state > 0 && c.fixable) fixes.insert(c.fix);
    logLine("doctor fixes: " + QStringList(fixes.begin(), fixes.end()).join(","));
    Options o;
    if (fixes.contains("files") && !payload.isEmpty()) {
        closeRunning(dest);
        for (const auto& f : readManifest(payload)) { const QString to = dest + "/" + f.path; if (!QFile::exists(to) || fileSha256(to) != f.sha) logLine("repair " + f.path + " " + copyOne(payload + "/" + f.path, to)); }
        QFile::remove(dest + "/manifest.json"); QFile::copy(payload + "/manifest.json", dest + "/manifest.json");
    }
    if (fixes.contains("vc")) QProcess::execute(payload + "/vc_redist.x64.exe", {"/install", "/quiet", "/norestart"});
    if (fixes.contains("shortcut")) { QFile::remove(startMenuLink()); QFile::link(dest + "/kinoz.exe", startMenuLink()); }
    if (fixes.contains("entry")) { QString v; readManifest(dest, &v); writeUninstallEntry(dest, v.isEmpty() ? kVersion : v, 0); }
    if (fixes.contains("shell")) registerShell(o, dest + "/kinoz.exe");
    if (fixes.contains("json")) for (const char* n : {"settings.json", "resume.json"}) {
        const QString p = dest + "/data/" + QString::fromLatin1(n);
        QFile f(p); if (!f.exists() || !f.open(QIODevice::ReadOnly)) continue;
        QJsonParseError pe; QJsonDocument::fromJson(f.readAll(), &pe); f.close();
        if (pe.error != QJsonParseError::NoError) { QFile::remove(p + ".corrupt"); QFile::rename(p, p + ".corrupt"); }
    }
}

// ---------------------------------------------------------------- install / uninstall jobs
bool safeDestination(const QString& dest, QString* why) {
    const QString d = QDir::toNativeSeparators(QDir::cleanPath(dest)).toLower();
    if (d.size() < 4 || QDir(dest).isRoot()) { *why = QObject::tr("Please choose a folder, not a whole drive."); return false; }
    for (const QString& bad : {qEnvironmentVariable("WINDIR"), qEnvironmentVariable("SystemRoot")})
        if (!bad.isEmpty() && d.startsWith(QDir::toNativeSeparators(bad).toLower())) { *why = QObject::tr("That folder belongs to Windows. Please choose another one."); return false; }
    return true;
}

QString installJob(const Options& o, const QString& payload, const Progress& progress) {
    const QString dest = QDir::cleanPath(o.dest);
    logLine("install to " + dest);
    QString why;
    if (!safeDestination(dest, &why)) return why;
    progress(0, 1, QObject::tr("Checking the setup files"), {});
    // 1. the setup files must be exactly what was built
    const auto man = readManifest(payload);
    qint64 total = 0;
    for (const auto& f : man) total += f.size;
    if (man.isEmpty()) return QObject::tr("The setup files are incomplete. Please download kinoz again.");
    qint64 checked = 0;
    for (const auto& f : man) {
        progress(checked, qMax<qint64>(total, 1), QObject::tr("Checking the setup files"), QFileInfo(f.path).fileName());
        if (fileSha256(payload + "/" + f.path) != f.sha) { logLine("payload mismatch: " + f.path); return QObject::tr("A setup file is damaged (%1). Please download kinoz again.").arg(f.path); }
        checked += f.size;
    }
    if (!QDir().mkpath(dest)) return QObject::tr("Couldn't create the folder %1.").arg(QDir::toNativeSeparators(dest));
    const qint64 freeB = QStorageInfo(dest).bytesAvailable();
    if (freeB > 0 && freeB < total + 100ll * 1024 * 1024) return QObject::tr("Not enough free space on that drive (%1 needed).").arg(QLocale().formattedDataSize(total + 100ll * 1024 * 1024));
    closeRunning(dest);

    // 2. copy, keeping the old files aside so a failure can be undone
    QStringList moved, created;
    auto rollback = [&] {
        for (const QString& c : created) QFile::remove(c);
        for (const QString& m : moved) { const QString orig = m.left(m.size() - 7); QFile::remove(orig); QFile::rename(m, orig); }
        logLine("rolled back");
    };
    qint64 done = 0;
    for (const auto& f : man) {
        progress(done, qMax<qint64>(total, 1), QObject::tr("Copying files"), QFileInfo(f.path).fileName());
        const QString to = dest + "/" + f.path;
        QDir().mkpath(QFileInfo(to).absolutePath());
        if (QFile::exists(to)) {
            QFile::remove(to + ".kz-old");
            if (QFile::rename(to, to + ".kz-old")) moved << to + ".kz-old";
            else { rollback(); return QObject::tr("Couldn't replace %1. Is kinoz still open?").arg(QDir::toNativeSeparators(to)); }
        }
        if (!QFile::copy(payload + "/" + f.path, to)) { rollback(); logLine("copy failed " + to); return QObject::tr("Couldn't write %1.").arg(QDir::toNativeSeparators(to)); }
        created << to;
        done += f.size;
    }
    QFile::remove(dest + "/manifest.json"); QFile::copy(payload + "/manifest.json", dest + "/manifest.json");
    for (const QString& m : moved) QFile::remove(m);
    progress(total, qMax<qint64>(total, 1), QObject::tr("Finishing up"), {});

    // 3. runtime, Windows entries, shortcuts
    const QString vc = payload + "/vc_redist.x64.exe";
    if (!vcRuntimeInstalled() && QFile::exists(vc)) { progress(total, total, QObject::tr("Installing the Microsoft runtime"), {}); QProcess::execute(vc, {"/install", "/quiet", "/norestart"}); }
    const QString exe = dest + "/kinoz.exe";
    registerShell(o, exe);
    writeUninstallEntry(dest, kVersion, total);
    QFile::remove(startMenuLink()); QFile::link(exe, startMenuLink());
    if (o.desktop) { QFile::remove(desktopLink()); QFile::link(exe, desktopLink()); }
    logLine("install ok");
    return {};
}

QString uninstallJob(const Options& o, const Progress& progress) {
    const QString dir = QDir::cleanPath(installedDir().isEmpty() ? QCoreApplication::applicationDirPath() : installedDir());
    logLine("uninstall " + dir + (o.deleteData ? " (with data)" : " (keep data)"));
    // Only ever remove a real kinoz install: it must have the exe and the setup's file list, and not be a system/root folder.
    QString why;
    if (!QFile::exists(dir + "/kinoz.exe") || !QFile::exists(dir + "/manifest.json") || !safeDestination(dir, &why)) {
        logLine("refused to remove " + dir + " (not a kinoz install)");
        return QObject::tr("That folder doesn't look like a kinoz install, so nothing was removed.");
    }
    QSet<QString> names;                                   // top-level entries that belong to kinoz (never anything else in the folder)
    for (const auto& mf : readManifest(dir)) names.insert(mf.path.section('/', 0, 0));
    names.insert(QStringLiteral("manifest.json"));
    if (o.deleteData) names.insert(QStringLiteral("data"));
    progress(0, 3, QObject::tr("Closing kinoz"), {});
    closeRunning(dir);
    progress(1, 3, QObject::tr("Removing shortcuts and menu entries"), {});
    unregisterShell();
    regDeleteTree(kUninstallKey);
    QFile::remove(startMenuLink());
    QFile::remove(desktopLink());
    progress(2, 3, QObject::tr("Removing files"), {});
    // This process may be running from that folder, so a tiny script removes it right after we exit.
    const QString nd = QDir::toNativeSeparators(dir);
    const QString bat = QDir::tempPath() + "/kinoz-uninstall.cmd";
    QFile f(bat);
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QTextStream s(&f);
        auto esc = [](QString p) { return p.replace('%', QStringLiteral("%%")); };
        s << "@echo off\r\n";
        for (int pass = 0; pass < 2; ++pass) {              // second pass catches files that were still locked a moment ago
            s << "ping -n 3 127.0.0.1 >nul\r\n";
            for (const QString& n : names) {
                const QString p = esc(nd + "\\" + QDir::toNativeSeparators(n));
                s << "if exist \"" << p << "\\\" (rmdir /s /q \"" << p << "\") else del /f /q \"" << p << "\" 2>nul\r\n";
            }
            s << "del /f /q \"" << esc(nd) << "\\*.kz-old\" 2>nul\r\n";
        }
        s << "rmdir \"" << esc(nd) << "\" 2>nul\r\n";       // removes the folder only if nothing else is left in it
        s << "del \"%~f0\"\r\n";
        f.close();
        QProcess::startDetached(QStringLiteral("cmd.exe"), {QStringLiteral("/c"), QDir::toNativeSeparators(bat)});
    }
    progress(3, 3, QObject::tr("Done"), {});
    return {};
}

// --------------------------------------------------------------------------------------------------------------
class SetupWindow : public QWidget {
public:
    enum Flow { Install, Uninstall };
    enum Page { Choose, Maintenance, Doctor, Remove, Working, Done, Failed };

    SetupWindow(const QString& payload, const QString& dest, bool launch) : m_payload(payload), m_launch(launch) {
        setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);
        setWindowTitle(QStringLiteral("kinoz setup"));
        resize(828, 528);
        m_opt.dest = dest;
        m_primary = new TextButton(tr("Install"), this, true);
        m_cancel = new TextButton(tr("Cancel"), this);
        m_change = new TextButton(tr("Change…"), this);
        m_open = new TextButton(tr("Open kinoz"), this, true);
        m_close = new TextButton(tr("Close"), this);
        m_retry = new TextButton(tr("Try again"), this, true);
        m_fix = new TextButton(tr("Fix problems"), this, true);
        m_back = new TextButton(tr("Back"), this);
        m_default = new TextButton(tr("Make kinoz my default player…"), this);
        connect(m_default, &QAbstractButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:defaultapps?registeredAppUser=kinoz"))); });
        for (auto* s : {&m_sw1, &m_sw2, &m_sw3}) *s = new Switch(this);
        m_sw1->setChecked(true); m_sw2->setChecked(true); m_sw3->setChecked(false);
        m_x = new IconButton(Icon::Close, this, 32);
        connect(m_x, &QAbstractButton::clicked, this, [this] { close(); });
        connect(m_cancel, &QAbstractButton::clicked, this, [this] { if (m_page == Remove && hasInstall() && !m_payload.isEmpty()) { m_flow = Install; setPage(Maintenance); } else close(); });
        connect(m_close, &QAbstractButton::clicked, this, [this] { close(); });
        connect(m_back, &QAbstractButton::clicked, this, [this] { setPage(Maintenance); });
        connect(m_primary, &QAbstractButton::clicked, this, [this] { start(); });
        connect(m_retry, &QAbstractButton::clicked, this, [this] { m_flow = Install; setPage(hasInstall() ? Maintenance : Choose); });
        connect(m_open, &QAbstractButton::clicked, this, [this] { QProcess::startDetached(QDir::cleanPath(m_opt.dest + "/kinoz.exe"), {}); close(); });
        connect(m_change, &QAbstractButton::clicked, this, [this] { chooseFolder(); });
        connect(m_fix, &QAbstractButton::clicked, this, [this] { fix(); });
        connect(&m_tick, &QTimer::timeout, this, [this] { update(); });
        m_tick.setInterval(33);
        m_fade.setStartValue(0.0); m_fade.setEndValue(1.0); m_fade.setEasingCurve(QEasingCurve::OutCubic);
        connect(&m_fade, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_fadeV = v.toReal(); update(); });
        connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
        m_installedDir = installedDir();
    }

    bool hasInstall() const { return !m_installedDir.isEmpty(); }
    void begin(const QStringList& args) {
        if (args.contains("--uninstall")) { m_flow = Uninstall; m_sw1->setChecked(true); setPage(Remove, false); }
        else if (args.contains("--doctor")) { setPage(Doctor, false); runCheck(); }
        else if (hasInstall()) setPage(Maintenance, false);
        else setPage(Choose, false);
    }

    void setPage(Page p, bool animate = true) {
        m_page = p;
        m_fadeV = 1.0;
        if (animate && Theme::i().animMs(1) > 0) { m_fade.stop(); m_fade.setDuration(Theme::i().animMs(260)); m_fade.start(); m_fadeV = 0.0; }
        const bool un = m_flow == Uninstall;
        m_primary->setText(p == Remove ? tr("Remove") : tr("Install"));
        m_primary->setDanger(p == Remove);
        m_primary->setVisible(p == Choose || p == Remove);
        m_cancel->setVisible(p == Choose || p == Remove);
        m_change->setVisible(p == Choose);
        m_sw1->setVisible(p == Choose || p == Remove); m_sw2->setVisible(p == Choose); m_sw3->setVisible(p == Choose);
        m_open->setVisible(p == Done && !un); m_close->setVisible(p == Done || p == Failed || p == Maintenance || p == Doctor);
        m_retry->setVisible(p == Failed);
        m_default->setVisible(p == Done && !un);
        m_fix->setVisible(p == Doctor && !m_checking && fixableCount() > 0);
        m_back->setVisible(p == Doctor && hasInstall() && !m_payload.isEmpty());
        m_x->setVisible(p != Working);
        if (p == Working || (p == Doctor && m_checking)) m_tick.start(); else m_tick.stop();
        layoutPage();
        update();
    }

    void forceProgress(double frac, const QString& stage, const QString& file) { m_frac = frac; m_stage = stage; m_file = file; }
    void forceDoctor(const QVector<Check>& c) { m_checks = c; m_checking = false; }
    void forceInstalled(const QString& dir) { m_installedDir = dir; }

protected:
    QRectF card() const { return QRectF(rect()).adjusted(26, 22, -26, -30); }
    void resizeEvent(QResizeEvent*) override { layoutPage(); }
    void keyPressEvent(QKeyEvent* e) override {
        if (e->key() == Qt::Key_Escape && m_page != Working) close();
        else if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && (m_page == Choose || m_page == Remove)) start();
    }
    void closeEvent(QCloseEvent* e) override { if (m_page == Working || m_checking) e->ignore(); else QWidget::closeEvent(e); }   // worker threads still point at this window

    QVector<QRectF> cardRects() const {
        const QRectF c = card(); const qreal x0 = c.left() + 300, w = c.right() - 36 - x0;
        QVector<QRectF> v; qreal y = c.top() + 140;
        for (int i = 0; i < 3; ++i) { v << QRectF(x0, y, w, 70); y += 80; }
        return v;
    }
    void mouseMoveEvent(QMouseEvent* e) override {
        int h = -1;
        if (m_page == Maintenance) { const auto r = cardRects(); for (int i = 0; i < r.size(); ++i) if (r[i].contains(e->position())) h = i; }
        if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
    }
    void mousePressEvent(QMouseEvent* e) override {
        if (e->button() != Qt::LeftButton) return;
        if (m_page == Maintenance) {
            const auto r = cardRects();
            for (int i = 0; i < r.size(); ++i) if (r[i].contains(e->position())) {
                if (i == 0) { m_flow = Install; m_opt.dest = m_installedDir; start(); }
                else if (i == 1) { runCheck(); }
                else { m_flow = Uninstall; setPage(Remove); }
                return;
            }
        }
        if (windowHandle()) windowHandle()->startSystemMove();
    }

    void layoutPage() {
        const QRectF c = card();
        const int x0 = int(c.left()) + 300, x1 = int(c.right()) - 36;
        m_x->move(int(c.right()) - 44, int(c.top()) + 12);
        const int by = int(c.bottom()) - 78;
        m_cancel->setGeometry(x1 - 110 - 130, by, 110, 40);
        m_primary->setGeometry(x1 - 130, by, 130, 40);
        m_change->setGeometry(x1 - 96, int(c.top()) + 168, 96, 36);
        int y = int(c.top()) + 232;
        for (auto* s : {m_sw1, m_sw2, m_sw3}) { s->move(x1 - 38, y + 10); y += 52; }
        if (m_page == Remove) m_sw1->move(x1 - 38, int(c.top()) + 184);
        m_open->setGeometry(x0, int(c.top()) + 306, 140, 42);
        m_retry->setGeometry(x0, int(c.top()) + 306, 120, 42);
        m_default->setGeometry(x0, int(c.top()) + 362, m_default->sizeHint().width(), 38);
        const bool bottom = m_page == Maintenance || m_page == Doctor;
        m_close->setGeometry(bottom ? x1 - 100 : (!m_open->isHidden() ? x0 + 152 : (!m_retry->isHidden() ? x0 + 132 : x0)), bottom ? by : int(c.top()) + 306, 100, 40);
        m_fix->setGeometry(x1 - 100 - 10 - 140, by, 140, 40);
        m_back->setGeometry(!m_fix->isHidden() ? x1 - 100 - 10 - 140 - 10 - 80 : x1 - 100 - 10 - 80, by, 80, 40);
    }

    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto& c = pal();
        const QRectF r = card();
        for (int i = 14; i >= 1; --i) { p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 5 + (c.dark ? 3 : 0))); p.drawRoundedRect(r.adjusted(-i, -i + 6, i, i + 6), 20 + i, 20 + i); }
        QPainterPath clip; clip.addRoundedRect(r, 20, 20);
        p.save(); p.setClipPath(clip);
        p.fillRect(r, c.bg);
        const QRectF left(r.left(), r.top(), 272, r.height());
        QLinearGradient g(left.topLeft(), left.bottomLeft()); g.setColorAt(0, QColor("#1c1c16")); g.setColorAt(1, QColor("#0b0b08"));
        p.fillRect(left, g);
        QRadialGradient glow(left.center() + QPointF(0, -20), left.width() * 0.95); glow.setColorAt(0, QColor(79, 209, 214, 90)); glow.setColorAt(1, QColor(79, 209, 214, 0));
        p.fillRect(left, glow);
        const qreal d = 112; const QRectF disc(left.center().x() - d / 2, left.top() + 112, d, d);
        QLinearGradient mg(disc.topLeft(), disc.bottomRight()); mg.setColorAt(0, QColor("#f4f7fb")); mg.setColorAt(0.45, QColor("#a9b2be")); mg.setColorAt(0.58, QColor("#e4e9ef")); mg.setColorAt(1, QColor("#7d8793"));
        p.setPen(QPen(QColor(255, 255, 255, 140), 1.2)); p.setBrush(mg); p.drawEllipse(disc);
        drawIcon(&p, Icon::Play, disc.adjusted(30, 28, -28, -28), QColor("#0c6a6e"), 1.4);
        p.setPen(QColor("#edebe2")); p.setFont(Theme::i().font(38, QFont::Bold, true));
        p.drawText(QRectF(left.left(), disc.bottom() + 24, left.width(), 52), Qt::AlignCenter, kApp);
        p.setPen(QColor("#aba99b")); p.setFont(Theme::i().font(14));
        p.drawText(QRectF(left.left() + 20, disc.bottom() + 80, left.width() - 40, 44), Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, tr("everything you play,\nin one calm window"));
        p.setPen(QColor(255, 255, 255, 70)); p.setFont(Theme::i().font(12));
        p.drawText(QRectF(left.left(), left.bottom() - 40, left.width(), 24), Qt::AlignCenter, QStringLiteral("version ") + kVersion);
        p.restore();
        p.setPen(QPen(c.border, 1)); p.setBrush(Qt::NoBrush); p.drawRoundedRect(r, 20, 20);

        p.setOpacity(m_fadeV);
        const qreal x0 = r.left() + 300, w = r.right() - 36 - x0;
        const bool un = m_flow == Uninstall;
        auto title = [&](const QString& t) { p.setPen(c.text); p.setFont(Theme::i().font(30, QFont::DemiBold, true)); p.drawText(QRectF(x0, r.top() + 56, w, 46), Qt::AlignVCenter, t); };
        auto sub = [&](const QString& t, qreal y = 108) { p.setPen(c.muted); p.setFont(Theme::i().font(14)); p.drawText(QRectF(x0, r.top() + y, w, 44), Qt::AlignTop | Qt::TextWordWrap, t); };
        if (m_page == Choose) {
            title(tr("Install kinoz"));
            sub(tr("Free, offline and calm. It only takes a few seconds."));
            p.setPen(c.muted); p.setFont(Theme::i().font(12, QFont::DemiBold)); p.drawText(QRectF(x0, r.top() + 150, w, 18), Qt::AlignVCenter, tr("INSTALL TO"));
            p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::Medium));
            p.drawText(QRectF(x0, r.top() + 172, w - 110, 30), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(QDir::toNativeSeparators(m_opt.dest), Qt::ElideMiddle, int(w - 110)));
            p.setPen(c.border); p.drawLine(QPointF(x0, r.top() + 216), QPointF(x0 + w, r.top() + 216));
            const QString rows[3] = {tr("Add “Play with kinoz” to the right-click menu"), tr("Add kinoz to the “Open with” list"), tr("Create a desktop shortcut")};
            qreal y = r.top() + 232;
            p.setFont(Theme::i().font(14, QFont::Medium));
            for (const QString& t : rows) { p.setPen(c.text); p.drawText(QRectF(x0, y, w - 56, 44), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(t, Qt::ElideRight, int(w - 60))); y += 52; }
            p.setPen(c.muted); p.setFont(Theme::i().font(12));
            p.drawText(QRectF(x0, r.bottom() - 34, w, 22), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(tr("Free software (GPLv3) by Atharva Patil, P4inz Interactive Labs"), Qt::ElideRight, int(w)));
        } else if (m_page == Maintenance) {
            title(tr("kinoz is installed"));
            const QString iv = installedVersion();
            const bool older = !iv.isEmpty() && versionNewer(iv, kVersion);
            sub(older ? tr("Installed %1 is newer than this setup (%2).").arg(iv, kVersion) : tr("Installed version %1. This setup has %2.").arg(iv.isEmpty() ? QStringLiteral("?") : iv, kVersion));
            const auto cr = cardRects();
            const QString titles[3] = {iv == kVersion ? tr("Reinstall") : (older ? tr("Install this older version") : tr("Update to %1").arg(kVersion)), tr("Repair"), tr("Remove")};
            const QString descs[3] = {tr("Replace the program files. Your settings and library stay."), tr("Run the Doctor: check everything and fix what's wrong."), tr("Uninstall kinoz, and all your data too if you like.")};
            const Icon icons[3] = {Icon::Export, Icon::Settings, Icon::Trash};
            for (int i = 0; i < 3; ++i) {
                const bool hov = i == m_hover;
                p.setPen(QPen(hov ? c.accent : c.border, 1)); p.setBrush(hov ? c.accentSubtle : c.surface); p.drawRoundedRect(cr[i], 14, 14);
                drawIcon(&p, icons[i], QRectF(cr[i].left() + 18, cr[i].center().y() - 14, 28, 28), i == 2 ? c.danger : c.accent, 1.7);
                p.setPen(c.text); p.setFont(Theme::i().font(16, QFont::DemiBold)); p.drawText(QRectF(cr[i].left() + 64, cr[i].top() + 14, cr[i].width() - 80, 24), Qt::AlignVCenter, titles[i]);
                p.setPen(c.muted); p.setFont(Theme::i().font(12.5)); p.drawText(QRectF(cr[i].left() + 64, cr[i].top() + 40, cr[i].width() - 80, 32), Qt::AlignTop | Qt::TextWordWrap, descs[i]);
            }
        } else if (m_page == Doctor) {
            title(tr("Doctor"));
            int bad = 0; for (const auto& ch : m_checks) if (ch.state > 0) ++bad;
            sub(m_checking ? tr("Checking your installation…") : (m_checks.isEmpty() ? tr("Nothing to check.") : (bad == 0 ? (m_fixedNote.isEmpty() ? tr("Everything looks healthy.") : m_fixedNote) : (m_fixedNote.isEmpty() ? tr("%1 thing(s) need attention.").arg(bad) : m_fixedNote))), 100);
            qreal y = r.top() + 134;
            for (const auto& ch : m_checks) {
                const QColor col = ch.state == 0 ? c.accent : (ch.state == 1 ? QColor("#d99a2b") : c.danger);
                drawIcon(&p, ch.state == 0 ? Icon::Check : Icon::Close, QRectF(x0, y + 4, 18, 18), col, 2.2);
                p.setPen(c.text); p.setFont(Theme::i().font(13.5)); p.drawText(QRectF(x0 + 28, y, 170, 26), Qt::AlignVCenter, ch.name);
                p.setPen(c.muted); p.setFont(Theme::i().font(12)); p.drawText(QRectF(x0 + 200, y, w - 200, 26), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(ch.detail, Qt::ElideMiddle, int(w - 204)));
                y += 26;
            }
            if (m_checking) { const qreal ph = QTime::currentTime().msecsSinceStartOfDay() / 700.0; p.setPen(Qt::NoPen); p.setBrush(c.accent); for (int i = 0; i < 3; ++i) { const qreal rr = 3.2 + 1.6 * std::sin(ph * 6.28 + i); p.drawEllipse(QPointF(x0 + 8 + i * 18, y + 16), rr, rr); } }
        } else if (m_page == Remove) {
            title(tr("Remove kinoz?"));
            sub(tr("This removes kinoz, its right-click entries and its shortcuts from your PC."));
            p.setPen(c.text); p.setFont(Theme::i().font(14.5, QFont::DemiBold)); p.drawText(QRectF(x0, r.top() + 176, w - 56, 24), Qt::AlignVCenter, tr("Delete all my data too"));
            p.setPen(c.muted); p.setFont(Theme::i().font(12.5)); p.drawText(QRectF(x0, r.top() + 204, w - 56, 80), Qt::AlignTop | Qt::TextWordWrap, tr("Settings, resume points, thumbnails, logs and frames you exported, all in one go. Your own media files are never touched."));
        } else if (m_page == Working) {
            title(un ? tr("Removing…") : (m_updating ? tr("Updating…") : tr("Installing…")));
            sub(m_stage.isEmpty() ? tr("One moment.") : m_stage);
            const QRectF track(x0, r.top() + 200, w, 10);
            p.setPen(Qt::NoPen); p.setBrush(c.sunken); p.drawRoundedRect(track, 5, 5);
            const QRectF fill(track.left(), track.top(), qMax(10.0, track.width() * qBound(0.0, m_frac, 1.0)), track.height());
            QLinearGradient lg(fill.topLeft(), fill.topRight());
            const qreal ph = Theme::i().reduceMotion() ? 0.5 : std::fmod(QTime::currentTime().msecsSinceStartOfDay() / 1600.0, 1.0);
            lg.setColorAt(0, c.accent); lg.setColorAt(qBound(0.0, ph - 0.15, 1.0), c.accent); lg.setColorAt(qBound(0.0, ph, 1.0), QColor(255, 255, 255, 230)); lg.setColorAt(qBound(0.0, ph + 0.15, 1.0), c.accent); lg.setColorAt(1, c.accent);
            p.setBrush(lg); p.drawRoundedRect(fill, 5, 5);
            p.setPen(c.muted); p.setFont(Theme::i().font(13));
            p.drawText(QRectF(x0, r.top() + 224, w, 22), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("%1%   %2").arg(int(m_frac * 100)).arg(m_file));
        } else if (m_page == Done) {
            const QRectF ring(x0, r.top() + 56, 56, 56);
            p.setPen(Qt::NoPen); p.setBrush(c.accentSubtle); p.drawEllipse(ring);
            drawIcon(&p, Icon::Check, ring.adjusted(13, 13, -13, -13), c.accent, 2.6);
            p.setPen(c.text); p.setFont(Theme::i().font(30, QFont::DemiBold, true)); p.drawText(QRectF(x0, r.top() + 128, w, 46), Qt::AlignVCenter, un ? tr("kinoz was removed") : tr("kinoz is ready"));
            sub(un ? (m_opt.deleteData ? tr("kinoz and all its data are gone. Your own media files were not touched.") : tr("Your data was kept. Your own media files were not touched.")) : tr("Everything is in place. Drop a folder onto kinoz and press play."), 182);
        } else {
            const QRectF ring(x0, r.top() + 56, 56, 56);
            p.setPen(Qt::NoPen); p.setBrush(QColor(c.danger.red(), c.danger.green(), c.danger.blue(), 40)); p.drawEllipse(ring);
            drawIcon(&p, Icon::Close, ring.adjusted(15, 15, -15, -15), c.danger, 2.4);
            p.setPen(c.text); p.setFont(Theme::i().font(28, QFont::DemiBold, true)); p.drawText(QRectF(x0, r.top() + 128, w, 44), Qt::AlignVCenter, tr("That didn't work"));
            sub(m_error + "\n" + tr("Details: ") + QDir::toNativeSeparators(logPath()), 180);
        }
    }

    void chooseFolder() {
        auto* sh = new Sheet(this);
        auto* fp = new FolderPicker(FolderPicker::PickFolder, QFileInfo(m_opt.dest).absolutePath().isEmpty() ? QDir::homePath() : QFileInfo(m_opt.dest).absolutePath());
        sh->setContent(fp, QSize(500, 420));
        connect(fp, &FolderPicker::cancelled, sh, &Sheet::closeSheet);
        connect(fp, &FolderPicker::chosen, this, [this, sh](const QString& dir) {
            sh->closeSheet();
            QString d = QDir::cleanPath(dir);
            if (!d.endsWith("/kinoz", Qt::CaseInsensitive)) d += "/kinoz";
            m_opt.dest = QDir::toNativeSeparators(d); update();
        });
        sh->openSheet();
    }

    int fixableCount() const { int n = 0; for (const auto& c : m_checks) if (c.state > 0 && c.fixable) ++n; return n; }

    void runCheck() {
        m_checking = true; m_checks.clear(); m_fixedNote.clear();
        setPage(Doctor);
        const QString dest = !m_installedDir.isEmpty() ? m_installedDir : QCoreApplication::applicationDirPath();
        const QString payload = m_payload;
        QThread* t = QThread::create([this, dest, payload] {
            const auto res = runDoctor(QDir::fromNativeSeparators(dest), payload);
            QThread::msleep(500);
            QMetaObject::invokeMethod(this, [this, res] { m_checks = res; m_checking = false; setPage(Doctor, false); }, Qt::QueuedConnection);
        });
        connect(t, &QThread::finished, t, &QObject::deleteLater);
        t->start();
    }

    void fix() {
        m_checking = true; m_fixedNote.clear(); setPage(Doctor, false);
        const QString dest = !m_installedDir.isEmpty() ? m_installedDir : QCoreApplication::applicationDirPath();
        const QString payload = m_payload; const auto checks = m_checks;
        QThread* t = QThread::create([this, dest, payload, checks] {
            const QString d = QDir::fromNativeSeparators(dest);
            applyFixes(checks, d, payload);
            const auto res = runDoctor(d, payload);
            QThread::msleep(400);
            QMetaObject::invokeMethod(this, [this, res] {
                m_checks = res; m_checking = false;
                int bad = 0; for (const auto& c : res) if (c.state > 0) ++bad;
                m_fixedNote = bad == 0 ? tr("Fixed. Everything looks healthy now.") : tr("Fixed what could be fixed; %1 left.").arg(bad);
                setPage(Doctor, false);
            }, Qt::QueuedConnection);
        });
        connect(t, &QThread::finished, t, &QObject::deleteLater);
        t->start();
    }

    void start() {
        m_opt.rightClick = m_sw1->isChecked(); m_opt.openWith = m_sw2->isChecked(); m_opt.desktop = m_sw3->isChecked();
        const Flow flow = m_flow;
        m_opt.deleteData = flow == Uninstall && m_sw1->isChecked();
        if (flow == Install && hasInstall() && m_opt.dest.isEmpty()) m_opt.dest = m_installedDir;
        m_updating = flow == Install && hasInstall();
        m_frac = 0; m_stage.clear(); m_file.clear();
        setPage(Working);
        const Options opt = m_opt; const QString payload = m_payload;
        QThread* t = QThread::create([this, opt, payload, flow] {
            Progress pr = [this](qint64 d, qint64 tot, const QString& stage, const QString& file) {
                QMetaObject::invokeMethod(this, [this, d, tot, stage, file] { m_frac = tot > 0 ? double(d) / double(tot) : 0; m_stage = stage; m_file = file; }, Qt::QueuedConnection);
            };
            const QString err = flow == Install ? installJob(opt, payload, pr) : uninstallJob(opt, pr);
            QThread::msleep(450);
            QMetaObject::invokeMethod(this, [this, err] { m_error = err; if (err.isEmpty()) { m_frac = 1; m_installedDir = m_flow == Install ? QDir::toNativeSeparators(m_opt.dest) : QString(); } setPage(err.isEmpty() ? Done : Failed); }, Qt::QueuedConnection);
        });
        connect(t, &QThread::finished, t, &QObject::deleteLater);
        t->start();
    }

private:
    Flow m_flow = Install;
    Page m_page = Choose;
    QString m_payload, m_installedDir;
    bool m_launch;
    bool m_updating = false, m_checking = false;
    Options m_opt;
    TextButton *m_primary, *m_cancel, *m_change, *m_open, *m_close, *m_retry, *m_fix, *m_back, *m_default;
    Switch *m_sw1 = nullptr, *m_sw2 = nullptr, *m_sw3 = nullptr;
    IconButton* m_x;
    QTimer m_tick;
    QVariantAnimation m_fade;
    qreal m_fadeV = 1.0;
    double m_frac = 0;
    int m_hover = -1;
    QString m_stage, m_file, m_error, m_fixedNote;
    QVector<Check> m_checks;
};
}

namespace Installer {
void registerDefaultApp(const QString& exePath) { registerCapabilities(exePath); }

bool wanted(const QStringList& args) {
    for (const char* f : {"--installer", "--uninstall", "--doctor", "--installer-shots"}) if (args.contains(QString::fromLatin1(f))) return true;
    return false;
}

int run(const QStringList& args) {
    Theme::i().loadFonts();
    Theme::i().setMotionMode(0);
    const bool silent = args.contains(QStringLiteral("--silent"));
    const bool noLaunch = args.contains(QStringLiteral("--no-launch"));
    const bool installerMode = args.contains(QStringLiteral("--installer")) || args.contains(QStringLiteral("--installer-shots"));
    const QString payload = installerMode ? QCoreApplication::applicationDirPath() : QString();
    auto arg = [&](const char* name) { const int i = args.indexOf(QString::fromLatin1(name)); return (i >= 0 && i + 1 < args.size()) ? args.at(i + 1) : QString(); };
    logLine(QStringLiteral("--- kinoz setup %1: %2").arg(kVersion, args.join(' ')));

    if (args.contains(QStringLiteral("--installer-shots"))) {          // pictures of every page, for the docs
        const QString dir = arg("--installer-shots");
        QDir().mkpath(dir);
        Theme::i().setMotionMode(2);
        const QVector<Check> sample = {{"Install folder", "F:\\Programs\\kinoz", 0, false, {}}, {"Program files", "142 files verified", 0, false, {}}, {"Microsoft runtime", "installed", 0, false, {}},
                                       {"Start Menu shortcut", "missing", 1, true, "shortcut"}, {"Settings → Apps entry", "present", 0, false, {}}, {"Right-click menu", "“Play with kinoz” is set up", 0, false, {}},
                                       {"“Open with” list", "present", 0, false, {}}, {"Data folder", "F:\\Programs\\kinoz\\data", 0, false, {}}, {"Settings files", "healthy", 0, false, {}}, {"Disk space", "812 GB free", 0, false, {}}};
        for (int dark = 0; dark < 2; ++dark) {
            Theme::i().setMode(dark ? Theme::Mode::Dark : Theme::Mode::Light);
            const QString sfx = dark ? "-dark" : "-light";
            auto shot = [&](SetupWindow& w, const QString& name) { w.ensurePolished(); w.grab().save(dir + "/" + name + sfx + ".png"); };
            SetupWindow w(payload, QStringLiteral("F:\\Programs\\kinoz"), false);
            w.setPage(SetupWindow::Choose, false); shot(w, "setup-choose");
            w.forceInstalled(QStringLiteral("F:\\Programs\\kinoz")); w.setPage(SetupWindow::Maintenance, false); shot(w, "setup-maintenance");
            w.forceDoctor(sample); w.setPage(SetupWindow::Doctor, false); shot(w, "setup-doctor");
            w.setPage(SetupWindow::Remove, false); shot(w, "setup-remove");
            w.setPage(SetupWindow::Working, false); w.forceProgress(0.62, QObject::tr("Copying files"), QStringLiteral("Qt6Widgets.dll")); shot(w, "setup-installing");
            w.setPage(SetupWindow::Done, false); shot(w, "setup-done");
        }
        return 0;
    }

    const bool uninstall = args.contains(QStringLiteral("--uninstall"));
    Options o;
    o.dest = !arg("--dest").isEmpty() ? QDir::toNativeSeparators(arg("--dest")) : defaultInstallDir();
    o.silent = silent; o.launch = !noLaunch;

    if (silent) {                                                       // updates and scripted installs: no window
        Progress none = [](qint64, qint64, const QString&, const QString&) {};
        QString err;
        if (uninstall) { o.deleteData = args.contains(QStringLiteral("--delete-data")); err = uninstallJob(o, none); }
        else if (args.contains(QStringLiteral("--doctor"))) { const QString d = installedDir(); applyFixes(runDoctor(d, payload), d, payload); }
        else {
            err = installJob(o, payload, none);
            if (err.isEmpty() && o.launch) QProcess::startDetached(QDir::cleanPath(o.dest + "/kinoz.exe"), {});
        }
        if (!err.isEmpty()) {
            logLine("FAILED: " + err);
            // An update that could not be applied must not leave the user without a player: reopen what is installed
            // (the failed copy was rolled back, so this is the previous, working version).
            const QString prev = QDir::cleanPath(o.dest + "/kinoz.exe");
            if (!uninstall && o.launch && QFile::exists(prev)) QProcess::startDetached(prev, {});
        }
        return err.isEmpty() ? 0 : 1;
    }
    SetupWindow w(payload, o.dest, o.launch);
    w.begin(args);
    if (QScreen* s = QGuiApplication::primaryScreen()) w.move(s->availableGeometry().center() - QPoint(w.width() / 2, w.height() / 2));
    w.show();
    w.activateWindow();
    return QApplication::exec();
}
}
#endif  // Q_OS_WIN
