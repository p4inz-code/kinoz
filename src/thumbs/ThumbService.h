// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include "media/MediaKind.h"
#include <QCache>
#include <QMutex>
#include <QObject>
#include <QPixmap>
#include <QSet>
#include <QThread>
#include <QWaitCondition>

class FrameGrabber;

// Async, cancellable thumbnails with an on-disk cache inside ./data. Newest request is served first (LIFO),
// so scrolling a huge folder never waits behind thumbnails you already scrolled past.
class ThumbService : public QObject {
    Q_OBJECT
public:
    explicit ThumbService(QObject* parent = nullptr);
    ~ThumbService() override;
    QPixmap get(const QString& path, MediaKind kind);    // null until ready (then ready() fires)
    bool failed(const QString& path) const { return m_failed.contains(path); }
    void trimDiskCache(qint64 maxBytes = 600ll * 1024 * 1024);

signals:
    void ready(const QString& path);

private:
    struct Job { QString path; MediaKind kind; };
    void workerLoop();
    QImage make(const Job& j, FrameGrabber*& grabber);
    QString cacheFile(const QString& path) const;

    QCache<QString, QPixmap> m_mem{700};
    QSet<QString> m_inflight, m_failed;
    QMutex m_mx;
    QWaitCondition m_cv;
    QList<Job> m_stack;
    bool m_quit = false;
    QVector<QThread*> m_threads;
};
