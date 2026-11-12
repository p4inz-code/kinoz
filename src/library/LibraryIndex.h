// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include "media/MediaKind.h"
#include <QObject>
#include <QStringList>
#include <QVector>
#include <atomic>

// In-memory name index of every media file under the user's "places", used by the Ctrl+K palette.
class LibraryIndex : public QObject {
    Q_OBJECT
public:
    struct Entry { QString path, name, lower; MediaKind kind; };
    explicit LibraryIndex(QObject* parent = nullptr);
    void rebuild(const QStringList& roots);
    QVector<Entry> search(const QString& query, int limit = 40) const;
    int count() const { return m_entries.size(); }
    bool busy() const { return m_busy; }
signals:
    void updated();
private:
    QVector<Entry> m_entries;
    std::atomic<quint64> m_gen{0};
    bool m_busy = false;
};
