#pragma once
#include <QWidget>

// Shown when nothing is loaded.
class EmptyState : public QWidget {
    Q_OBJECT
public:
    explicit EmptyState(QWidget* parent = nullptr);
    void setRecent(const QStringList& r) { m_recent = r; relayout(); update(); }
signals:
    void openFolderRequested();
    void recentClicked(const QString& path);
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override { m_hover = -1; update(); }
    void resizeEvent(QResizeEvent*) override { relayout(); }
private:
    void relayout();
    QStringList m_recent;
    QVector<QRectF> m_rows;
    QRectF m_btn;
    int m_hover = -1;
};

// Now-playing overlay for audio: text over the cover art mpv renders, or a clean card when there is no art.
class AudioStage : public QWidget {
    Q_OBJECT
public:
    explicit AudioStage(QWidget* parent = nullptr);
    void setInfo(const QString& title, const QString& artist, const QString& album, bool hasCover) {
        m_title = title; m_artist = artist; m_album = album; m_cover = hasCover; update();
    }
protected:
    void paintEvent(QPaintEvent*) override;
    bool hitTest(const QPoint&) const { return false; }
private:
    QString m_title, m_artist, m_album;
    bool m_cover = false;
};
