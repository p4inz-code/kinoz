// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QString>
#include <QVector>

// Turns what people paste into a folder path: quotes, file:///C:/..., %USERPROFILE%\Videos, ~/Videos, either slash style,
// a trailing slash. Several lines give several paths. A typed network share (\\server\share) is allowed: it is the user's own
// choice (playlists from other people still refuse network paths).
namespace PathInput {
struct Item {
    QString path;          // clean absolute path with forward slashes
    QString error;         // empty when the path exists and can be read; otherwise a short, plain sentence
    bool isFile = false;   // the path is a file, not a folder
    bool ok() const { return error.isEmpty(); }
};

QString normalize(const QString& raw);          // one path, cleaned; empty when the text is not a path at all
bool looksLikePath(const QString& text);        // clearly a path (drive letter, \\server, ~, %VAR%, file://, leading /)
QVector<Item> parse(const QString& text);       // one item per non-empty line, each checked against the disk
}
