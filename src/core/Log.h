// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QString>
#include <QStringList>

// The app log. One file per run (logs/app.log), the last two runs are kept as app.previous.log and app.previous2.log so a
// crash can still be explained after kinoz was reopened. Every line has a time, a level and an area tag:
//     12:03:41.512 I [updater] check: HTTP 200, latest 0.3.0
// Use the helpers below; plain qWarning() / qInfo() lines are picked up too. Nothing here ever records the names of the
// user's media files unless the caller passes them in (callers keep to the file's extension and the engine's error text).
namespace Log {
void init(bool debug);                         // call once, early; rotates the old logs and writes the header
void shutdown();                               // call before the log file goes away
bool debugEnabled();                           // --debug or KINOZ_LOG=debug
void info(const char* area, const QString& msg);
void warn(const char* area, const QString& msg);
void error(const char* area, const QString& msg);
void debug(const char* area, const QString& msg);   // only written when debugEnabled()
QString headerText();                          // version, system, install type, data folder (for the log and for diagnostics)
QStringList tail(int lines);                   // the last lines of the current log, newest last
QString installType();                         // "installed", "portable", "AppImage", "app bundle" or "development build"
}
