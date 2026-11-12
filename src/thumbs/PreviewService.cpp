// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "thumbs/PreviewService.h"
#include "thumbs/FrameGrabber.h"
#include <QMetaObject>
#include <cmath>

PreviewService::PreviewService(QObject* parent) : QObject(parent) {
    m_thread = QThread::create([this] { loop(); });
    m_thread->start();
    m_boardThread = QThread::create([this] { boardLoop(); });
    m_boardThread->start(QThread::LowPriority);
}

PreviewService::~PreviewService() {
    { QMutexLocker l(&m_mx); m_quit = true; }
    m_cv.wakeAll();
    m_cvBoard.wakeAll();
    for (QThread* t : {m_thread, m_boardThread}) {
        t->wait(2500);
        if (t->isRunning()) t->terminate();
        delete t;
    }
}

PreviewService::Board* PreviewService::findBoard(const QString& path) {
    for (Board& b : m_boards) if (b.path == path) return &b;
    return nullptr;
}

int PreviewService::boardFrames(const QString& path) const {
    QMutexLocker l(const_cast<QMutex*>(&m_mx));
    for (const Board& b : m_boards) if (b.path == path) return b.done;
    return 0;
}

void PreviewService::request(const QString& path, double sec, int maxW, double duration) {
    QImage quick;
    {
        QMutexLocker l(&m_mx);
        m_path = path; m_sec = sec; m_w = maxW; m_has = true;
        if (duration >= 60) {                                         // short clips decode fast enough on their own
            Board* b = findBoard(path);
            if (!b) {
                if (m_boards.size() >= 4) m_boards.removeFirst();
                m_boards.append(Board{path, duration, QVector<QImage>(kSlots), 0});
                m_boardPath = path; m_boardDur = duration; m_wantBoard = true;
                m_cvBoard.wakeOne();
            } else if (b->done < kSlots && m_boardPath != path) {         // an earlier, interrupted build: resume it
                m_boardPath = path; m_boardDur = duration; m_wantBoard = true;
                m_cvBoard.wakeOne();
            }
            if (b && b->done > 0) {
                const int want = qBound(0, int(sec / b->dur * kSlots), kSlots - 1);
                for (int d = 0; d < kSlots && quick.isNull(); ++d) {   // nearest finished frame, within ~12% of the video
                    if (d > 6) break;
                    for (int s : {want - d, want + d}) if (s >= 0 && s < kSlots && !b->img[s].isNull()) { quick = b->img[s]; break; }
                }
            }
        }
    }
    m_cv.wakeOne();
    if (!quick.isNull()) QMetaObject::invokeMethod(this, [this, quick, sec, path] { emit frame(quick, sec, path); }, Qt::QueuedConnection);
}

void PreviewService::cancel() { QMutexLocker l(&m_mx); m_has = false; }

void PreviewService::loop() {
    FrameGrabber* g = nullptr;
    for (;;) {
        QString path; double sec; int w;
        {
            QMutexLocker l(&m_mx);
            while (!m_has && !m_quit) {
                if (!m_cv.wait(&m_mx, 15000) && g) { l.unlock(); delete g; g = nullptr; l.relock(); }
            }
            if (m_quit) break;
            path = m_path; sec = m_sec; w = m_w; m_has = false;
        }
        if (!g) g = new FrameGrabber();
        if (!g->ok()) continue;
        if (g->path() != path && !g->open(path)) continue;
        const QImage img = g->grab(sec, w, w);
        if (!img.isNull()) QMetaObject::invokeMethod(this, [this, img, sec, path] { emit frame(img, sec, path); }, Qt::QueuedConnection);
    }
    delete g;
}

// Builds the strip for the video that is being hovered: every 4th slot first, then the in-betweens, so a usable
// strip exists after a second or two. Abandons the job the moment another video is requested.
void PreviewService::boardLoop() {
    FrameGrabber* g = nullptr;
    QVector<int> order;
    for (int phase : {0, 2, 1, 3}) for (int i = phase; i < kSlots; i += 4) order << i;
    for (;;) {
        QString path; double dur;
        {
            QMutexLocker l(&m_mx);
            while (!m_wantBoard && !m_quit) {
                if (!m_cvBoard.wait(&m_mx, 10000) && g) { l.unlock(); delete g; g = nullptr; l.relock(); }
            }
            if (m_quit) break;
            path = m_boardPath; dur = m_boardDur; m_wantBoard = false;
        }
        if (!g) g = new FrameGrabber();
        if (!g->ok() || !g->open(path)) continue;
        for (int i : order) {
            {
                QMutexLocker l(&m_mx);
                if (m_quit || m_wantBoard || m_boardPath != path) break;       // a newer video was asked for
                Board* b = findBoard(path);
                if (!b) break;
                if (!b->img[i].isNull()) continue;                              // already have it (resumed build)
            }
            const QImage img = g->grab((i + 0.5) * dur / kSlots, 240, 240);
            QMutexLocker l(&m_mx);
            if (Board* b = findBoard(path); b && !img.isNull() && b->img[i].isNull()) { b->img[i] = img; ++b->done; }
        }
    }
    delete g;
}
