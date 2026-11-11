#pragma once
#include <QImage>
#include <QString>
#include <mpv/client.h>
#include <mpv/render.h>

// A tiny headless libmpv instance that decodes ONE frame into memory (software render API).
// Used for library thumbnails and the seek-bar hover preview. Not thread-safe: one thread per instance.
class FrameGrabber {
public:
    FrameGrabber();
    ~FrameGrabber();
    bool ok() const { return m_h && m_rc; }
    bool open(const QString& path);                       // blocks up to a few seconds
    QImage grab(double sec, int maxW, int maxH);          // sec < 0 => ~10% into the file
    double duration() const { return m_dur; }
    const QString& path() const { return m_path; }

private:
    bool waitFor(mpv_event_id id, double timeoutSec);
    mpv_handle* m_h = nullptr;
    mpv_render_context* m_rc = nullptr;
    QString m_path;
    double m_dur = 0;
};
