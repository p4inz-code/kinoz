// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/Doctor.h"
#include "core/JsonStore.h"
#include "core/Log.h"
#include "core/Paths.h"
#include "core/Updater.h"
#include "installer/Installer.h"
#include "ui/MainWindow.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLocalServer>
#include <QLocalSocket>
#include <QFileOpenEvent>
#include <functional>
#include <QSurfaceFormat>
#include <QTextStream>
#include <QTimer>
#include <clocale>
#include <csignal>
#include <exception>
#ifdef Q_OS_WIN
#include <windows.h>
#include <dbghelp.h>
static wchar_t g_dumpPath[MAX_PATH] = {};
static LONG WINAPI crashFilter(EXCEPTION_POINTERS* ep) {
    HANDLE f = CreateFileW(g_dumpPath, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        MINIDUMP_EXCEPTION_INFORMATION mei{GetCurrentThreadId(), ep, FALSE};
        MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), f, MiniDumpNormal, &mei, nullptr, nullptr);
        CloseHandle(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

static QString pipeName() {
    return QStringLiteral("kinoz-") + QString::fromLatin1(QCryptographicHash::hash(QCoreApplication::applicationDirPath().toUtf8(), QCryptographicHash::Sha1).toHex().left(12));
}

// macOS hands files to an app through "open file" events (Finder double-click, Open With, dropping on the Dock icon),
// not through the command line. Collect a burst of them (several selected files arrive one by one) and open them together.
// Files that arrive before the window exists are kept and opened as soon as it does.
class OpenFilter : public QObject {
public:
    std::function<void(const QStringList&)> sink;
    QStringList pending;
    bool eventFilter(QObject*, QEvent* e) override {
        if (e->type() != QEvent::FileOpen) return false;
        const QString f = static_cast<QFileOpenEvent*>(e)->file();
        if (f.isEmpty()) return false;
        pending << f;
        if (sink && !m_timer) {
            m_timer = true;
            QTimer::singleShot(150, this, [this] { m_timer = false; flush(); });
        }
        return true;
    }
    void flush() { if (sink && !pending.isEmpty()) { const QStringList p = pending; pending.clear(); sink(p); } }
private:
    bool m_timer = false;
};

int main(int argc, char** argv) {
#ifdef Q_OS_WIN
    // Libraries are only ever loaded from kinoz's own folder and System32 (blocks "DLL planting" from the current directory or PATH).
    SetDefaultDllDirectories(LOAD_LIBRARY_SEARCH_APPLICATION_DIR | LOAD_LIBRARY_SEARCH_SYSTEM32);
#endif
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapInterval(1);
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QApplication app(argc, argv);
    std::setlocale(LC_NUMERIC, "C");                                  // libmpv refuses to start under a locale with a decimal comma
    app.setApplicationName(QStringLiteral(APP_NAME));
    app.setOrganizationName(QStringLiteral("local"));
    app.setQuitOnLastWindowClosed(true);
    static OpenFilter openFilter;
    app.installEventFilter(&openFilter);
#ifdef Q_OS_LINUX
    QGuiApplication::setDesktopFileName(QStringLiteral("kinoz"));      // lets Wayland and the shell match the window to kinoz.desktop (icon, name)
#endif
    if (Installer::wanted(app.arguments())) return Installer::run(app.arguments());      // our own setup wizard
    Paths::ensure();
    // `kinoz --doctor` (macOS, Linux) or `kinoz --doctor-text` (everywhere): a text health report; add --fix to repair the safe things.
    if (app.arguments().contains(QStringLiteral("--doctor-text")) || (app.arguments().contains(QStringLiteral("--doctor")) && !Installer::wanted(app.arguments())))
        return Doctor::runTextMode(app.arguments());

    // `kinoz --wipe` forgets what kinoz remembers (recent files, resume points, marks, music index, thumbnails, logs) and exits.
    // Close kinoz first. Your folders, settings, playlists and saved frames are left alone.
    if (app.arguments().contains(QStringLiteral("--wipe"))) {
        { JsonStore s(Paths::dataDir() + QStringLiteral("/settings.json")); s.remove("recent"); s.remove("lastFolder"); s.flush(); }
        for (const char* f : {"resume.json", "marks.json", "music.json"}) QFile::remove(Paths::dataDir() + QLatin1Char('/') + QString::fromLatin1(f));
        for (const QFileInfo& fi : QDir(Paths::thumbsDir()).entryInfoList({QStringLiteral("*.jpg")}, QDir::Files)) QFile::remove(fi.absoluteFilePath());
        for (const QFileInfo& fi : QDir(Paths::logsDir()).entryInfoList(QDir::Files)) QFile::remove(fi.absoluteFilePath());
        return 0;
    }

    Log::init(app.arguments().contains(QStringLiteral("--debug")));       // keeps the last runs' logs, writes a header (docs/MAINTAINING.md)
    struct LogGuard { ~LogGuard() { Log::shutdown(); } } logGuard;       // never log into a destroyed file at exit

#ifdef Q_OS_WIN
    // If the app ever crashes, leave a small dump in the logs folder so the problem can be found and fixed.
    QDir::toNativeSeparators(Paths::logsDir() + QStringLiteral("/crash.dmp")).toWCharArray(g_dumpPath);
    SetUnhandledExceptionFilter(crashFilter);
    if (qEnvironmentVariableIsSet("KINOZ_TRACE_ABORT")) {      // debugging aid: where does an abort() come from?
        signal(SIGABRT, [](int) {
            void* fr[40]; const USHORT n = CaptureStackBackTrace(0, 40, fr, nullptr);
            FILE* f = _wfopen((std::wstring(g_dumpPath) + L".abort.txt").c_str(), L"w");
            if (!f) return;
            for (USHORT i = 0; i < n; ++i) {
                HMODULE m = nullptr; wchar_t nm[MAX_PATH] = L"?";
                if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, reinterpret_cast<LPCWSTR>(fr[i]), &m)) GetModuleFileNameW(m, nm, MAX_PATH);
                fwprintf(f, L"%u %ls +0x%llx\n", i, wcsrchr(nm, L'\\') ? wcsrchr(nm, L'\\') + 1 : nm, (unsigned long long)((char*)fr[i] - (char*)m));
            }
            fclose(f);
        });
        std::set_terminate([] { FILE* f = _wfopen((std::wstring(g_dumpPath) + L".terminate.txt").c_str(), L"w"); if (f) { fputs("std::terminate called", f); fclose(f); } abort(); });
    }
#endif
    QStringList args;
    const QStringList all = app.arguments();
    for (int i = 1; i < all.size(); ++i) {
        const QString& a = all[i];
        if (a.startsWith(QStringLiteral("--action="))) args << a;                       // remote-control: --action=fullscreen
        else if (!a.startsWith('-') && !(i > 0 && all[i - 1] == QStringLiteral("--shots"))) args << QDir::cleanPath(QFileInfo(a).absoluteFilePath());
    }
    const bool selftest = all.contains(QStringLiteral("--selftest"));
    const bool newWindow = all.contains(QStringLiteral("--new"));      // extra independent player window
    const QString shotsDir = all.contains(QStringLiteral("--shots")) ? all.value(all.indexOf(QStringLiteral("--shots")) + 1) : QString();
    const bool bench = all.contains(QStringLiteral("--bench"));
    const bool stress = selftest || bench || newWindow || !shotsDir.isEmpty();

    // Single instance: hand the files to the running player and exit immediately.
    if (!stress) {
        QLocalSocket sock;
        sock.connectToServer(pipeName());
        if (sock.waitForConnected(120)) {
            sock.write(QJsonDocument(QJsonArray::fromStringList(args)).toJson(QJsonDocument::Compact));
            sock.waitForBytesWritten(500);
            return 0;
        }
    }
    QLocalServer server;
    QLocalServer::removeServer(pipeName());
    if (!stress) server.listen(pipeName());

    Theme::i().loadFonts();
    JsonStore settings(Paths::dataDir() + QStringLiteral("/settings.json"));
    JsonStore resume(Paths::dataDir() + QStringLiteral("/resume.json"));
    if (newWindow) { settings.setReadOnly(true); resume.setReadOnly(true); }

    // An update the user chose to install "later" is applied now, before any window exists; the installer reopens kinoz.
    if (!stress && Updater::applyPendingOnStartup(&settings)) { settings.flush(); return 0; }

    // Crash-loop protection: a "running" marker exists while the main window is open and is removed on a clean exit.
    // If it is still there at the next start, the last run ended abnormally (crash, driver reset, power cut). After two
    // abnormal ends in a row kinoz starts in safe mode: software decoding, no glass shader, taskbar-friendly fullscreen.
    const QString marker = Paths::dataDir() + QStringLiteral("/running.marker");
    bool safeMode = false;
    if (!stress) {
        const bool crashed = QFile::exists(marker);
        const int streak = crashed ? int(settings.num("crashStreak", 0)) + 1 : 0;
        settings.set("crashStreak", streak);
        if (streak >= 2) {
            safeMode = true;
            settings.set("decode", 2); settings.set("glass", false); settings.set("fsSafe", true);
        }
        QFile m(marker);
        if (m.open(QIODevice::WriteOnly)) m.write("1");
    }

    MainWindow w(&settings, &resume);
    w.show();
    if (safeMode) QTimer::singleShot(1800, &w, [&w] { w.notifySafeMode(); });
    openFilter.sink = [&w](const QStringList& p) { w.openPaths(p); };      // macOS file-open events from now on go straight to the window
    openFilter.flush();                                                       // ... and the ones that arrived while it was starting

    QObject::connect(&server, &QLocalServer::newConnection, &w, [&] {
        while (QLocalSocket* c = server.nextPendingConnection()) {
            QObject::connect(c, &QLocalSocket::readyRead, c, [c, &w] {
                const auto arr = QJsonDocument::fromJson(c->readAll()).array();
                QStringList paths;
                for (const auto& v : arr) paths << v.toString();
                w.openPaths(paths);
                c->disconnectFromServer();
            });
            QObject::connect(c, &QLocalSocket::disconnected, c, &QObject::deleteLater);
        }
    });

    if (!args.isEmpty()) w.openPaths(args);
    if (selftest) w.runSelfTest();
    if (bench) w.runBench();
    if (!shotsDir.isEmpty()) w.runShots(shotsDir);
    const int rc = app.exec();
    if (!stress) { settings.flush(); QFile::remove(marker); }          // reaching this line means the run ended cleanly
    return rc;
}
