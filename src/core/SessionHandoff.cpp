// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/SessionHandoff.h"
#include "core/JsonStore.h"
#include <QJsonArray>
#include <QJsonObject>

namespace Handoff {
static const char* kKey = "afterUpdate";

void write(JsonStore* settings, const Session& s) {
    QJsonObject o;
    o["path"] = s.path;
    o["pos"] = s.pos;
    o["paused"] = s.paused;
    o["version"] = s.version;
    o["at"] = double(s.at);
    QJsonArray q;
    for (const QString& p : s.queue.mid(0, 300)) q.append(p);              // a long queue is cut: the folder is rebuilt around the file anyway
    o["queue"] = q;
    settings->set(kKey, o);
}

Session take(JsonStore* settings, const QString& runningVersion, qint64 nowSecs, qint64 maxAgeSecs) {
    Session s;
    if (!settings->has(kKey)) return s;
    const QJsonObject o = settings->get(kKey).toObject();
    settings->remove(kKey);                                                // read once, whatever happens next
    const qint64 at = qint64(o.value("at").toDouble());
    if (o.value("version").toString() != runningVersion) return s;         // not the version that was installed (a failed update, a downgrade ...)
    if (at <= 0 || nowSecs - at < 0 || nowSecs - at > maxAgeSecs) return s;
    s.path = o.value("path").toString();
    s.pos = o.value("pos").toDouble();
    s.paused = o.value("paused").toBool();
    s.version = o.value("version").toString();
    s.at = at;
    for (const auto& v : o.value("queue").toArray()) s.queue << v.toString();
    return s;
}

void erase(JsonStore* settings) { settings->remove(kKey); }
}
