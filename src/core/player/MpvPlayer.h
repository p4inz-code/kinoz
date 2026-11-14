// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QObject>
#include <QVariant>
#include <QStringList>
#include <atomic>
#include <mpv/client.h>

// The ONLY class that talks to libmpv's client API. All calls are async; state is cached.
class MpvPlayer : public QObject {
    Q_OBJECT
public:
    explicit MpvPlayer(QObject* parent = nullptr);
    ~MpvPlayer() override;

    void initAsync(bool hwdec);          // create + initialise on a worker thread; emits ready()
    bool isReady() const { return m_ready; }
    mpv_handle* handle() const { return m_h; }

    // ---- commands (never block the UI) ----
    void command(const QStringList& args);
    bool commandSync(const QStringList& args);           // blocks briefly; for explicit user actions only
    void setProp(const char* name, double v);
    void setProp(const char* name, bool v);
    void setProp(const char* name, const QString& v);
    void setPropInt(const char* name, qint64 v);
    QVariant getProp(const char* name) const;            // synchronous: use off hot paths only
    void loadFile(const QString& path, double startSec = -1, bool loopFile = false, bool startPaused = false);
    void appendFile(const QString& path);
    void stopPlayback();
    void seek(double sec, bool exact);                   // coalesced: at most one seek in flight
    void seekRelative(double sec);
    void setPause(bool p) { setProp("pause", p); }
    void setVolume(double v) { m_volume = v; setProp("volume", v); }
    void setMute(bool m) { setProp("mute", m); }
    void setSpeed(double s) { m_speed = s; setProp("speed", s); }
    void frameStep(bool back);

    // ---- cached state ----
    double position() const { return m_pos; }
    double duration() const { return m_dur; }
    bool paused() const { return m_paused; }
    bool eof() const { return m_eof; }
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    double speed() const { return m_speed; }
    int videoW() const { return m_w; }
    int videoH() const { return m_h2; }
    int dispW() const { return m_dw; }
    int dispH() const { return m_dh; }
    bool hasVideo() const { return m_hasVideo; }
    bool hasCover() const { return m_hasCover; }
    QVariantMap metadata() const { return m_meta; }
    QVariantList tracks() const { return m_tracks; }
    QString mediaTitle() const { return m_mediaTitle; }
    QString currentPath() const { return m_path; }

signals:
    void ready();
    void fileStarted(const QString& path);
    void fileLoaded();
    void fileEnded(bool eof, bool error, const QString& reason);
    void initFailed();
    void positionChanged(double);
    void durationChanged(double);
    void pauseChanged(bool);
    void volumeChanged(double);
    void muteChanged(bool);
    void speedChanged(double);
    void metadataChanged();
    void tracksChanged();
    void videoSizeChanged();
    void eofReached();
    void bufferingChanged(bool);
    void seekingDone();
    void logLine(const QString&);

private slots:
    void drain();

private:
    void handleProp(mpv_event_property* ep);
    void sendSeek();
    static void wakeup(void* ctx);

    mpv_handle* m_h = nullptr;
    std::atomic<bool> m_ready{false};
    std::atomic<bool> m_wake{false};
    bool m_destroying = false;

    double m_pos = 0, m_dur = 0, m_volume = 100, m_speed = 1;
    bool m_paused = true, m_muted = false, m_eof = false, m_hasVideo = false, m_hasCover = false;
    int m_w = 0, m_h2 = 0, m_dw = 0, m_dh = 0;
    QVariantMap m_meta;
    QVariantList m_tracks;
    QString m_mediaTitle, m_path;

    bool m_hiRes = false;
    bool m_seekInFlight = false, m_seekPending = false, m_pendingExact = false;
    double m_pendingSeek = 0;
};

