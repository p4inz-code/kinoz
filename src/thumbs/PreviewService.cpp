#include "thumbs/PreviewService.h"
#include "thumbs/FrameGrabber.h"
#include <QMetaObject>

PreviewService::PreviewService(QObject* parent) : QObject(parent) {
    m_thread = QThread::create([this] { loop(); });
    m_thread->start();
}

PreviewService::~PreviewService() {
    { QMutexLocker l(&m_mx); m_quit = true; }
    m_cv.wakeAll();
    m_thread->wait(2500);
    if (m_thread->isRunning()) m_thread->terminate();
    delete m_thread;
}

void PreviewService::request(const QString& path, double sec, int maxW) {
    { QMutexLocker l(&m_mx); m_path = path; m_sec = sec; m_w = maxW; m_has = true; }
    m_cv.wakeOne();
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
