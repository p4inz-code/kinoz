// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "library/MusicIndex.h"
#include "core/Paths.h"
#include "media/MediaKind.h"
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QSet>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMetaObject>
#include <QSaveFile>
#include <QThread>
#include <algorithm>
#include <mpv/client.h>

namespace {

// One headless mpv, reused for every file: it only opens the file far enough to read its tags and length.
class TagReader {
public:
    struct Tags { QString title, artist, album, albumArtist; int track = 0, disc = 0, year = 0; double dur = 0; };

    TagReader() {
        m_h = mpv_create();
        if (!m_h) return;
        auto opt = [&](const char* k, const char* v) { mpv_set_option_string(m_h, k, v); };
        opt("config", "no"); opt("vo", "null"); opt("ao", "null"); opt("idle", "yes"); opt("pause", "yes");
        opt("terminal", "no"); opt("msg-level", "all=no"); opt("load-scripts", "no"); opt("ytdl", "no");
        opt("sid", "no"); opt("vid", "no"); opt("audio-display", "no"); opt("access-references", "no");
        opt("demuxer-max-bytes", "2MiB"); opt("demuxer-max-back-bytes", "1MiB"); opt("cache", "no");
        if (mpv_initialize(m_h) < 0) { mpv_destroy(m_h); m_h = nullptr; }
    }
    ~TagReader() { if (m_h) mpv_terminate_destroy(m_h); }
    bool ok() const { return m_h != nullptr; }

    bool read(const QString& path, Tags* out) {
        if (!m_h) return false;
        while (mpv_wait_event(m_h, 0)->event_id != MPV_EVENT_NONE) {}            // leftovers from the previous file
        const QByteArray p = path.toUtf8();
        const char* argv[] = {"loadfile", p.constData(), "replace", nullptr};
        if (mpv_command(m_h, argv) < 0) return false;
        QElapsedTimer t; t.start();
        bool loaded = false;
        while (t.elapsed() < 4000) {
            mpv_event* ev = mpv_wait_event(m_h, 0.05);
            if (ev->event_id == MPV_EVENT_FILE_LOADED) { loaded = true; break; }
            if (ev->event_id == MPV_EVENT_SHUTDOWN) break;
            if (ev->event_id == MPV_EVENT_END_FILE && static_cast<mpv_event_end_file*>(ev->data)->reason == MPV_END_FILE_REASON_ERROR) break;   // this file can't be read
        }
        if (!loaded) return false;
        double d = 0;
        if (mpv_get_property(m_h, "duration", MPV_FORMAT_DOUBLE, &d) >= 0) out->dur = d;
        mpv_node n;
        if (mpv_get_property(m_h, "metadata", MPV_FORMAT_NODE, &n) >= 0) {
            if (n.format == MPV_FORMAT_NODE_MAP && n.u.list) {
                for (int i = 0; i < n.u.list->num; ++i) {
                    if (n.u.list->values[i].format != MPV_FORMAT_STRING) continue;
                    const QString k = QString::fromUtf8(n.u.list->keys[i]).toLower();
                    const QString v = QString::fromUtf8(n.u.list->values[i].u.string).trimmed();
                    if (k == "title") out->title = v;
                    else if (k == "artist") out->artist = v;
                    else if (k == "album") out->album = v;
                    else if (k == "album_artist" || k == "albumartist" || k == "album artist") out->albumArtist = v;
                    else if (k == "track" || k == "tracknumber") out->track = v.section('/', 0, 0).toInt();
                    else if (k == "disc" || k == "discnumber") out->disc = v.section('/', 0, 0).toInt();
                    else if (k == "date" || k == "year" || k == "originaldate") out->year = v.left(4).toInt();
                }
            }
            mpv_free_node_contents(&n);
        }
        return true;
    }
private:
    mpv_handle* m_h = nullptr;
};

QString stripTrackPrefix(QString s) {
    int i = 0;
    while (i < s.size() && s[i].isDigit()) ++i;
    if (i > 0 && i <= 3 && i < s.size() && (s[i] == '.' || s[i] == '-' || s[i] == ' ' || s[i] == '_')) {
        int j = i;
        while (j < s.size() && (s[j] == '.' || s[j] == '-' || s[j] == ' ' || s[j] == '_')) ++j;
        if (j < s.size()) return s.mid(j);
    }
    return s;
}

}  // namespace

QString MusicIndex::fallbackTitle(const QString& path) { return stripTrackPrefix(QFileInfo(path).completeBaseName()); }

MusicIndex::MusicIndex(QObject* parent) : QObject(parent) {}

MusicIndex::~MusicIndex() {
    m_quit = true; ++m_gen;
    if (m_thread) m_old << m_thread;
    for (QThread* t : m_old) { t->wait(4000); if (t->isRunning()) t->terminate(); delete t; }
}

QString MusicIndex::cachePath() const { return Paths::dataDir() + QStringLiteral("/music.json"); }

QVector<MusicIndex::Track> MusicIndex::tracks() const { QMutexLocker l(&m_mx); return m_tracks; }

void MusicIndex::setRoots(const QStringList& roots) {
    if (roots == m_roots) return;
    m_roots = roots;
    if (m_wanted) start();
}

void MusicIndex::clearAll() {
    ++m_gen;                                         // a scan in progress notices and stops without writing anything back
    { QMutexLocker l(&m_mx); m_tracks.clear(); }
    QFile::remove(cachePath());
    m_wanted = false;                                // the next time the Music tab opens, it scans again from scratch
    emit changed();
}

void MusicIndex::ensureScanned() {
    if (m_wanted) return;
    m_wanted = true;
    start();
}

void MusicIndex::start() {
    const quint64 gen = ++m_gen;                         // any scan still running notices and stops (we don't wait for it)
    if (m_thread) { m_old << m_thread; m_thread = nullptr; }
    for (int i = m_old.size() - 1; i >= 0; --i) if (m_old[i]->isFinished()) { delete m_old[i]; m_old.removeAt(i); }
    if (m_quit) return;
    const QStringList roots = m_roots;
    m_thread = QThread::create([this, gen, roots] { run(gen, roots); });
    m_thread->start(QThread::LowPriority);
}

void MusicIndex::run(quint64 gen, QStringList roots) {
    auto alive = [&] { return !m_quit && m_gen == gen; };
    if (!alive()) return;
    m_scanning = true; m_total = 0; m_done = 0;
    QMetaObject::invokeMethod(this, [this] { emit scanningChanged(true); }, Qt::QueuedConnection);

    // 1. what we already know (cache keyed by path; reused when size and mtime still match)
    QHash<QString, Track> cache;
    {
        QFile f(cachePath());
        if (f.open(QIODevice::ReadOnly)) {
            const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
            for (auto it = o.begin(); it != o.end(); ++it) {
                const QJsonArray a = it.value().toArray();
                if (a.size() < 10) continue;
                Track t;
                t.path = it.key(); t.mtime = qint64(a[0].toDouble()); t.size = qint64(a[1].toDouble());
                t.title = a[2].toString(); t.artist = a[3].toString(); t.album = a[4].toString(); t.albumArtist = a[5].toString();
                t.track = a[6].toInt(); t.disc = a[7].toInt(); t.year = a[8].toInt(); t.dur = a[9].toDouble();
                cache.insert(t.path, t);
            }
        }
    }

    // 2. find the audio files (bounded, so pointing a place at a whole drive can't run away)
    QStringList files;
    static const QSet<QString> skip{"$recycle.bin", "system volume information", "node_modules", ".git", "windows", "program files", "program files (x86)"};
    for (const QString& root : roots) {
        if (!alive()) return;
        QDirIterator it(root, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks, QDirIterator::Subdirectories);
        while (it.hasNext() && alive() && files.size() < 40000) {
            it.next();
            const QFileInfo fi = it.fileInfo();
            if (fi.isDir()) continue;
            if (Media::kindForSuffix(fi.suffix().toLower()) != MediaKind::Audio) continue;
            bool bad = false;
            for (const QString& part : fi.absolutePath().toLower().split('/')) if (skip.contains(part)) { bad = true; break; }
            if (!bad) files << fi.absoluteFilePath();
        }
    }
    files.removeDuplicates();
    m_total = int(files.size());

    // 3. known files appear immediately; the rest are read one by one
    QVector<Track> result;
    QStringList todo;
    for (const QString& p : files) {
        const QFileInfo fi(p);
        auto c = cache.constFind(p);
        if (c != cache.constEnd() && c->size == fi.size() && c->mtime == fi.lastModified().toSecsSinceEpoch()) { result << *c; ++m_done; }
        else todo << p;
    }
    { QMutexLocker l(&m_mx); m_tracks = result; }
    QMetaObject::invokeMethod(this, [this] { emit changed(); }, Qt::QueuedConnection);

    auto saveCache = [&] {
        QJsonObject o;
        for (const Track& t : result) o.insert(t.path, QJsonArray{double(t.mtime), double(t.size), t.title, t.artist, t.album, t.albumArtist, t.track, t.disc, t.year, t.dur});
        QSaveFile f(cachePath());
        if (f.open(QIODevice::WriteOnly)) { f.write(QJsonDocument(o).toJson(QJsonDocument::Compact)); f.commit(); }
    };

    if (!todo.isEmpty()) {
        TagReader reader;
        QElapsedTimer pub; pub.start();
        int sinceSave = 0;
        for (const QString& p : todo) {
            if (!alive()) break;
            const QFileInfo fi(p);
            Track t;
            t.path = p; t.size = fi.size(); t.mtime = fi.lastModified().toSecsSinceEpoch();
            TagReader::Tags g;
            const bool ok = reader.ok() && reader.read(p, &g);
            t.title = ok && !g.title.isEmpty() ? g.title : fallbackTitle(p);
            t.artist = ok ? g.artist : QString();
            t.album = ok ? g.album : QString();
            t.albumArtist = ok ? g.albumArtist : QString();
            t.track = ok ? g.track : 0; t.disc = ok ? g.disc : 0; t.year = ok ? g.year : 0; t.dur = ok ? g.dur : 0;
            result << t;
            ++m_done;
            if (pub.elapsed() > 400) {                                         // show progress about twice a second
                pub.restart();
                { QMutexLocker l(&m_mx); m_tracks = result; }
                QMetaObject::invokeMethod(this, [this] { emit changed(); }, Qt::QueuedConnection);
            }
            if (++sinceSave >= 400) { sinceSave = 0; saveCache(); }
            QThread::msleep(1);                                                // stay out of the way of playback
        }
    }
    if (alive()) {
        { QMutexLocker l(&m_mx); m_tracks = result; }
        saveCache();
    }
    if (m_gen != gen) return;                                                   // a newer scan has taken over
    m_scanning = false;
    QMetaObject::invokeMethod(this, [this] { emit changed(); emit scanningChanged(false); }, Qt::QueuedConnection);
}
