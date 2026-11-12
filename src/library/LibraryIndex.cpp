// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "library/LibraryIndex.h"
#include <QDirIterator>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <algorithm>

LibraryIndex::LibraryIndex(QObject* parent) : QObject(parent) {}

void LibraryIndex::rebuild(const QStringList& roots) {
    const quint64 gen = ++m_gen;
    m_busy = true;
    auto* w = new QFutureWatcher<QVector<Entry>>(this);
    connect(w, &QFutureWatcher<QVector<Entry>>::finished, this, [this, w, gen] {
        if (gen == m_gen) { m_entries = w->result(); m_busy = false; emit updated(); }
        w->deleteLater();
    });
    w->setFuture(QtConcurrent::run([roots, gen, this]() {
        QVector<Entry> out;
        for (const QString& r : roots) {
            QDirIterator it(r, QDir::Files | QDir::Readable, QDirIterator::Subdirectories);
            while (it.hasNext() && out.size() < 300000) {
                if (gen != m_gen) return out;
                const QString p = it.next();
                const MediaKind k = Media::kindFor(p);
                if (k == MediaKind::Unknown) continue;
                const QString n = QFileInfo(p).completeBaseName();
                out.push_back({p, n, (n + ' ' + QFileInfo(p).dir().dirName()).toLower(), k});
            }
        }
        return out;
    }));
}

QVector<LibraryIndex::Entry> LibraryIndex::search(const QString& query, int limit) const {
    const QStringList toks = query.toLower().split(' ', Qt::SkipEmptyParts);
    if (toks.isEmpty()) return {};
    struct Hit { int score; int idx; };
    QVector<Hit> hits;
    for (int i = 0; i < m_entries.size(); ++i) {
        const Entry& e = m_entries[i];
        int score = 0; bool ok = true;
        for (const QString& t : toks) {
            const int pos = e.lower.indexOf(t);
            if (pos < 0) { ok = false; break; }
            score += pos == 0 ? 0 : (e.lower[pos - 1] == ' ' || e.lower[pos - 1] == '.' || e.lower[pos - 1] == '_' || e.lower[pos - 1] == '-') ? 2 : 6 + pos / 8;
        }
        if (ok) hits.push_back({score + int(e.name.size() / 12), i});
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.score < b.score; });
    QVector<Entry> out;
    for (int i = 0; i < qMin<int>(limit, int(hits.size())); ++i) out.push_back(m_entries[hits[i].idx]);
    return out;
}


