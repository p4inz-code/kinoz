#include "core/JsonStore.h"
#include "core/Paths.h"
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
#include <QSurfaceFormat>
#include <QTextStream>
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

static QFile* g_log = nullptr;
static void logHandler(QtMsgType, const QMessageLogContext&, const QString& msg) {
    if (!g_log) return;
    QTextStream ts(g_log); ts.setEncoding(QStringConverter::Utf8); ts << msg << '\n';
    g_log->flush();
}

int main(int argc, char** argv) {
    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setSwapInterval(1);
    fmt.setDepthBufferSize(0);
    fmt.setStencilBufferSize(0);
    QSurfaceFormat::setDefaultFormat(fmt);
    QApplication::setAttribute(Qt::AA_UseDesktopOpenGL);

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral(APP_NAME));
    app.setOrganizationName(QStringLiteral("local"));
    app.setQuitOnLastWindowClosed(true);
    if (Installer::wanted(app.arguments())) return Installer::run(app.arguments());      // our own setup wizard
    Paths::ensure();

    QFile log(Paths::logsDir() + QStringLiteral("/app.log"));
    if (log.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) { g_log = &log; qInstallMessageHandler(logHandler); }
    struct LogGuard { ~LogGuard() { qInstallMessageHandler(nullptr); g_log = nullptr; } } logGuard;   // never log into a destroyed file at exit

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
    const bool stress = selftest || newWindow || !shotsDir.isEmpty();

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

    MainWindow w(&settings, &resume);
    w.show();

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
    if (!shotsDir.isEmpty()) w.runShots(shotsDir);
    return app.exec();
}
