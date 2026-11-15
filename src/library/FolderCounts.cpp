// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "library/FolderCounts.h"
#include "core/Log.h"
#include "core/Paths.h"
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPointer>
#include <QSaveFile>
#include <QtConcurrent>

FolderCounts::FolderCounts(QObject* parent) : QObject(parent) {
    m_pool.setMaxThreadCount(1);
    m_saveTimer.setSingleShot(true); m_saveTimer.setInterval(2500);
    connect(&m_saveTimer, &QTimer::timeout, this, &FolderCounts::save);
    QFile f(file());
    if (f.open(QIODevice::ReadOnly) && f.size() < 8 * 1024 * 1024) {
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        const qint64 now = QDateTime::currentSecsSinceEpoch();
        for (auto it = o.begin(); it != o.end(); ++it) {
            const QJsonArray a = it.value().toArray();
            if (a.size() < 7) continue;
            Entry e; e.at = qint64(a[0].toDouble());
            if (now - e.at > 7 * 24 * 3600) continue;                                  // old numbers are not worth showing
            e.c.videos = a[1].toInt(); e.c.gifs = a[2].toInt(); e.c.pictures = a[3].toInt(); e.c.music = a[4].toInt(); e.c.folders = a[5].toInt(); e.c.partial = a[6].toBool();
            m_cache.insert(it.key(), e);
        }
    }
}

FolderCounts::~FolderCounts() { m_quit = true; m_pool.waitForDone(3000); }

QString FolderCounts::key(const QString& dir) {
    const QString k = QDir::cleanPath(dir);
#ifdef Q_OS_WIN
    return k.toLower();
#else
    return k;
#endif
}

QString FolderCounts::file() const { return Paths::cacheDir() + QStringLiteral("/foldercounts.json"); }

FolderScan::Counts FolderCounts::get(const QString& dir, bool* known) {
    const QString k = key(dir);
    const auto it = m_cache.constFind(k);
    if (known) *known = it != m_cache.constEnd();
    if (it == m_cache.constEnd() || QDateTime::currentSecsSinceEpoch() - it->at > 600) request(dir);       // fresh number comes later via ready()
    return it == m_cache.constEnd() ? FolderScan::Counts{} : it->c;
}

QString FolderCounts::text(const QString& dir) {
    bool known = false;
    const FolderScan::Counts c = get(dir, &known);
    return known ? c.text() : QString();
}

void FolderCounts::request(const QString& dir) {
    const QString k = key(dir);
    if (m_inflight.contains(k) || m_quit) return;
    m_inflight.insert(k);
    QPointer<FolderCounts> self(this);
    (void)QtConcurrent::run(&m_pool, [self, dir, k] {
        if (!self || self->m_quit) return;
        const FolderScan::Counts c = FolderScan::count(dir, FolderScan::kMaxFiles, 2500, &self->m_quit);
        if (!self) return;
        QMetaObject::invokeMethod(self.data(), [self, dir, k, c] {
            if (!self) return;
            self->m_inflight.remove(k);
            Entry e; e.c = c; e.at = QDateTime::currentSecsSinceEpoch();
            self->m_cache.insert(k, e);
            if (!self->m_private) self->m_saveTimer.start();
            emit self->ready(dir);
        }, Qt::QueuedConnection);
    });
}

void FolderCounts::save() {
    if (m_private) return;
    QJsonObject o;
    int n = 0;
    for (auto it = m_cache.begin(); it != m_cache.end() && n < 500; ++it, ++n)
        o.insert(it.key(), QJsonArray{double(it->at), it->c.videos, it->c.gifs, it->c.pictures, it->c.music, it->c.folders, it->c.partial});
    QDir().mkpath(Paths::cacheDir());
    QSaveFile f(file());
    if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(o).toJson(QJsonDocument::Compact)); f.commit(); }
}

void FolderCounts::clear() {
    m_cache.clear();
    QFile::remove(file());
}
