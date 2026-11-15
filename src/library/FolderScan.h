// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include "media/MediaKind.h"
#include <QJsonObject>
#include <QStringList>
#include <atomic>
#include <functional>

class JsonStore;

// "A folder means everything inside it." Lists the media in a folder tree for Play all, and counts what is in a folder for the
// pickers. Bounded in files and time, background-friendly (cancellable), never follows symlinks, skips hidden and system folders,
// reads names only (never file contents).
//
// Advance rules (unchanged and not part of this module): videos and songs go on to the next file by themselves, a GIF keeps looping
// until the user presses next, a picture stays until the user moves on.
namespace FolderScan {

struct Options {
    bool subfolders = true;      // include everything below the folder
    bool videos = true, gifs = true, music = true;
    bool pictures = false;       // pictures never advance by themselves, so a binge leaves them out unless asked
    bool shuffle = false;
    bool allows(MediaKind k) const;
    QJsonObject toJson() const;
    static Options fromJson(const QJsonObject& o);
    bool operator==(const Options& o) const { return subfolders == o.subfolders && videos == o.videos && gifs == o.gifs && music == o.music && pictures == o.pictures && shuffle == o.shuffle; }
};

struct Result {
    QStringList files;           // in play order: the files of a folder first, then its subfolders, each in natural name order
    bool truncated = false;      // stopped at the file limit
    bool timedOut = false;
    bool cancelled = false;
};

constexpr int kMaxFiles = 20000;
constexpr int kMaxMs = 30000;

Result scan(const QStringList& roots, const Options& opts, const std::atomic<bool>* cancel = nullptr,
            const std::function<void(int)>& progress = {}, int maxFiles = kMaxFiles, int maxMs = kMaxMs);

struct Counts {
    int videos = 0, gifs = 0, pictures = 0, music = 0;
    int folders = 0;             // direct subfolders
    bool partial = false;        // the count stopped at its limit
    int media() const { return videos + gifs + pictures + music; }
    QString text() const;        // "142 videos · 18 GIFs · 6 subfolders"
};
Counts count(const QString& dir, int maxEntries = kMaxFiles, int maxMs = 3000, const std::atomic<bool>* cancel = nullptr);

// Fisher-Yates on list[keepFirst..]; every entry once, so a shuffled queue plays each file exactly once.
void shuffle(QStringList& list, int keepFirst = 0);

Options optionsFor(JsonStore* settings, const QString& dir);                       // what was chosen for this folder last time (defaults otherwise)
void rememberOptions(JsonStore* settings, const QString& dir, const Options& o);
}
