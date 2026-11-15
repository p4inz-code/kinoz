// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include "library/FolderScan.h"
#include <QHash>
#include <QObject>
#include <QSet>
#include <QTimer>
#include <QThreadPool>

// What is inside a folder ("142 videos · 18 GIFs · 6 subfolders"), counted on a background thread one folder at a time and
// remembered (in memory and in cache/foldercounts.json, never in Private mode). The UI asks, gets what is known right away, and
// is told through ready() when a fresh number arrives.
class FolderCounts : public QObject {
    Q_OBJECT
public:
    explicit FolderCounts(QObject* parent = nullptr);
    ~FolderCounts() override;
    QString text(const QString& dir);                       // "" until the first count is in (a request is queued meanwhile)
    FolderScan::Counts get(const QString& dir, bool* known = nullptr);
    void setPrivate(bool on) { m_private = on; }
    void clear();                                           // forget everything, in memory and on disk (Clean identity, --wipe)
signals:
    void ready(const QString& dir);
private:
    struct Entry { FolderScan::Counts c; qint64 at = 0; };
    static QString key(const QString& dir);
    void request(const QString& dir);
    void save();
    QString file() const;

    QHash<QString, Entry> m_cache;
    QSet<QString> m_inflight;
    QThreadPool m_pool;
    QTimer m_saveTimer;
    std::atomic<bool> m_quit{false};
    bool m_private = false;
};
