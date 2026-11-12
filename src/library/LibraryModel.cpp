#include "library/LibraryModel.h"
#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <algorithm>

LibraryModel::LibraryModel(QObject* parent) : QAbstractListModel(parent) {}

QVariant LibraryModel::data(const QModelIndex& idx, int role) const {
    if (!idx.isValid() || idx.row() >= m_shown.size()) return {};
    const Entry& e = m_all[m_shown[idx.row()]];
    switch (role) {
    case Qt::DisplayRole: return e.name;
    case PathRole: return e.path;
    case IsDirRole: return e.dir;
    case IsPlaceRole: return e.place;
    case KindRole: return int(e.kind);
    case SizeRole: return e.size;
    case MtimeRole: return e.mtime;
    }
    return {};
}

void LibraryModel::setPlaces(const QStringList& p) { m_places = p; if (m_folder.isEmpty()) setFolder({}); }

void LibraryModel::setFolder(const QString& dir) {
    const quint64 gen = ++m_gen;
    m_folder = dir;
    m_error.clear();
    if (dir.isEmpty()) {
        beginResetModel();
        m_all.clear();
        for (const QString& p : m_places) {
            Entry e; e.path = p; e.dir = true; e.place = true;
            e.name = QFileInfo(p).fileName().isEmpty() ? p : QFileInfo(p).fileName();
            m_all.push_back(e);
        }
        m_loading = false;
        endResetModel();
        refilter();
        emit loaded();
        return;
    }
    m_loading = true; emit loadingChanged(true);
    auto* w = new QFutureWatcher<QPair<QVector<Entry>, QString>>(this);
    connect(w, &QFutureWatcher<QPair<QVector<Entry>, QString>>::finished, this, [this, w, gen] {
        w->deleteLater();
        if (gen != m_gen) return;
        auto res = w->result();
        beginResetModel();
        m_all = res.first;
        m_error = res.second;
        m_loading = false;
        endResetModel();
        refilter();
        emit loadingChanged(false);
        if (!m_error.isEmpty()) emit failed(m_error);
        emit loaded();
    });
    w->setFuture(QtConcurrent::run([dir]() -> QPair<QVector<Entry>, QString> {
        QDir d(dir);
        if (!d.exists()) return {{}, QObject::tr("This folder no longer exists")};
        const auto list = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::Readable, QDir::NoSort);
        if (list.isEmpty() && !QFileInfo(dir).isReadable()) return {{}, QObject::tr("Can't read this folder (access denied)")};
        QVector<Entry> out;
        out.reserve(list.size());
        for (const QFileInfo& fi : list) {
            Entry e;
            e.name = fi.fileName(); e.path = fi.absoluteFilePath(); e.dir = fi.isDir();
            if (!e.dir) {
                e.kind = Media::kindFor(e.name);
                if (e.kind == MediaKind::Unknown) continue;
                e.size = fi.size(); e.mtime = fi.lastModified().toSecsSinceEpoch();
            } else if (e.name.startsWith('$') || e.name.compare("System Volume Information", Qt::CaseInsensitive) == 0) continue;
            out.push_back(e);
        }
        QCollator col; col.setNumericMode(true); col.setCaseSensitivity(Qt::CaseInsensitive);
        std::sort(out.begin(), out.end(), [&](const Entry& a, const Entry& b) {
            if (a.dir != b.dir) return a.dir;
            return col.compare(a.name, b.name) < 0;
        });
        return {out, QString()};
    }));
}

void LibraryModel::setKindFilter(int k) { m_kind = k; refilter(); }
void LibraryModel::setText(const QString& t) { m_text = t.trimmed(); refilter(); }
void LibraryModel::setSort(int s) { m_sort = s; refilter(); }

void LibraryModel::refilter() {
    beginResetModel();
    m_shown.clear();
    const QString t = m_text.toLower();
    for (int i = 0; i < m_all.size(); ++i) {
        const Entry& e = m_all[i];
        if (!t.isEmpty() && !e.name.toLower().contains(t)) continue;
        if (!e.dir && m_kind) {
            const bool ok = (m_kind == 1 && e.kind == MediaKind::Video) || (m_kind == 2 && e.kind == MediaKind::Audio) || (m_kind == 3 && Media::isPicture(e.kind));
            if (!ok) continue;
        }
        if (!e.dir && m_review && m_state) {
            const int s = m_state(e.path);
            const bool av = e.kind == MediaKind::Video || e.kind == MediaKind::Audio;
            const bool ok = m_review == 1 ? (av && !(s & 4) && !(s & 2)) : m_review == 2 ? (s & 1) != 0 : (s & 2) != 0;
            if (!ok) continue;
        }
        m_shown.push_back(i);
    }
    if (m_sort != 0) {
        std::stable_sort(m_shown.begin(), m_shown.end(), [&](int a, int b) {
            const Entry &x = m_all[a], &y = m_all[b];
            if (x.dir != y.dir) return x.dir;
            if (x.dir) return false;
            return m_sort == 1 ? x.mtime > y.mtime : x.size > y.size;
        });
    }
    endResetModel();
}

QStringList LibraryModel::shownFiles() const {
    QStringList l;
    for (int i : m_shown) if (!m_all[i].dir) l << m_all[i].path;
    return l;
}
QStringList LibraryModel::shownFiles(int family) const {
    QStringList l;
    for (int i : m_shown) if (!m_all[i].dir && (!family || Media::family(m_all[i].kind) == family)) l << m_all[i].path;
    return l;
}
int LibraryModel::fileCount() const { int n = 0; for (const auto& e : m_all) if (!e.dir) ++n; return n; }
