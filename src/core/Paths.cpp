// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/Paths.h"
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QStandardPaths>

namespace Paths {
QString appDir() { return QDir::cleanPath(QCoreApplication::applicationDirPath()); }

// Windows: data lives next to the program (./data) so nothing is scattered over C:.
//          If that folder isn't writable (e.g. Program Files) we fall back to %LOCALAPPDATA%.
// macOS / Linux: the standard per-user data location (app bundles and AppImages are read-only).
QString dataDir() {
    static QString cached;
    if (!cached.isEmpty()) return cached;
    const QByteArray override = qgetenv("KINOZ_DATA_DIR");      // tests / custom portable setups
    if (!override.isEmpty()) { cached = QDir::cleanPath(QString::fromLocal8Bit(override)); return cached; }
    QString d;
#ifdef Q_OS_WIN
    d = appDir() + QStringLiteral("/data");
    QDir().mkpath(d);
    QFile probe(d + QStringLiteral("/.w"));
    if (probe.open(QIODevice::WriteOnly)) { probe.close(); probe.remove(); }
    else d = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/") + QStringLiteral(APP_NAME);
#else
    d = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/") + QStringLiteral(APP_NAME);
#endif
    cached = QDir::cleanPath(d);
    return cached;
}
QString cacheDir() { return dataDir() + QStringLiteral("/cache"); }
QString thumbsDir() { return cacheDir() + QStringLiteral("/thumbs"); }
QString tmpDir() { return dataDir() + QStringLiteral("/tmp"); }
QString exportsDir() { return dataDir() + QStringLiteral("/exports"); }
QString logsDir() { return dataDir() + QStringLiteral("/logs"); }
void ensure() {
    for (const QString& d : {dataDir(), cacheDir(), thumbsDir(), tmpDir(), exportsDir(), logsDir()})
        QDir().mkpath(d);
}
}
