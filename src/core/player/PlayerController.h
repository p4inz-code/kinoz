// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include "media/MediaKind.h"
#include <QElapsedTimer>
#include <QObject>
#include <QStringListModel>
#include <QTimer>

class MpvPlayer;
class JsonStore;

// App-level playback logic: queue, shuffle/repeat, resume, per-kind behaviour, image view, EQ/colour state.
class PlayerController : public QObject {
    Q_OBJECT
public:
    enum Repeat { RepeatOff, RepeatAll, RepeatOne };
    PlayerController(MpvPlayer* mpv, JsonStore* settings, JsonStore* resume, QObject* parent = nullptr);

    MpvPlayer* mpv() const { return m_mpv; }
    QStringListModel* queue() { return &m_queue; }
    QString path() const { return m_path; }
    MediaKind kind() const { return m_kind; }
    int index() const { return m_queue.stringList().indexOf(m_path); }
    bool shuffle() const { return m_shuffle; }
    Repeat repeat() const { return m_repeat; }
    bool hasMedia() const { return !m_path.isEmpty(); }
    bool settled() const { return !m_pending.has && m_mpvPath == m_path && m_fileLoaded; }   // the requested file is open in mpv
    int decodeMode() const;                              // 0 auto (fast start, GPU for big video), 1 always GPU, 2 CPU only
    void applyDecode();                                  // re-apply after the setting changes
    int loadCount() const { return m_loadCount; }        // how many files were really handed to mpv (tests: skip-spam coalescing)
    QString title() const;
    QString artist() const;
    QString album() const;

    // opening
    void open(const QStringList& paths);              // files and/or folders
    void play(const QString& path);                   // play one file (queue untouched if it contains it)
    void playIndex(int i);
    void enqueue(const QStringList& paths);
    void removeAt(int i);
    void clearQueue();
    void setQueue(const QStringList& paths, const QString& playPath);

    // transport
    void togglePause();
    void pause();
    void stop();
    void next();
    void prev();
    void prevFile();                                    // always the previous file (prev() restarts a song/video that is past 3 s)
    void seekFrac(double f, bool exact);
    void seekBy(double sec);
    void setShuffle(bool s);
    void cycleRepeat();
    void setSpeed(double s);
    void speedStep(int dir);
    void setVolume(double v);
    void volumeStep(int dir);
    void setStopAfterCurrent(bool on) { m_stopAfterCurrent = on; }
    bool stopAfterCurrent() const { return m_stopAfterCurrent; }
    void toggleMute();

    // pictures
    void zoomBy(double steps);
    void panByPixels(const QPoint& delta, const QSizeF& viewPx, qreal dpr);
    void resetView();
    void rotate(int deg);
    void fit100();

    // video/audio tools
    void setColor(const QString& prop, int value);   // brightness|contrast|saturation|gamma|hue
    void resetColor();
    int colorValue(const QString& prop) const { return m_color.value(prop, 0); }
    void applyEq();                                   // (re)builds the audio filter chain from settings
    void setSubDelay(double d);
    void setSubScale(double s);
    void selectTrack(const QString& type, int id);
    void loadSubtitleFile(const QString& p);

    // review: mark files while you skim (kept / rejected), and remember what you've already seen
    enum Mark { MarkKeep = 1, MarkReject = 2, MarkSeen = 4 };
    int reviewState(const QString& path) const;        // bit mask of Mark
    double watched(const QString& path) const;         // 0..1, how far you got
    void toggleMark(const QString& path, int mark);    // MarkKeep / MarkReject; choosing one clears the other
    // privacy
    bool privateMode() const;                           // while on: no recent list, no resume points, no watched progress is saved
    void clearRecent();
    void clearResume();
    void clearMarks();                                  // keep, reject and watched progress, for every file
    void forgetFile(const QString& path);               // one file: its resume point, marks and place in the recent list
    int nextUnwatched() const;                          // queue index of the next unseen, not-rejected file, or -1
    QStringList rejected() const;                       // queue files marked as rejected
    void dropFiles(const QStringList& files);           // forget files that were deleted (queue, marks, resume); moves playback on
    void markSeen(const QString& path, double frac);

    // history
    QStringList recent() const;
    void saveResumeNow();

signals:
    void pathChanged();
    void queueChanged();
    void modeChanged();         // shuffle/repeat
    void message(const QString&);
    void error(const QString&);   // shown as a red toast
    void viewChanged();         // zoom/pan/rotate
    void markChanged(const QString& path);   // keep / reject / seen changed

private slots:
    void onEnded(bool eof, bool err, const QString& reason);
    void onEof();
    void onFileLoaded();
    void onPause(bool);

private:
    void load(const QString& path);
    void startLoad();                       // really hands the pending file to mpv
    void warmNext();                        // reads the head/tail of the next file so it opens instantly
    int peekNext() const;                   // next index without side effects (-1 when unknown / shuffle)
    void buildQueueFromSiblings(const QString& path, quint64 gen);
    void addRecent(const QString& p);
    QString key(const QString& p) const;
    int pickNext(int dir);

    MpvPlayer* m_mpv;
    JsonStore* m_set;
    JsonStore* m_resume;
    JsonStore* m_marks = nullptr;
    QStringListModel m_queue;
    QString m_path;                         // what the user asked for (updates instantly, so the UI never waits)
    QString m_mpvPath;                      // what mpv actually has loaded (resume points belong to this one)
    struct Pending { QString path; double start = -1; bool gif = false; bool has = false; } m_pending;
    QString m_warmed;                       // last file whose head/tail we pre-read
    int m_loadCount = 0;
    bool m_hwUp = false;                    // this file already switched to the GPU
    bool m_fileLoaded = false;              // mpv reported the current file as loaded
    QTimer m_loadTimer;                     // trailing edge of the click burst
    QElapsedTimer m_sinceLoad;              // leading edge: a calm click loads immediately
    MediaKind m_kind = MediaKind::Unknown;
    bool m_shuffle = false;
    Repeat m_repeat = RepeatOff;
    quint64 m_gen = 0;
    QTimer m_resumeTimer;
    QStringList m_history;
    int m_failStreak = 0;
    bool m_stopAfterCurrent = false;
    QHash<QString, int> m_color;
    double m_zoom = 0, m_panX = 0, m_panY = 0;
    int m_rot = 0;
};
