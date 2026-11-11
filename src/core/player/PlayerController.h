#pragma once
#include "media/MediaKind.h"
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
    void seekFrac(double f, bool exact);
    void seekBy(double sec);
    void setShuffle(bool s);
    void cycleRepeat();
    void setSpeed(double s);
    void speedStep(int dir);
    void setVolume(double v);
    void volumeStep(int dir);
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

private slots:
    void onEnded(bool eof, bool err, const QString& reason);
    void onEof();
    void onFileLoaded();
    void onPause(bool);

private:
    void load(const QString& path);
    void buildQueueFromSiblings(const QString& path, quint64 gen);
    void addRecent(const QString& p);
    QString key(const QString& p) const;
    int pickNext(int dir);

    MpvPlayer* m_mpv;
    JsonStore* m_set;
    JsonStore* m_resume;
    QStringListModel m_queue;
    QString m_path;
    MediaKind m_kind = MediaKind::Unknown;
    bool m_shuffle = false;
    Repeat m_repeat = RepeatOff;
    quint64 m_gen = 0;
    QTimer m_resumeTimer;
    QStringList m_history;
    int m_failStreak = 0;
    QHash<QString, int> m_color;
    double m_zoom = 0, m_panX = 0, m_panY = 0;
    int m_rot = 0;
};
