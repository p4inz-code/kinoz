#pragma once
#include "media/MediaKind.h"
#include <QObject>
#include <QStringList>
#include <QVector>

// In-memory name index of every media file under the user's "places", used by the Ctrl+K palette.
class LibraryIndex : public QObject {
    Q_OBJECT
public:
    struct Entry { QString path, name, lower; MediaKind kind; };
    explicit LibraryIndex(QObject* parent = nullptr);
    void rebuild(const QStringList& roots);
    QVector<Entry> search(const QString& query, int limit = 40) const;
    int count() const { return m_entries.size(); }
    bool busy() const { return m_busy; }
signals:
    void updated();
private:
    QVector<Entry> m_entries;
    quint64 m_gen = 0;
    bool m_busy = false;
};
