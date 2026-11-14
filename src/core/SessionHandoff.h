// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QString>
#include <QStringList>

class JsonStore;

// "Come back to where you were" after an update restart. Just before the update starts, kinoz writes what was open (file,
// second, paused or playing, the queue). The new version reads it once at its first start and erases it. It is only used when
// it is fresh (15 minutes) and belongs to the version that was installed, and it is never written in Private mode or when
// "Clean identity when kinoz closes" is on. Clean identity and `kinoz --wipe` remove it too.
namespace Handoff {
struct Session {
    QString path;
    double pos = 0;
    bool paused = false;
    QStringList queue;
    QString version;          // the version being installed
    qint64 at = 0;            // when it was written (seconds since 1970)
    bool valid() const { return !path.isEmpty(); }
};

void write(JsonStore* settings, const Session& s);
// Reads and ERASES the record. Empty (not valid) when there is none, it is older than maxAgeSecs, or it is for another version.
Session take(JsonStore* settings, const QString& runningVersion, qint64 nowSecs, qint64 maxAgeSecs = 15 * 60);
void erase(JsonStore* settings);
}
