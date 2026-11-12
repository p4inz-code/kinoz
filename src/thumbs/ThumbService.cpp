// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "thumbs/ThumbService.h"
#include "core/Paths.h"
#include "thumbs/FrameGrabber.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QImageReader>
#include <QMetaObject>
#include <QtConcurrent>
#include <algorithm>

static constexpr int kThumb = 320;

ThumbService::ThumbService(QObject* parent) : QObject(parent) {
    for (int i = 0; i < 2; ++i) {
        QThread* t = QThread::create([this] { workerLoop(); });
        t->setObjectName(QStringLiteral("thumb-worker"));
        t->start(QThread::LowPriority);
        m_threads << t;
    }
}

ThumbService::~ThumbService() {
    { QMutexLocker l(&m_mx); m_quit = true; m_stack.clear(); }
    m_cv.wakeAll();
    for (QThread* t : m_threads) { t->wait(2500); if (t->isRunning()) t->terminate(); delete t; }
}

QString ThumbService::cacheFile(const QString& path) const {
    const QFileInfo fi(path);
    const QByteArray key = path.toUtf8() + '|' + QByteArray::number(fi.lastModified().toSecsSinceEpoch()) + '|' + QByteArray::number(fi.size()) + "|v1";
    return Paths::thumbsDir() + '/' + QString::fromLatin1(QCryptographicHash::hash(key, QCryptographicHash::Sha1).toHex()) + QStringLiteral(".jpg");
}

QPixmap ThumbService::get(const QString& path, MediaKind kind) {
    if (QPixmap* p = m_mem.object(path)) return *p;
    if (m_failed.contains(path) || m_inflight.contains(path)) return {};
    m_inflight.insert(path);
    {
        QMutexLocker l(&m_mx);
        m_stack.append({path, kind});
        while (m_stack.size() > 160) { m_inflight.remove(m_stack.first().path); m_stack.removeFirst(); }   // drop oldest (scrolled past)
    }
    m_cv.wakeOne();
    return {};
}

void ThumbService::workerLoop() {
    FrameGrabber* grabber = nullptr;
    for (;;) {
        Job j;
        {
            QMutexLocker l(&m_mx);
            while (m_stack.isEmpty() && !m_quit) {
                if (!m_cv.wait(&m_mx, 20000) && grabber) { l.unlock(); delete grabber; grabber = nullptr; l.relock(); }   // free the decoder when idle
            }
            if (m_quit) break;
            j = m_stack.takeLast();
        }
        const QImage img = make(j, grabber);
        QMetaObject::invokeMethod(this, [this, j, img] {
            m_inflight.remove(j.path);
            if (img.isNull()) { m_failed.insert(j.path); emit ready(j.path); return; }
            m_mem.insert(j.path, new QPixmap(QPixmap::fromImage(img)));
            emit ready(j.path);
        }, Qt::QueuedConnection);
    }
    delete grabber;
}

QImage ThumbService::make(const Job& j, FrameGrabber*& grabber) {
    const QString cf = cacheFile(j.path);
    if (QFileInfo::exists(cf)) { QImage c(cf); if (!c.isNull()) return c; }
    QImage img;
    const QString suf = QFileInfo(j.path).suffix().toLower();
    static const QSet<QString> qtFmt{"jpg", "jpeg", "png", "bmp", "gif", "ico", "jfif"};
    if (Media::isPicture(j.kind) && qtFmt.contains(suf)) {
        QImageReader r(j.path);
        r.setAutoTransform(true);
        const QSize s = r.size();
        if (s.isValid()) r.setScaledSize(s.scaled(kThumb, kThumb, Qt::KeepAspectRatio));
        img = r.read();
    }
    if (img.isNull()) {
        if (!grabber) grabber = new FrameGrabber();
        if (grabber->ok() && grabber->open(j.path)) img = grabber->grab(-1, kThumb, kThumb);
    }
    if (!img.isNull()) img.save(cf, "JPG", 82);
    return img;
}

void ThumbService::trimDiskCache(qint64 maxBytes) {
    (void)QtConcurrent::run([maxBytes] {
        QDir d(Paths::thumbsDir());
        auto list = d.entryInfoList({QStringLiteral("*.jpg")}, QDir::Files);
        qint64 total = 0;
        for (const auto& f : list) total += f.size();
        if (total <= maxBytes) return;
        std::sort(list.begin(), list.end(), [](const QFileInfo& a, const QFileInfo& b) { return a.lastModified() < b.lastModified(); });
        for (const auto& f : list) { if (total <= maxBytes * 8 / 10) break; total -= f.size(); QFile::remove(f.absoluteFilePath()); }
    });
}

