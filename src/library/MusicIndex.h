#pragma once
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QVector>
#include <atomic>

class QThread;

// Reads the tags (title, artist, album, track number, length) of every audio file under your library folders,
// on a low-priority background thread, and keeps them in <data>/music.json so the next start is instant.
class MusicIndex : public QObject {
    Q_OBJECT
public:
    struct Track {
        QString path, title, artist, album, albumArtist;
        int track = 0, disc = 0, year = 0;
        double dur = 0;
        qint64 size = 0, mtime = 0;
        QString artistKey() const { return albumArtist.isEmpty() ? artist : albumArtist; }
    };

    explicit MusicIndex(QObject* parent = nullptr);
    ~MusicIndex() override;

    void setRoots(const QStringList& roots);        // call whenever the library folders change; rescans in the background
    void ensureScanned();                           // starts the first scan (lazy: only when the Music tab is opened)
    QVector<Track> tracks() const;                  // a copy of what has been read so far
    bool scanning() const { return m_scanning; }
    int total() const { return m_total; }
    int done() const { return m_done; }

    static QString fallbackTitle(const QString& path);

signals:
    void changed();                                 // more tracks are available (throttled)
    void scanningChanged(bool);

private:
    void start();
    void run(quint64 gen, QStringList roots);
    QString cachePath() const;

    mutable QMutex m_mx;
    QVector<Track> m_tracks;
    QStringList m_roots;
    std::atomic<quint64> m_gen{0};
    std::atomic<bool> m_scanning{false}, m_quit{false};
    std::atomic<int> m_total{0}, m_done{0};
    bool m_wanted = false;
    QThread* m_thread = nullptr;
    QVector<QThread*> m_old;                        // superseded scans that are still winding down
};
