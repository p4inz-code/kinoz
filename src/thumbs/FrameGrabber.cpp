// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "thumbs/FrameGrabber.h"
#include "core/player/MpvCompat.h"
#include <QElapsedTimer>
#include <QByteArray>

FrameGrabber::FrameGrabber() {
    m_h = mpv_create();
    if (!m_h) return;
    auto opt = [&](const char* k, const char* v) { mpv_set_option_string(m_h, k, v); };
    opt("config", "no");
    opt("vo", "libmpv");
    opt("ao", "null");
    opt("idle", "yes");
    opt("pause", "yes");
    opt("keep-open", "yes");
    opt("hwdec", "no");
    opt("terminal", "no");
    opt("msg-level", "all=no");
    opt("audio-display", "embedded-first");
    opt("image-display-duration", "inf");
    opt("sid", "no");
    opt("hr-seek", "no");
    opt("demuxer-max-bytes", "12MiB");
    opt("demuxer-max-back-bytes", "4MiB");
    opt("vd-lavc-threads", "2");
    opt("load-scripts", "no");
    if (mpv_initialize(m_h) < 0) { mpv_destroy(m_h); m_h = nullptr; return; }
    mpv_render_param params[] = {{MPV_RENDER_PARAM_API_TYPE, const_cast<char*>(MPV_RENDER_API_TYPE_SW)}, {MPV_RENDER_PARAM_INVALID, nullptr}};
    if (mpv_render_context_create(&m_rc, m_h, params) < 0) m_rc = nullptr;
}

FrameGrabber::~FrameGrabber() {
    if (m_rc) mpv_render_context_free(m_rc);
    if (m_h) mpv_terminate_destroy(m_h);
}

bool FrameGrabber::waitFor(mpv_event_id id, double timeoutSec) {
    QElapsedTimer t; t.start();
    while (t.elapsed() < timeoutSec * 1000) {
        mpv_event* ev = mpv_wait_event(m_h, 0.04);
        if (ev->event_id == id) return true;
        if (ev->event_id == MPV_EVENT_END_FILE || ev->event_id == MPV_EVENT_SHUTDOWN) return false;
    }
    return false;
}

bool FrameGrabber::open(const QString& path, double timeoutSec) {
    if (!ok()) return false;
    m_path = path; m_dur = 0;
    const QByteArray p = path.toUtf8();
    const char* argvNew[] = {"loadfile", p.constData(), "replace", "-1", "pause=yes", nullptr};
    const char* argvOld[] = {"loadfile", p.constData(), "replace", "pause=yes", nullptr};      // libmpv before 0.38 has no index argument
    if (mpv_command(m_h, mpvLoadfileHasIndex(m_h) ? argvNew : argvOld) < 0) return false;
    if (!waitFor(MPV_EVENT_PLAYBACK_RESTART, timeoutSec)) { m_path.clear(); return false; }
    double d = 0;
    if (mpv_get_property(m_h, "duration", MPV_FORMAT_DOUBLE, &d) >= 0) m_dur = d;
    return true;
}

QImage FrameGrabber::grab(double sec, int maxW, int maxH) {
    if (!ok() || m_path.isEmpty()) return {};
    double target = sec;
    if (target < 0) target = m_dur > 20 ? m_dur * 0.10 : 0.0;
    if (m_dur > 0) target = qMin(target, m_dur - 0.5);
    if (target > 0.05) {
        const QByteArray t = QByteArray::number(target, 'f', 3);
        const char* argv[] = {"seek", t.constData(), "absolute+keyframes", nullptr};
        mpv_command(m_h, argv);
        waitFor(MPV_EVENT_PLAYBACK_RESTART, 3.0);
    }
    int64_t dw = 0, dh = 0;
    mpv_get_property(m_h, "dwidth", MPV_FORMAT_INT64, &dw);
    mpv_get_property(m_h, "dheight", MPV_FORMAT_INT64, &dh);
    if (dw <= 0 || dh <= 0) return {};
    const double s = qMin(double(maxW) / dw, double(maxH) / dh);
    const int tw = qMax(2, int(dw * s)) & ~1, th = qMax(2, int(dh * s)) & ~1;
    QImage img(tw, th, QImage::Format_RGB32);
    img.fill(Qt::black);
    int size[2] = {tw, th};
    size_t stride = size_t(img.bytesPerLine());
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_SW_SIZE, size},
        {MPV_RENDER_PARAM_SW_FORMAT, const_cast<char*>("bgr0")},
        {MPV_RENDER_PARAM_SW_STRIDE, &stride},
        {MPV_RENDER_PARAM_SW_POINTER, img.bits()},
        {MPV_RENDER_PARAM_INVALID, nullptr}};
    mpv_render_context_update(m_rc);
    if (mpv_render_context_render(m_rc, params) < 0) return {};
    return img;
}
