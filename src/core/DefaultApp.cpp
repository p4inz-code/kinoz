// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/DefaultApp.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QPixmap>
#include <QStandardPaths>
#include <QDesktopServices>
#include <QObject>
#include <QProcess>
#include <QSaveFile>
#include <QUrl>

#include "core/Log.h"
#include "installer/Installer.h"

namespace DefaultApp {
#if defined(Q_OS_LINUX)
static QStringList kLinuxTypes() {
    return {"video/mp4", "video/x-matroska", "video/webm", "video/quicktime", "video/x-msvideo", "video/mpeg", "video/ogg", "video/x-flv", "video/3gpp",
            "audio/mpeg", "audio/flac", "audio/x-wav", "audio/ogg", "audio/mp4", "audio/aac", "audio/x-opus+ogg", "audio/x-vorbis+ogg", "audio/x-m4a",
            "image/png", "image/jpeg", "image/gif", "image/webp", "image/bmp", "image/avif"};
}
#endif
Result apply() {
#if defined(Q_OS_WIN)
    Installer::registerDefaultApp(QCoreApplication::applicationFilePath());
    if (QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:defaultapps?registeredAppUser=kinoz"))))
        return {true, QObject::tr("Windows opened. Pick kinoz, then choose “Set default”.")};
    return {false, QObject::tr("Couldn't open Windows Settings. Open Settings > Apps > Default apps and search for kinoz.")};
#elif defined(Q_OS_LINUX)
    const QStringList types = kLinuxTypes();
    // An AppImage has no installed .desktop file, so the desktop can't offer it as a handler. Write the small user-level
    // entry (and icon) that a package would have installed; it points at the AppImage file itself and touches nothing else.
    const QByteArray appImage = qgetenv("APPIMAGE");
    if (!appImage.isEmpty()) {
        const QString apps = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/applications");
        const QString icons = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/icons/hicolor/256x256/apps");
        QDir().mkpath(apps); QDir().mkpath(icons);
        QGuiApplication::windowIcon().pixmap(256, 256).save(icons + QStringLiteral("/kinoz.png"), "PNG");
        QSaveFile f(apps + QStringLiteral("/kinoz.desktop"));          // all or nothing: never a half-written launcher entry
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QString exec = QString::fromLocal8Bit(appImage);
            exec.replace(QLatin1Char('\\'), QStringLiteral("\\\\")).replace(QLatin1Char('"'), QStringLiteral("\\\"")).replace(QLatin1Char('%'), QStringLiteral("%%"));
            const QString body = QStringLiteral("[Desktop Entry]\nType=Application\nName=kinoz\nGenericName=Media player\n"
                                                "Comment=A calm, fast, offline player for videos, music, images and GIFs\n"
                                                "Exec=\"") + exec + QStringLiteral("\" %F\nIcon=kinoz\nTerminal=false\nStartupWMClass=kinoz\n"
                                                "Categories=AudioVideo;Audio;Video;Player;\nMimeType=") + types.join(QLatin1Char(';')) + QStringLiteral(";\n");
            f.write(body.toUtf8());
            if (!f.commit()) Log::warn("default-app", QStringLiteral("couldn't save the launcher entry: %1").arg(f.errorString()));
        } else Log::warn("default-app", QStringLiteral("can't write the launcher entry: %1").arg(f.errorString()));
        QProcess::execute(QStringLiteral("update-desktop-database"), {apps});      // harmless if the tool is missing
    }
    QStringList args = {QStringLiteral("default"), QStringLiteral("kinoz.desktop")};
    args << types;
    const int rc = QProcess::execute(QStringLiteral("xdg-mime"), args);
    Log::info("default-app", QStringLiteral("xdg-mime finished with code %1").arg(rc));
    if (rc == 0) return {true, QObject::tr("kinoz is now your default player.")};
    return {false, QObject::tr("Couldn't set it. Use your desktop's Default Applications settings.")};
#else
    return {false, QObject::tr("Select a video in Finder, press Cmd+I, set “Open with” to kinoz, then click Change All.")};
#endif
}
}
