// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QString>
#include <QStringList>

// Saved playlists are plain UTF-8 .m3u8 files in <data>/playlists, so they open in any other player too.
namespace Playlists {
QString dir();
QStringList names();                                           // playlist names, sorted
QString fileFor(const QString& name);
bool exists(const QString& name);
bool save(const QString& name, const QStringList& paths, QString* error = nullptr);
bool remove(const QString& name);
bool rename(const QString& from, const QString& to, QString* error = nullptr);
QStringList load(const QString& file);                         // .m3u / .m3u8 / .pls; relative paths resolve next to the file; missing files are skipped
QStringList tracks(const QString& name);
bool isPlaylistFile(const QString& path);
QString sanitize(const QString& name);                         // safe file name, never empty
QString uniqueName(const QString& base);                       // "Playlist", "Playlist 2", ...
}
