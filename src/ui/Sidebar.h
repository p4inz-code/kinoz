#pragma once
#include "media/MediaKind.h"
#include <QListView>
#include <QStyledItemDelegate>
#include <QTimer>
#include <QWidget>

class LibraryModel;
class PlayerController;
class ThumbService;
class JsonStore;
class Segmented;
class IconButton;
class PopupMenu;
class QLineEdit;

// List view that lets unhandled keys (Space, etc.) reach the global shortcuts.
class MediaView : public QListView {
    Q_OBJECT
public:
    explicit MediaView(QWidget* parent = nullptr);
    void setGrid(bool g);
    bool grid() const { return m_grid; }
signals:
    void activatedIndex(const QModelIndex&);
    void contextRequested(const QModelIndex&, const QPoint& globalPos);
protected:
    void keyPressEvent(QKeyEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
private:
    bool m_grid = false;
};

class MediaDelegate : public QStyledItemDelegate {
public:
    MediaDelegate(PlayerController* c, ThumbService* t, QObject* parent) : QStyledItemDelegate(parent), m_ctl(c), m_thumbs(t) {}
    void paint(QPainter*, const QStyleOptionViewItem&, const QModelIndex&) const override;
    QSize sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const override;
    void setGrid(bool g) { m_grid = g; }
    void setQueueMode(bool q) { m_queue = q; }
private:
    PlayerController* m_ctl;
    ThumbService* m_thumbs;
    bool m_grid = false, m_queue = false;
};

class Sidebar : public QWidget {
    Q_OBJECT
public:
    Sidebar(PlayerController* ctl, ThumbService* thumbs, JsonStore* settings, QWidget* parent = nullptr);
    QStringList places() const;
    void addPlace(const QString& path);
    void removePlace(const QString& path);
    void showQueue(bool q);
    void navigate(const QString& folder);
    bool isCaptionAt(const QPoint& localPos) const { return localPos.y() < 44; }
    void focusSearch();
    int libraryRows() const;
    QString currentFolder() const;
signals:
    void addFolderRequested();
    void placesChanged();
    void openInNewWindow(const QString& path);
    void toast(const QString&);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
private:
    void relayout();
    void onActivated(const QModelIndex& i);
    void onContext(const QModelIndex& i, const QPoint& g);
    void applyGrid(bool g);
    void savePlaces(const QStringList& p);
    QString parentOf(const QString& f) const;

    PlayerController* m_ctl;
    ThumbService* m_thumbs;
    JsonStore* m_set;
    LibraryModel* m_model;
    Segmented* m_tabs;
    Segmented* m_filter;
    IconButton *m_up, *m_viewBtn, *m_sortBtn, *m_addPlace, *m_clearQ;
    QLineEdit* m_search;
    MediaView* m_list;
    MediaView* m_queueView;
    MediaDelegate *m_libDel, *m_queueDel;
    QTimer m_repaint;
    bool m_queueTab = false, m_grid = false;
    int m_sort = 0;
};

