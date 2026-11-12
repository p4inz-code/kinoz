// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/MusicView.h"
#include "core/player/PlayerController.h"
#include "library/Playlists.h"
#include "ui/Overlays.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"
#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QMap>
#include <QPainter>
#include <algorithm>

namespace {
QString fmtDur(double s) {
    if (s <= 0) return {};
    const int t = int(s + 0.5);
    return t >= 3600 ? QStringLiteral("%1:%2:%3").arg(t / 3600).arg(t / 60 % 60, 2, 10, QLatin1Char('0')).arg(t % 60, 2, 10, QLatin1Char('0'))
                     : QStringLiteral("%1:%2").arg(t / 60).arg(t % 60, 2, 10, QLatin1Char('0'));
}
QCollator& collator() { static QCollator c = [] { QCollator x; x.setNumericMode(true); x.setCaseSensitivity(Qt::CaseInsensitive); return x; }(); return c; }
QString artistOf(const MusicIndex::Track& t) { const QString k = t.artistKey(); return k.isEmpty() ? QObject::tr("Unknown artist") : k; }
QString albumOf(const MusicIndex::Track& t) { return t.album.isEmpty() ? QObject::tr("Unknown album") : t.album; }
bool trackLess(const MusicIndex::Track& a, const MusicIndex::Track& b) {
    if (a.disc != b.disc) return a.disc < b.disc;
    if (a.track != b.track) return a.track < b.track;
    return collator().compare(a.title, b.title) < 0;
}
}

MusicView::MusicView(PlayerController* ctl, MusicIndex* idx, QWidget* parent) : QWidget(parent), m_ctl(ctl), m_idx(idx) {
    m_seg = new Segmented({tr("Artists"), tr("Albums"), tr("Songs"), tr("Playlists")}, this);
    m_back = new IconButton(Icon::ChevronLeft, this, 30);
    m_list = new RowList(this);
    m_list->setRowHeight(48);
    connect(m_seg, &Segmented::currentChanged, this, [this](int) { m_artist.clear(); m_album.clear(); m_playlist.clear(); rebuild(); });
    connect(m_back, &QAbstractButton::clicked, this, [this] { goBack(); });
    connect(m_list, &RowList::activated, this, [this](int r) { onActivated(r); });
    m_debounce.setSingleShot(true); m_debounce.setInterval(250);
    connect(&m_debounce, &QTimer::timeout, this, [this] { rebuild(true); });
    connect(m_idx, &MusicIndex::changed, this, [this] { if (isVisible() && m_seg->current() != 3 && !m_debounce.isActive()) m_debounce.start(); });   // scanning updates the lists as it goes
    connect(m_idx, &MusicIndex::scanningChanged, this, [this](bool) { update(); if (isVisible()) rebuild(true); });
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
    rebuild();
}

void MusicView::drill(const QString& artist, const QString& album) { m_playlist.clear(); m_seg->setCurrent(0); m_artist = artist; m_album = album; rebuild(); }

void MusicView::showPlaylists() { m_artist.clear(); m_album.clear(); m_playlist.clear(); m_seg->setCurrent(3); rebuild(); }

void MusicView::resizeEvent(QResizeEvent*) {
    m_seg->setGeometry(12, 0, qMin(width() - 24, m_seg->sizeHint().width()), 30);
    m_back->move(12, 40);
    m_list->setGeometry(0, 78, width(), height() - 78);
}

QString MusicView::trail() const {
    QStringList parts;
    if (!m_playlist.isEmpty()) return m_playlist;
    if (!m_artist.isEmpty() && m_seg->current() == 0) parts << m_artist;
    if (!m_album.isEmpty()) parts << m_album;
    return parts.join(QStringLiteral("  ›  "));
}

void MusicView::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    const bool drilled = !trail().isEmpty();
    p.setFont(Theme::i().font(13, QFont::DemiBold)); p.setPen(c.text);
    const int x = drilled ? 48 : 16, wmax = width() - x - 12;
    QString head = trail();
    if (!drilled) {
        if (m_seg->current() == 3) head = tr("%n playlist(s)", nullptr, Playlists::names().size());
        else head = tr("%n song(s)", nullptr, m_tracks.size());
        if (m_idx->scanning()) head += tr("  ·  reading tags %1 / %2").arg(m_idx->done()).arg(m_idx->total());
    }
    p.drawText(QRectF(x, 40, wmax, 30), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(head, Qt::ElideMiddle, wmax));
}

void MusicView::goBack() {
    if (!m_album.isEmpty()) m_album.clear();
    else if (!m_artist.isEmpty()) m_artist.clear();
    else m_playlist.clear();
    rebuild();
}

void MusicView::rebuild(bool keepScroll) {
    m_tracks = m_idx->tracks();
    QVector<RowList::Row> rows;
    m_playSet.clear();
    const int seg = m_seg->current();
    auto trackRow = [&](const MusicIndex::Track& t, bool showArtist, bool numbered) {
        RowList::Row r;
        r.icon = Icon::Music;
        r.title = (numbered && t.track > 0 ? QStringLiteral("%1   ").arg(t.track, 2, 10, QLatin1Char('0')) : QString()) + t.title;
        r.subtitle = showArtist ? (t.artist.isEmpty() ? albumOf(t) : t.artist + QStringLiteral("  ·  ") + albumOf(t)) : (t.artist.isEmpty() || t.artist == t.albumArtist ? QString() : t.artist);
        r.hint = fmtDur(t.dur);
        r.data = QStringLiteral("t:") + t.path;
        return r;
    };
    auto action = [&](Icon ic, const QString& title, const QString& data) { RowList::Row r; r.icon = ic; r.title = title; r.data = data; rows << r; };

    if (!m_playlist.isEmpty()) {                                                           // inside one playlist
        action(Icon::Play, tr("Play playlist"), QStringLiteral("plplay"));
        action(Icon::Pen, tr("Rename…"), QStringLiteral("plrename"));
        action(Icon::Trash, tr("Delete playlist"), QStringLiteral("pldelete"));
        QHash<QString, const MusicIndex::Track*> byPath;
        for (const auto& t : m_tracks) byPath.insert(t.path, &t);
        const QStringList files = Playlists::tracks(m_playlist);
        rows << RowList::Row{Icon::File, tr("Songs · %1").arg(files.size()), {}, {}, {}, true};
        for (const QString& f : files) {
            m_playSet << f;
            if (auto it = byPath.constFind(f); it != byPath.constEnd()) rows << trackRow(**it, true, false);
            else { RowList::Row r; r.icon = Icon::Music; r.title = MusicIndex::fallbackTitle(f); r.subtitle = QFileInfo(f).dir().dirName(); r.data = QStringLiteral("t:") + f; rows << r; }
        }
    } else if (seg == 3) {                                                                  // playlists
        action(Icon::Plus, tr("Save the current queue as a playlist…"), QStringLiteral("save"));
        for (const QString& n : Playlists::names()) {
            RowList::Row r; r.icon = Icon::Queue; r.title = n; r.subtitle = tr("%n song(s)", nullptr, Playlists::tracks(n).size()); r.data = QStringLiteral("pl:") + n; rows << r;
        }
    } else if (!m_album.isEmpty()) {                                                        // inside one album
        QVector<MusicIndex::Track> ts;
        for (const auto& t : m_tracks) if (albumOf(t) == m_album && (m_artist.isEmpty() || artistOf(t) == m_artist)) ts << t;
        std::sort(ts.begin(), ts.end(), trackLess);
        action(Icon::Play, tr("Play album"), QStringLiteral("playset"));
        for (const auto& t : ts) { m_playSet << t.path; rows << trackRow(t, false, true); }
    } else if (seg == 0 && !m_artist.isEmpty()) {                                           // inside one artist: their albums
        QMap<QString, QPair<int, int>> albums;                                              // album -> (songs, year)
        QVector<MusicIndex::Track> all;
        for (const auto& t : m_tracks) if (artistOf(t) == m_artist) { auto& a = albums[albumOf(t)]; ++a.first; a.second = qMax(a.second, t.year); all << t; }
        std::sort(all.begin(), all.end(), [](const auto& a, const auto& b) { const int c = collator().compare(albumOf(a), albumOf(b)); return c ? c < 0 : trackLess(a, b); });
        for (const auto& t : all) m_playSet << t.path;
        action(Icon::Play, tr("Play all songs"), QStringLiteral("playset"));
        QStringList names = albums.keys();
        std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) { return collator().compare(a, b) < 0; });
        for (const QString& n : names) {
            RowList::Row r; r.icon = Icon::Folder; r.title = n;
            r.subtitle = tr("%n song(s)", nullptr, albums[n].first) + (albums[n].second > 0 ? QStringLiteral("  ·  %1").arg(albums[n].second) : QString());
            r.data = QStringLiteral("al:") + m_artist + QChar(0x1f) + n; rows << r;
        }
    } else if (seg == 0) {                                                                  // all artists
        QMap<QString, QPair<QSet<QString>, int>> artists;
        for (const auto& t : m_tracks) { auto& a = artists[artistOf(t)]; a.first.insert(albumOf(t)); ++a.second; }
        QStringList names = artists.keys();
        std::sort(names.begin(), names.end(), [](const QString& a, const QString& b) { return collator().compare(a, b) < 0; });
        for (const QString& n : names) {
            RowList::Row r; r.icon = Icon::Music; r.title = n;
            r.subtitle = tr("%n album(s)", nullptr, artists[n].first.size()) + QStringLiteral("  ·  ") + tr("%n song(s)", nullptr, artists[n].second);
            r.data = QStringLiteral("a:") + n; rows << r;
        }
    } else if (seg == 1) {                                                                  // all albums
        struct Al { QString artist, album; int n = 0; };
        QMap<QString, Al> albums;
        for (const auto& t : m_tracks) { auto& a = albums[artistOf(t) + QChar(0x1f) + albumOf(t)]; a.artist = artistOf(t); a.album = albumOf(t); ++a.n; }
        QVector<Al> list = albums.values().toVector();
        std::sort(list.begin(), list.end(), [](const Al& a, const Al& b) { const int c = collator().compare(a.album, b.album); return c ? c < 0 : collator().compare(a.artist, b.artist) < 0; });
        for (const Al& a : list) {
            RowList::Row r; r.icon = Icon::Folder; r.title = a.album; r.subtitle = a.artist + QStringLiteral("  ·  ") + tr("%n song(s)", nullptr, a.n); r.data = QStringLiteral("al:") + a.artist + QChar(0x1f) + a.album; rows << r;
        }
    } else {                                                                                // all songs
        QVector<MusicIndex::Track> ts = m_tracks;
        std::sort(ts.begin(), ts.end(), [](const auto& a, const auto& b) { return collator().compare(a.title, b.title) < 0; });
        for (const auto& t : ts) { m_playSet << t.path; rows << trackRow(t, true, false); }
    }
    m_list->setEmptyText(m_idx->scanning() ? tr("Reading your music…") : seg == 3 ? tr("No playlists yet. Save the queue as one.") : tr("No music found. Add the folders where you keep your songs from the Library tab."));
    m_list->setRows(rows, keepScroll);
    m_back->setVisible(!trail().isEmpty());
    update();
}

void MusicView::onActivated(int row) {
    const auto& rows = m_list->rows();
    if (row < 0 || row >= rows.size()) return;
    const QString d = rows[row].data;
    auto playSet = [this](const QString& first) { if (m_playSet.isEmpty()) return; m_ctl->setQueue(m_playSet, first.isEmpty() ? m_playSet.first() : first); };
    if (d.startsWith(QStringLiteral("a:"))) { m_artist = d.mid(2); m_album.clear(); rebuild(); }
    else if (d.startsWith(QStringLiteral("al:"))) {
        const QStringList parts = d.mid(3).split(QChar(0x1f));
        m_artist = parts.value(0); m_album = parts.value(1); rebuild();
    }
    else if (d == QStringLiteral("playset")) playSet({});
    else if (d.startsWith(QStringLiteral("t:"))) {
        const QString path = d.mid(2);
        if (!QFileInfo::exists(path)) { emit toast(tr("That file is gone")); return; }
        playSet(path);
    }
    else if (d == QStringLiteral("save")) emit savePlaylistRequested();
    else if (d.startsWith(QStringLiteral("pl:"))) { m_playlist = d.mid(3); rebuild(); }
    else if (d == QStringLiteral("plplay")) { const QStringList l = Playlists::tracks(m_playlist); if (l.isEmpty()) emit toast(tr("That playlist has no playable files")); else m_ctl->setQueue(l, l.first()); }
    else if (d == QStringLiteral("plrename")) emit renamePlaylistRequested(m_playlist);
    else if (d == QStringLiteral("pldelete")) emit deletePlaylistRequested(m_playlist);
}
