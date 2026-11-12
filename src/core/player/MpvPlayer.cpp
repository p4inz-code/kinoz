// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "core/player/MpvPlayer.h"
#include "core/Paths.h"
#include <QMetaObject>
#include <QThread>
#include <QtConcurrent>
#include <cstring>

namespace {
constexpr uint64_t kSeekReply = 0x5EEC;

QVariant nodeToVariant(const mpv_node* n) {
    switch (n->format) {
    case MPV_FORMAT_STRING: return QString::fromUtf8(n->u.string);
    case MPV_FORMAT_FLAG: return bool(n->u.flag);
    case MPV_FORMAT_INT64: return qlonglong(n->u.int64);
    case MPV_FORMAT_DOUBLE: return n->u.double_;
    case MPV_FORMAT_NODE_ARRAY: {
        QVariantList l;
        for (int i = 0; i < n->u.list->num; ++i) l << nodeToVariant(&n->u.list->values[i]);
        return l;
    }
    case MPV_FORMAT_NODE_MAP: {
        QVariantMap m;
        for (int i = 0; i < n->u.list->num; ++i) m.insert(QString::fromUtf8(n->u.list->keys[i]), nodeToVariant(&n->u.list->values[i]));
        return m;
    }
    default: return {};
    }
}
}

MpvPlayer::MpvPlayer(QObject* parent) : QObject(parent) {}

MpvPlayer::~MpvPlayer() {
    m_destroying = true;
    if (m_h) {
        mpv_set_wakeup_callback(m_h, nullptr, nullptr);
        mpv_handle* h = m_h;
        m_h = nullptr;
        // Tear down off the UI thread so closing the window never waits for decoder shutdown.
        QThread* t = QThread::create([h] { mpv_terminate_destroy(h); });
        t->start();
        t->wait(1500);   // normally returns in a few ms; bounded so a stuck decoder can't hang exit
    }
}

void MpvPlayer::initAsync(bool hwdec) {
    auto fut = QtConcurrent::run([this, hwdec]() -> mpv_handle* {
        mpv_handle* h = mpv_create();
        if (!h) return nullptr;
        auto opt = [&](const char* k, const char* v) { return mpv_set_option_string(h, k, v); };
        opt("config", "no");
        opt("vo", "libmpv");
        opt("idle", "yes");
        opt("keep-open", "yes");
        opt("osc", "no");
        opt("osd-level", "0");
        opt("input-default-bindings", "no");
        opt("input-vo-keyboard", "no");
        opt("input-cursor", "no");
        opt("cursor-autohide", "no");
        opt("terminal", "no");
        opt("msg-level", "all=warn");
        opt("audio-display", "embedded-first");
        opt("image-display-duration", "inf");
        opt("sub-auto", "fuzzy");
        opt("save-position-on-quit", "no");
        opt("demuxer-max-bytes", "64MiB");
        opt("demuxer-max-back-bytes", "16MiB");
        opt("audio-pitch-correction", "yes");
        opt("screenshot-format", "png");
        opt("audio-client-name", APP_NAME);
        // --- hardening: this player only plays local files; never run external tools or scripts ---
        opt("ytdl", "no");
        opt("load-scripts", "no");
        opt("load-unsafe-playlists", "no");
        opt("access-references", "no");
        opt("network-timeout", "10");
        opt("cookies", "no");
        // --- picture quality for high resolutions (4K/8K downscaled to the window) ---
        opt("scale", "spline36"); opt("cscale", "spline36"); opt("dscale", "mitchell");
        opt("correct-downscaling", "yes"); opt("linear-downscaling", "yes"); opt("sigmoid-upscaling", "yes");
        opt("vd-lavc-threads", "4"); opt("vd-lavc-dr", "yes");     // moderate thread count: smooth playback without pegging every core
        if (qEnvironmentVariableIsSet("KINOZ_SILENT")) opt("ao", "null");
        const QByteArray cache = Paths::cacheDir().toUtf8();
        opt("gpu-shader-cache-dir", cache.constData());
        opt("icc-cache-dir", cache.constData());
        const QByteArray tmp = Paths::tmpDir().toUtf8();
        opt("screenshot-directory", tmp.constData());
        if (hwdec) {
            if (opt("hwdec", "auto-copy-safe") < 0) opt("hwdec", "auto-safe");
        }
        if (mpv_initialize(h) < 0) { mpv_destroy(h); return nullptr; }
        mpv_observe_property(h, 0, "time-pos", MPV_FORMAT_DOUBLE);
        mpv_observe_property(h, 0, "duration", MPV_FORMAT_DOUBLE);
        mpv_observe_property(h, 0, "pause", MPV_FORMAT_FLAG);
        mpv_observe_property(h, 0, "volume", MPV_FORMAT_DOUBLE);
        mpv_observe_property(h, 0, "mute", MPV_FORMAT_FLAG);
        mpv_observe_property(h, 0, "speed", MPV_FORMAT_DOUBLE);
        mpv_observe_property(h, 0, "eof-reached", MPV_FORMAT_FLAG);
        mpv_observe_property(h, 0, "paused-for-cache", MPV_FORMAT_FLAG);
        mpv_observe_property(h, 0, "metadata", MPV_FORMAT_NODE);
        mpv_observe_property(h, 0, "track-list", MPV_FORMAT_NODE);
        mpv_observe_property(h, 0, "media-title", MPV_FORMAT_STRING);
        mpv_observe_property(h, 0, "width", MPV_FORMAT_INT64);
        mpv_observe_property(h, 0, "height", MPV_FORMAT_INT64);
        mpv_observe_property(h, 0, "dwidth", MPV_FORMAT_INT64);
        mpv_observe_property(h, 0, "dheight", MPV_FORMAT_INT64);
        mpv_request_log_messages(h, "warn");
        return h;
    });
    auto* w = new QFutureWatcher<mpv_handle*>(this);
    connect(w, &QFutureWatcher<mpv_handle*>::finished, this, [this, w] {
        m_h = w->result();
        w->deleteLater();
        if (!m_h) { emit initFailed(); return; }
        mpv_set_wakeup_callback(m_h, &MpvPlayer::wakeup, this);
        m_ready = true;
        emit ready();
        drain();
    });
    w->setFuture(fut);
}

void MpvPlayer::wakeup(void* ctx) {
    auto* self = static_cast<MpvPlayer*>(ctx);
    if (!self->m_wake.exchange(true))
        QMetaObject::invokeMethod(self, "drain", Qt::QueuedConnection);
}

void MpvPlayer::drain() {
    m_wake = false;
    if (!m_h) return;
    for (int guard = 0; guard < 512; ++guard) {
        mpv_event* ev = mpv_wait_event(m_h, 0);
        if (ev->event_id == MPV_EVENT_NONE) break;
        switch (ev->event_id) {
        case MPV_EVENT_PROPERTY_CHANGE: handleProp(static_cast<mpv_event_property*>(ev->data)); break;
        case MPV_EVENT_START_FILE:
            m_eof = false;
            m_path = QString::fromUtf8(getProp("path").toString().toUtf8());
            emit fileStarted(m_path);
            break;
        case MPV_EVENT_FILE_LOADED: emit fileLoaded(); break;
        case MPV_EVENT_END_FILE: {
            auto* ef = static_cast<mpv_event_end_file*>(ev->data);
            emit fileEnded(ef->reason == MPV_END_FILE_REASON_EOF, ef->reason == MPV_END_FILE_REASON_ERROR,
                           ef->reason == MPV_END_FILE_REASON_ERROR ? QString::fromUtf8(mpv_error_string(ef->error)) : QString());
            break;
        }
        case MPV_EVENT_PLAYBACK_RESTART: emit seekingDone(); break;
        case MPV_EVENT_COMMAND_REPLY:
            if (ev->reply_userdata == kSeekReply) { m_seekInFlight = false; if (m_seekPending) sendSeek(); }
            break;
        case MPV_EVENT_LOG_MESSAGE: {
            auto* lm = static_cast<mpv_event_log_message*>(ev->data);
            emit logLine(QString::fromUtf8(lm->prefix) + QStringLiteral(": ") + QString::fromUtf8(lm->text).trimmed());
            break;
        }
        case MPV_EVENT_SHUTDOWN: return;
        default: break;
        }
    }
}

void MpvPlayer::handleProp(mpv_event_property* ep) {
    const char* n = ep->name;
    auto isD = [&] { return ep->format == MPV_FORMAT_DOUBLE; };
    auto isF = [&] { return ep->format == MPV_FORMAT_FLAG; };
    auto isI = [&] { return ep->format == MPV_FORMAT_INT64; };
    if (!std::strcmp(n, "time-pos")) { m_pos = isD() ? *static_cast<double*>(ep->data) : 0; emit positionChanged(m_pos); }
    else if (!std::strcmp(n, "duration")) { m_dur = isD() ? *static_cast<double*>(ep->data) : 0; emit durationChanged(m_dur); }
    else if (!std::strcmp(n, "pause") && isF()) { m_paused = *static_cast<int*>(ep->data); emit pauseChanged(m_paused); }
    else if (!std::strcmp(n, "volume") && isD()) { m_volume = *static_cast<double*>(ep->data); emit volumeChanged(m_volume); }
    else if (!std::strcmp(n, "mute") && isF()) { m_muted = *static_cast<int*>(ep->data); emit muteChanged(m_muted); }
    else if (!std::strcmp(n, "speed") && isD()) { m_speed = *static_cast<double*>(ep->data); emit speedChanged(m_speed); }
    else if (!std::strcmp(n, "eof-reached")) {
        const bool e = isF() && *static_cast<int*>(ep->data);
        if (e && !m_eof) { m_eof = true; emit eofReached(); } else if (!e) m_eof = false;
    }
    else if (!std::strcmp(n, "paused-for-cache") && isF()) emit bufferingChanged(*static_cast<int*>(ep->data));
    else if (!std::strcmp(n, "metadata")) {
        m_meta.clear();
        if (ep->format == MPV_FORMAT_NODE) {
            const QVariantMap m = nodeToVariant(static_cast<mpv_node*>(ep->data)).toMap();
            for (auto it = m.begin(); it != m.end(); ++it) m_meta.insert(it.key().toLower(), it.value());
        }
        emit metadataChanged();
    }
    else if (!std::strcmp(n, "track-list")) {
        m_tracks.clear();
        if (ep->format == MPV_FORMAT_NODE) m_tracks = nodeToVariant(static_cast<mpv_node*>(ep->data)).toList();
        m_hasVideo = false; m_hasCover = false;
        for (const auto& t : m_tracks) {
            const QVariantMap tm = t.toMap();
            if (tm.value("type") == "video") { if (tm.value("albumart").toBool()) m_hasCover = true; else m_hasVideo = true; }
        }
        emit tracksChanged();
    }
    else if (!std::strcmp(n, "media-title")) { m_mediaTitle = ep->format == MPV_FORMAT_STRING ? QString::fromUtf8(*static_cast<char**>(ep->data)) : QString(); emit metadataChanged(); }
    else if (!std::strcmp(n, "width") && isI()) {
        m_w = int(*static_cast<int64_t*>(ep->data));
        // big frames need a bigger demuxer window to stay smooth; shrink again for small ones
        if (m_w >= 3000 && !m_hiRes) { m_hiRes = true; setProp("demuxer-max-bytes", QStringLiteral("192MiB")); }
        else if (m_w > 0 && m_w < 3000 && m_hiRes) { m_hiRes = false; setProp("demuxer-max-bytes", QStringLiteral("64MiB")); }
        emit videoSizeChanged();
    }
    else if (!std::strcmp(n, "height") && isI()) { m_h2 = int(*static_cast<int64_t*>(ep->data)); emit videoSizeChanged(); }
    else if (!std::strcmp(n, "dwidth") && isI()) { m_dw = int(*static_cast<int64_t*>(ep->data)); emit videoSizeChanged(); }
    else if (!std::strcmp(n, "dheight") && isI()) { m_dh = int(*static_cast<int64_t*>(ep->data)); emit videoSizeChanged(); }
}

void MpvPlayer::command(const QStringList& args) {
    if (!m_h || args.isEmpty()) return;
    QList<QByteArray> bytes;
    for (const auto& a : args) bytes << a.toUtf8();
    std::vector<const char*> argv;
    for (const auto& b : bytes) argv.push_back(b.constData());
    argv.push_back(nullptr);
    mpv_command_async(m_h, 0, argv.data());
}

bool MpvPlayer::commandSync(const QStringList& args) {
    if (!m_h || args.isEmpty()) return false;
    QList<QByteArray> bytes;
    for (const auto& a : args) bytes << a.toUtf8();
    std::vector<const char*> argv;
    for (const auto& b : bytes) argv.push_back(b.constData());
    argv.push_back(nullptr);
    return mpv_command(m_h, argv.data()) >= 0;
}

void MpvPlayer::setProp(const char* name, double v) { if (m_h) mpv_set_property_async(m_h, 0, name, MPV_FORMAT_DOUBLE, &v); }
void MpvPlayer::setProp(const char* name, bool v) { int f = v; if (m_h) mpv_set_property_async(m_h, 0, name, MPV_FORMAT_FLAG, &f); }
void MpvPlayer::setPropInt(const char* name, qint64 v) { int64_t x = v; if (m_h) mpv_set_property_async(m_h, 0, name, MPV_FORMAT_INT64, &x); }
void MpvPlayer::setProp(const char* name, const QString& v) {
    if (!m_h) return;
    const QByteArray b = v.toUtf8();
    const char* s = b.constData();
    mpv_set_property_async(m_h, 0, name, MPV_FORMAT_STRING, &s);
}

QVariant MpvPlayer::getProp(const char* name) const {
    if (!m_h) return {};
    mpv_node node;
    if (mpv_get_property(m_h, name, MPV_FORMAT_NODE, &node) < 0) return {};
    QVariant v = nodeToVariant(&node);
    mpv_free_node_contents(&node);
    return v;
}

void MpvPlayer::loadFile(const QString& path, double startSec, bool loopFile) {
    if (!m_h) return;
    m_pos = 0; m_dur = 0; m_eof = false;
    m_seekPending = false;
    QStringList opts;
    if (startSec > 1) opts << QStringLiteral("start=%1").arg(startSec, 0, 'f', 2);
    opts << QStringLiteral("loop-file=%1").arg(loopFile ? "inf" : "no");
    command({QStringLiteral("loadfile"), path, QStringLiteral("replace"), QStringLiteral("-1"), opts.join(',')});
}

void MpvPlayer::appendFile(const QString& path) { command({QStringLiteral("loadfile"), path, QStringLiteral("append")}); }
void MpvPlayer::stopPlayback() { m_seekPending = false; command({QStringLiteral("stop")}); }

void MpvPlayer::seek(double sec, bool exact) {
    if (!m_h) return;
    m_pendingSeek = qMax(0.0, sec);
    m_pendingExact = exact;
    m_seekPending = true;
    if (!m_seekInFlight) sendSeek();
}

void MpvPlayer::sendSeek() {
    if (!m_h || !m_seekPending) return;
    m_seekPending = false;
    m_seekInFlight = true;
    const QByteArray t = QByteArray::number(m_pendingSeek, 'f', 3);
    const char* mode = m_pendingExact ? "absolute+exact" : "absolute+keyframes";
    const char* argv[] = {"seek", t.constData(), mode, nullptr};
    mpv_command_async(m_h, kSeekReply, argv);
    m_pos = m_pendingSeek;
}

void MpvPlayer::seekRelative(double sec) { seek(m_pos + sec, true); }

void MpvPlayer::frameStep(bool back) { command({back ? QStringLiteral("frame-back-step") : QStringLiteral("frame-step")}); }

