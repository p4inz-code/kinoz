#pragma once
#include "library/MusicIndex.h"
#include <QTimer>
#include <QWidget>

class PlayerController;
class RowList;
class Segmented;
class IconButton;

// The "Music" tab: Artists / Albums / Songs / Playlists, drilling down into albums and tracks.
class MusicView : public QWidget {
    Q_OBJECT
public:
    MusicView(PlayerController* ctl, MusicIndex* idx, QWidget* parent = nullptr);
    void showPlaylists();                  // jump to the Playlists list (e.g. right after saving one)
    void refresh() { rebuild(true); }
    void drill(const QString& artist, const QString& album);    // jump straight to an artist / album (used for screenshots)
    void keepScrolled() {}
signals:
    void toast(const QString&);
    void savePlaylistRequested();
    void renamePlaylistRequested(const QString& name);
    void deletePlaylistRequested(const QString& name);
protected:
    void resizeEvent(QResizeEvent*) override;
    void paintEvent(QPaintEvent*) override;
private:
    void rebuild(bool keepScroll = false);
    void onActivated(int row);
    void goBack();
    QString trail() const;

    PlayerController* m_ctl;
    MusicIndex* m_idx;
    Segmented* m_seg;
    IconButton* m_back;
    RowList* m_list;
    QTimer m_debounce;
    QString m_artist, m_album, m_playlist;        // drill-down state
    QVector<MusicIndex::Track> m_tracks;          // snapshot the rows were built from
    QStringList m_playSet;                        // the track paths behind the current rows, in order (becomes the queue)
};
