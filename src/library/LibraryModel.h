#pragma once
#include "media/MediaKind.h"
#include <QAbstractListModel>
#include <QVector>

// One folder listing (or the list of "places"), scanned off the UI thread, filtered/sorted in memory.
class LibraryModel : public QAbstractListModel {
    Q_OBJECT
public:
    enum Roles { PathRole = Qt::UserRole + 1, IsDirRole, KindRole, SizeRole, MtimeRole, IsPlaceRole };
    struct Entry { QString name, path; bool dir = false; bool place = false; MediaKind kind = MediaKind::Unknown; qint64 size = -1, mtime = 0; };

    explicit LibraryModel(QObject* parent = nullptr);
    int rowCount(const QModelIndex& = {}) const override { return m_shown.size(); }
    QVariant data(const QModelIndex& idx, int role) const override;

    void setFolder(const QString& dir);              // "" = places root
    void setPlaces(const QStringList& places);
    QString folder() const { return m_folder; }
    void setKindFilter(int k);                       // 0 all, 1 video, 2 audio, 3 pictures
    void setText(const QString& t);
    void setSort(int s);                             // 0 name, 1 newest, 2 largest
    QStringList shownFiles() const;
    QStringList shownFiles(int family) const;
    bool loading() const { return m_loading; }
    QString error() const { return m_error; }
    int fileCount() const;
    void refresh() { setFolder(m_folder); }

signals:
    void loadingChanged(bool);
    void failed(const QString& msg);
    void loaded();

private:
    void refilter();
    QVector<Entry> m_all;
    QVector<int> m_shown;
    QString m_folder, m_text, m_error;
    int m_kind = 0, m_sort = 0;
    bool m_loading = false;
    quint64 m_gen = 0;
    QStringList m_places;
};
