// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/Log.h"
#include "core/Paths.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QSysInfo>
#include <QTextStream>
#include <QtGlobal>
#include <csignal>
#if defined(__linux__) || defined(__APPLE__)
#include <execinfo.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace {
QFile* g_file = nullptr;
QMutex g_mutex;
bool g_debug = false;
int g_crashFd = -1;

void handler(QtMsgType type, const QMessageLogContext&, const QString& msg) {
    if (type == QtDebugMsg && !g_debug) return;
    QMutexLocker lock(&g_mutex);
    if (!g_file) return;
    const char lvl = type == QtDebugMsg ? 'D' : type == QtInfoMsg ? 'I' : type == QtWarningMsg ? 'W' : 'E';
    QTextStream ts(g_file);
    ts.setEncoding(QStringConverter::Utf8);
    ts << QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz")) << ' ' << lvl << ' ' << msg << '\n';
    g_file->flush();
}

void rotate() {
    const QString d = Paths::logsDir();
    QFile::remove(d + QStringLiteral("/app.previous2.log"));
    QFile::rename(d + QStringLiteral("/app.previous.log"), d + QStringLiteral("/app.previous2.log"));
    QFile::rename(d + QStringLiteral("/app.log"), d + QStringLiteral("/app.previous.log"));
}

#if defined(__linux__) || defined(__APPLE__)
void crashSignal(int sig) {                      // a small text backtrace; only async-signal-safe calls
    if (g_crashFd >= 0) {
        const char head[] = "kinoz crashed with signal ";
        (void)!write(g_crashFd, head, sizeof head - 1);
        char num[8] = {char('0' + (sig / 10) % 10), char('0' + sig % 10), '\n', 0};
        (void)!write(g_crashFd, num, 3);
        void* fr[48];
        const int n = backtrace(fr, 48);
        backtrace_symbols_fd(fr, n, g_crashFd);
        fsync(g_crashFd);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}
#endif
}

namespace Log {
bool debugEnabled() { return g_debug; }

QString installType() {
#ifdef Q_OS_WIN
    if (QFile::exists(Paths::appDir() + QStringLiteral("/manifest.json"))) return QStringLiteral("installed");
    return QFile::exists(Paths::appDir() + QStringLiteral("/../../CMakeLists.txt")) || QFile::exists(Paths::appDir() + QStringLiteral("/../CMakeLists.txt")) ? QStringLiteral("development build") : QStringLiteral("portable");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("app bundle");
#else
    return qEnvironmentVariableIsSet("APPIMAGE") ? QStringLiteral("AppImage") : QStringLiteral("unpackaged");
#endif
}

QString headerText() {
    return QStringLiteral("kinoz %1 | %2 (%3) | Qt %4 | %5 | data: %6")
        .arg(QStringLiteral(APP_VERSION), QSysInfo::prettyProductName(), QSysInfo::currentCpuArchitecture(), QString::fromLatin1(qVersion()), installType(), Paths::dataDir());
}

void init(bool debug) {
    g_debug = debug || qgetenv("KINOZ_LOG") == "debug";
    QDir().mkpath(Paths::logsDir());
    rotate();
    {
        QMutexLocker lock(&g_mutex);
        auto* f = new QFile(Paths::logsDir() + QStringLiteral("/app.log"));
        if (f->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) g_file = f; else delete f;
    }
    if (g_file) qInstallMessageHandler(handler);
    qInfo().noquote() << "[app]" << headerText();
    if (g_debug) qInfo().noquote() << "[app] verbose logging is on";
#if defined(__linux__) || defined(__APPLE__)
    g_crashFd = open((Paths::logsDir() + QStringLiteral("/crash.txt")).toLocal8Bit().constData(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
    for (int s : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL}) signal(s, crashSignal);
#endif
}

void shutdown() {
    qInstallMessageHandler(nullptr);
    QMutexLocker lock(&g_mutex);
    if (g_file) { g_file->close(); delete g_file; g_file = nullptr; }
#if defined(__linux__) || defined(__APPLE__)
    if (g_crashFd >= 0) {                        // a clean exit leaves no crash file behind
        const QString p = Paths::logsDir() + QStringLiteral("/crash.txt");
        ::close(g_crashFd); g_crashFd = -1;
        if (QFileInfo(p).size() == 0) QFile::remove(p);
    }
#endif
}

void info(const char* a, const QString& m) { qInfo().noquote() << QStringLiteral("[%1]").arg(QLatin1String(a)) << m; }
void warn(const char* a, const QString& m) { qWarning().noquote() << QStringLiteral("[%1]").arg(QLatin1String(a)) << m; }
void error(const char* a, const QString& m) { qCritical().noquote() << QStringLiteral("[%1]").arg(QLatin1String(a)) << m; }
void debug(const char* a, const QString& m) { if (g_debug) qDebug().noquote() << QStringLiteral("[%1]").arg(QLatin1String(a)) << m; }

QStringList tail(int lines) {
    QFile f(Paths::logsDir() + QStringLiteral("/app.log"));
    if (!f.open(QIODevice::ReadOnly)) return {};
    const qint64 size = f.size();
    f.seek(qMax<qint64>(0, size - 24 * 1024));
    const QStringList all = QString::fromUtf8(f.readAll()).split('\n', Qt::SkipEmptyParts);
    return all.mid(qMax(0, all.size() - lines));
}
}
