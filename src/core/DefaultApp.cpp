// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/DefaultApp.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QObject>
#include <QProcess>
#include <QUrl>

#include "installer/Installer.h"

namespace DefaultApp {
Result apply() {
#if defined(Q_OS_WIN)
    Installer::registerDefaultApp(QCoreApplication::applicationFilePath());
    if (QDesktopServices::openUrl(QUrl(QStringLiteral("ms-settings:defaultapps?registeredAppUser=kinoz"))))
        return {true, QObject::tr("Windows opened. Pick kinoz, then choose “Set default”.")};
    return {false, QObject::tr("Couldn't open Windows Settings. Open Settings > Apps > Default apps and search for kinoz.")};
#elif defined(Q_OS_LINUX)
    const QStringList types = {"video/mp4", "video/x-matroska", "video/webm", "video/quicktime", "video/x-msvideo", "video/mpeg", "audio/mpeg", "audio/flac",
                               "audio/x-wav", "audio/ogg", "audio/mp4", "audio/x-opus+ogg", "image/png", "image/jpeg", "image/gif", "image/webp"};
    QStringList args = {QStringLiteral("default"), QStringLiteral("kinoz.desktop")};
    args << types;
    if (QProcess::execute(QStringLiteral("xdg-mime"), args) == 0) return {true, QObject::tr("kinoz is now your default player.")};
    return {false, QObject::tr("Couldn't set it. Use your desktop's Default Applications settings.")};
#else
    return {false, QObject::tr("Select a video in Finder, press Cmd+I, set “Open with” to kinoz, then click Change All.")};
#endif
}
}
