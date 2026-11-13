// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/Sidebar.h"
#include "core/JsonStore.h"
#include "core/player/PlayerController.h"
#include "library/LibraryModel.h"
#include "library/MusicIndex.h"
#include "library/Playlists.h"
#include "ui/MusicView.h"
#include "thumbs/ThumbService.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QContextMenuEvent>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QProcess>
#include <QScrollBar>
#include <QUrl>

static void revealInFileManager(const QString& path) {
#ifdef Q_OS_WIN
    QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(path)});
#else
    QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath()));
#endif
}

static QString kindLabel(MediaKind k) {
    switch (k) { case MediaKind::Video: return QObject::tr("Video"); case MediaKind::Audio: return QObject::tr("Audio"); case MediaKind::Image: return QObject::tr("Image"); case MediaKind::Gif: return QObject::tr("GIF"); default: return {}; }
}
static Icon kindIcon(MediaKind k) {
    switch (k) { case MediaKind::Video: return Icon::Video; case MediaKind::Audio: return Icon::Music; case MediaKind::Image: return Icon::Image; case MediaKind::Gif: return Icon::Gif; default: return Icon::File; }
}

// ================= MediaView =================
MediaView::MediaView(QWidget* parent) : QListView(parent) {
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBar(new ScrollBar(Qt::Vertical, this));
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    verticalScrollBar()->setSingleStep(24);
    setMouseTracking(true);
    setUniformItemSizes(true);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setEditTriggers(NoEditTriggers);
    setFocusPolicy(Qt::ClickFocus);
    setStyleSheet(QStringLiteral("QListView{background:transparent;border:none;outline:none;}"));
    viewport()->setAutoFillBackground(false);
}
void MediaView::setGrid(bool g) {
    m_grid = g;
    setViewMode(g ? QListView::IconMode : QListView::ListMode);
    setResizeMode(QListView::Adjust);
    setMovement(QListView::Static);
    setWrapping(g);
    setSpacing(g ? 4 : 0);
    setGridSize(g ? QSize(148, 128) : QSize());
}
void MediaView::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {
    case Qt::Key_Return: case Qt::Key_Enter: if (currentIndex().isValid()) emit activatedIndex(currentIndex()); return;
    case Qt::Key_Up: case Qt::Key_Down: case Qt::Key_Home: case Qt::Key_End: QListView::keyPressEvent(e); return;
    case Qt::Key_Left: case Qt::Key_Right: if (m_grid) { QListView::keyPressEvent(e); return; } break;
    default: break;
    }
    e->ignore();       // bubbles to the main window's shortcuts (Space = play/pause, etc.)
}
void MediaView::mouseReleaseEvent(QMouseEvent* e) {
    const QModelIndex i = indexAt(e->pos());
    QListView::mouseReleaseEvent(e);
    if (e->button() == Qt::LeftButton && i.isValid() && state() != DraggingState) emit activatedIndex(i);
}
void MediaView::contextMenuEvent(QContextMenuEvent* e) {
    const QModelIndex i = indexAt(e->pos());
    if (i.isValid()) { setCurrentIndex(i); emit contextRequested(i, e->globalPos()); }
}

// ================= Delegate =================
namespace { enum { LibraryMark_Keep = 1, LibraryMark_Reject = 2, LibraryMark_Seen = 4 }; }   // = PlayerController::Mark*
QSize MediaDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const {
    return m_grid ? QSize(140, 120) : QSize(0, Theme::i().rowH() + 18);
}

void MediaDelegate::paint(QPainter* p, const QStyleOptionViewItem& opt, const QModelIndex& idx) const {
    const auto& c = pal();
    p->save();
    p->setRenderHint(QPainter::Antialiasing);
    QString path = idx.data(LibraryModel::PathRole).toString();
    if (path.isEmpty()) path = idx.data(Qt::DisplayRole).toString();     // queue model stores raw paths
    const bool dir = idx.data(LibraryModel::IsDirRole).toBool();
    const bool place = idx.data(LibraryModel::IsPlaceRole).toBool();
    MediaKind kind = MediaKind(idx.data(LibraryModel::KindRole).toInt());
    if (kind == MediaKind::Unknown && !dir) kind = Media::kindFor(path);
    const qint64 size = idx.data(LibraryModel::SizeRole).isValid() ? idx.data(LibraryModel::SizeRole).toLongLong() : -1;
    const QString name = dir ? QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName() : QFileInfo(path).completeBaseName();
    const bool playing = !dir && m_ctl->path() == path;
    const bool hover = opt.state & QStyle::State_MouseOver;
    const bool sel = opt.state & QStyle::State_Selected;

    QPixmap pm;
    if (!dir && m_thumbs) pm = m_thumbs->get(path, kind);

    auto drawThumb = [&](const QRectF& tr, qreal rad) {
        p->setPen(Qt::NoPen); p->setBrush(dir ? c.accentSubtle : c.sunken);
        p->drawRoundedRect(tr, rad, rad);
        if (!pm.isNull()) {
            p->save();
            QPainterPath clip; clip.addRoundedRect(tr, rad, rad); p->setClipPath(clip);
            const qreal dpr = p->device()->devicePixelRatioF();
            const QSizeF ps(pm.width() / pm.devicePixelRatio(), pm.height() / pm.devicePixelRatio());
            const qreal s = qMax(tr.width() / ps.width(), tr.height() / ps.height());          // cover
            const QSizeF ds(ps.width() * s, ps.height() * s);
            p->setRenderHint(QPainter::SmoothPixmapTransform);
            p->drawPixmap(QRectF(tr.center().x() - ds.width() / 2, tr.center().y() - ds.height() / 2, ds.width(), ds.height()), pm, QRectF(pm.rect()));
            Q_UNUSED(dpr)
            p->restore();
        } else {
            const qreal is = qMin(tr.width(), tr.height()) * 0.5;
            drawIcon(p, dir ? (place ? Icon::Drive : Icon::Folder) : kindIcon(kind), QRectF(tr.center().x() - is / 2, tr.center().y() - is / 2, is, is), dir ? c.accent : c.muted, 1.6);
        }
        if (dir) return;
        // review marks: always visible, no mode to switch on
        const int st = m_ctl->reviewState(path);
        const bool av = kind == MediaKind::Video || kind == MediaKind::Audio;
        QPainterPath clip; clip.addRoundedRect(tr, rad, rad);
        if (st & LibraryMark_Reject) { p->save(); p->setClipPath(clip); p->fillRect(tr, QColor(0, 0, 0, c.dark ? 120 : 105)); p->restore(); }
        const qreal wf = m_ctl->watched(path);
        if (av && wf > 0.02) {                                              // how far you got
            const QRectF track(tr.left() + 4, tr.bottom() - 6, tr.width() - 8, 3);
            p->setPen(Qt::NoPen); p->setBrush(QColor(0, 0, 0, 110)); p->drawRoundedRect(track, 1.5, 1.5);
            p->setBrush(wf >= 0.95 ? c.accent.lighter(115) : c.accent); p->drawRoundedRect(QRectF(track.left(), track.top(), track.width() * qMin(1.0, wf), track.height()), 1.5, 1.5);
        }
        const qreal bd = qMin<qreal>(18, tr.height() * 0.42);
        if (st & LibraryMark_Keep) { p->setPen(Qt::NoPen); p->setBrush(c.accent); p->drawEllipse(QRectF(tr.left() + 4, tr.top() + 4, bd, bd)); drawIcon(p, Icon::Check, QRectF(tr.left() + 4 + bd * 0.2, tr.top() + 4 + bd * 0.2, bd * 0.6, bd * 0.6), c.onAccent, 2.2); }
        else if (st & LibraryMark_Reject) { p->setPen(Qt::NoPen); p->setBrush(c.danger); p->drawEllipse(QRectF(tr.left() + 4, tr.top() + 4, bd, bd)); drawIcon(p, Icon::Close, QRectF(tr.left() + 4 + bd * 0.22, tr.top() + 4 + bd * 0.22, bd * 0.56, bd * 0.56), QColor(255, 255, 255), 2.2); }
        else if (av && !(st & LibraryMark_Seen)) { p->setPen(QPen(QColor(255, 255, 255, 200), 1.2)); p->setBrush(c.accent); p->drawEllipse(QRectF(tr.right() - 12, tr.top() + 5, 8, 8)); }   // not seen yet
    };

    if (m_grid) {
        const QRectF r = QRectF(opt.rect).adjusted(2, 2, -2, -2);
        if (playing || hover || sel) { p->setPen(Qt::NoPen); p->setBrush(playing ? c.accentSubtle : c.surface); p->drawRoundedRect(r, 10, 10); }
        drawThumb(QRectF(r.left() + 6, r.top() + 6, r.width() - 12, 78), 8);
        p->setPen(playing ? c.accent : c.text); p->setFont(Theme::i().font(12, playing ? QFont::DemiBold : QFont::Normal));
        p->drawText(QRectF(r.left() + 8, r.top() + 88, r.width() - 16, 28), Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, QFontMetrics(p->font()).elidedText(name, Qt::ElideMiddle, int((r.width() - 16) * 1.9)));
        p->restore();
        return;
    }
    const QRectF r = QRectF(opt.rect).adjusted(6, 2, -6, -2);
    if (playing) { p->setPen(Qt::NoPen); p->setBrush(c.accentSubtle); p->drawRoundedRect(r, 8, 8); }
    else if (hover || sel) { p->setPen(Qt::NoPen); p->setBrush(c.sunken.lighter(c.dark ? 130 : 103)); p->drawRoundedRect(r, 8, 8); }
    const qreal th = r.height() - 12;
    drawThumb(QRectF(r.left() + 8, r.top() + 6, th * (dir ? 1.0 : 1.4), th), 6);
    const qreal tx = r.left() + 8 + th * (dir ? 1.0 : 1.4) + 12;
    QFont f = Theme::i().font(14, playing ? QFont::DemiBold : QFont::Medium);
    p->setFont(f);
    p->setPen(playing ? c.accent : c.text);
    const QFontMetrics fm(f);
    p->drawText(QRectF(tx, r.top() + 5, r.right() - tx - 26, 20), Qt::AlignLeft | Qt::AlignVCenter, fm.elidedText(name, Qt::ElideMiddle, int(r.right() - tx - 26)));
    p->setFont(Theme::i().font(12));
    p->setPen(c.muted);
    QString sub;
    if (place) sub = QFileInfo(path).absolutePath();
    else if (dir) sub = QObject::tr("Folder");
    else { sub = kindLabel(kind); if (size >= 0) sub += QStringLiteral(" · ") + QLocale().formattedDataSize(size, 1); else if (m_queue) sub += QStringLiteral(" · ") + QFileInfo(path).dir().dirName(); }
    p->drawText(QRectF(tx, r.top() + 24, r.right() - tx - 26, 18), Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(p->font()).elidedText(sub, Qt::ElideMiddle, int(r.right() - tx - 26)));
    if (dir) drawIcon(p, Icon::ChevronRight, QRectF(r.right() - 24, r.center().y() - 8, 16, 16), c.muted);
    else if (playing && (kind == MediaKind::Audio || kind == MediaKind::Video)) drawIcon(p, Icon::Audio, QRectF(r.right() - 26, r.center().y() - 8, 16, 16), c.accent);
    p->restore();
}

// ================= Sidebar =================
void Sidebar::refreshFolder() { if (!m_model->folder().isEmpty()) m_model->refresh(); }

Sidebar::Sidebar(PlayerController* ctl, ThumbService* thumbs, JsonStore* settings, QWidget* parent)
    : QWidget(parent), m_ctl(ctl), m_thumbs(thumbs), m_set(settings) {
    m_model = new LibraryModel(this);
    m_tabs = new Segmented({tr("Library"), tr("Queue"), tr("Music")}, this);
    m_filter = new Segmented({tr("All"), tr("Video"), tr("Audio"), tr("Images")}, this);
    m_up = new IconButton(Icon::ChevronLeft, this, 30);
    m_viewBtn = new IconButton(Icon::Grid, this, 32);
    m_sortBtn = new IconButton(Icon::List, this, 32);
    m_addPlace = new IconButton(Icon::Plus, this, 30);
    m_clearQ = new IconButton(Icon::Trash, this, 30);
    m_saveQ = new IconButton(Icon::Export, this, 30);
    m_up->setToolTip(tr("Up one folder"));
    m_viewBtn->setToolTip(tr("List or grid"));
    m_sortBtn->setToolTip(tr("Sort and filter"));
    m_addPlace->setToolTip(tr("Add this folder to your places"));
    m_saveQ->setToolTip(tr("Save the queue as a playlist"));
    m_clearQ->setToolTip(tr("Clear the queue"));
    m_musicIdx = new MusicIndex(this);
    m_music = new MusicView(ctl, m_musicIdx, this);
    connect(m_music, &MusicView::toast, this, &Sidebar::toast);
    connect(m_music, &MusicView::savePlaylistRequested, this, &Sidebar::savePlaylistRequested);
    connect(m_music, &MusicView::renamePlaylistRequested, this, &Sidebar::renamePlaylistRequested);
    connect(m_music, &MusicView::deletePlaylistRequested, this, &Sidebar::deletePlaylistRequested);
    connect(m_saveQ, &QAbstractButton::clicked, this, &Sidebar::savePlaylistRequested);
    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Filter this folder"));
    m_search->setClearButtonEnabled(true);
    m_list = new MediaView(this);
    m_queueView = new MediaView(this);
    m_libDel = new MediaDelegate(ctl, thumbs, this);
    m_queueDel = new MediaDelegate(ctl, thumbs, this);
    m_queueDel->setQueueMode(true);
    m_list->setModel(m_model);
    m_list->setItemDelegate(m_libDel);
    m_queueView->setModel(ctl->queue());
    m_queueView->setItemDelegate(m_queueDel);
    m_queueView->setDragDropMode(QAbstractItemView::InternalMove);
    m_queueView->setDefaultDropAction(Qt::MoveAction);
    m_queueView->setDragEnabled(true);
    m_queueView->setAcceptDrops(true);
    m_queueView->setDropIndicatorShown(true);

    m_repaint.setSingleShot(true); m_repaint.setInterval(60);
    connect(&m_repaint, &QTimer::timeout, this, [this] { m_list->viewport()->update(); m_queueView->viewport()->update(); });
    connect(thumbs, &ThumbService::ready, this, [this] { if (!m_repaint.isActive()) m_repaint.start(); });
    connect(ctl, &PlayerController::markChanged, this, [this] { if (!m_repaint.isActive()) m_repaint.start(); });
    connect(ctl, &PlayerController::pathChanged, this, [this] { m_list->viewport()->update(); m_queueView->viewport()->update(); update(); });
    connect(ctl, &PlayerController::queueChanged, this, [this] { update(); relayout(); });

    connect(m_tabs, &Segmented::currentChanged, this, [this](int i) { showTab(i); });
    connect(m_filter, &Segmented::currentChanged, this, [this](int i) { m_model->setKindFilter(i); m_set->set("filter", i); });
    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& t) { m_model->setText(t); });
    connect(m_up, &QAbstractButton::clicked, this, [this] { navigate(parentOf(m_model->folder())); });
    connect(m_viewBtn, &QAbstractButton::clicked, this, [this] { applyGrid(!m_grid); });
    connect(m_sortBtn, &QAbstractButton::clicked, this, [this] {
        auto* m = new PopupMenu(this);
        m->addHeader(tr("Sort by"));
        m->addItem("0", tr("Name"), m_sort == 0); m->addItem("1", tr("Newest first"), m_sort == 1); m->addItem("2", tr("Largest first"), m_sort == 2);
        m->addSeparator();
        m->addHeader(tr("Show"));
        const int rf = m_model->reviewFilter();
        m->addItem("r0", tr("Everything"), rf == 0); m->addItem("r1", tr("Not seen yet"), rf == 1); m->addItem("r2", tr("Kept"), rf == 2); m->addItem("r3", tr("Marked to reject"), rf == 3);
        connect(m, &PopupMenu::triggered, this, [this](const QString& id) {
            if (id.startsWith('r')) { m_model->setReviewFilter(id.mid(1).toInt(), [this](const QString& p) { return m_ctl->reviewState(p); }); return; }
            m_sort = id.toInt(); m_model->setSort(m_sort); m_set->set("sort", m_sort);
        });
        m->popup(m_sortBtn->mapToGlobal(QPoint(0, m_sortBtn->height() + 4)));
    });
    connect(m_addPlace, &QAbstractButton::clicked, this, [this] {
        const QString f = m_model->folder();
        if (f.isEmpty()) emit addFolderRequested(); else { addPlace(f); emit toast(tr("Added to your places")); }
    });
    connect(m_clearQ, &QAbstractButton::clicked, this, [this] { m_ctl->clearQueue(); });
    connect(m_list, &MediaView::activatedIndex, this, &Sidebar::onActivated);
    connect(m_list, &MediaView::contextRequested, this, &Sidebar::onContext);
    connect(m_queueView, &MediaView::activatedIndex, this, [this](const QModelIndex& i) { m_ctl->playIndex(i.row()); });
    connect(m_queueView, &MediaView::contextRequested, this, [this](const QModelIndex& i, const QPoint& g) {
        auto* m = new PopupMenu(this);
        const QString path = i.data(Qt::DisplayRole).toString();
        m->addItem("play", tr("Play")); m->addItem("remove", tr("Remove from queue")); m->addItem("forget", tr("Forget this file")); m->addSeparator(); m->addItem("reveal", tr("Show in Explorer"));
        connect(m, &PopupMenu::triggered, this, [this, i, path](const QString& id) {
            if (id == "play") m_ctl->playIndex(i.row());
            else if (id == "remove") m_ctl->removeAt(i.row());
            else if (id == "forget") { m_ctl->forgetFile(path); m_thumbs->forget(path); emit toast(tr("Forgot this file: no resume point, marks, history or thumbnail left")); }
            else if (id == "reveal") revealInFileManager(path);
        });
        m->popup(g);
    });
    connect(m_model, &LibraryModel::failed, this, [this](const QString& m) { emit toast(m); });
    connect(m_model, &LibraryModel::loadingChanged, this, [this](bool) { update(); });
    connect(&Theme::i(), &Theme::changed, this, [this] {
        const auto& c = pal();
        m_search->setStyleSheet(QStringLiteral("QLineEdit{background:%1;border:1px solid %2;border-radius:8px;padding:0 8px;color:%3;selection-background-color:%4;selection-color:%5;font-size:14px;}QLineEdit:focus{border:1px solid %6;}")
                                    .arg(c.raised.name(), c.border.name(), c.text.name(), c.accent.name(), c.onAccent.name(), c.accent.name()));
        m_search->setFont(Theme::i().font(14));
        update();
    });
    Theme::i().changed();

    m_sort = int(m_set->num("sort", 0)); m_model->setSort(m_sort);
    m_filter->setCurrent(int(m_set->num("filter", 0))); m_model->setKindFilter(m_filter->current());
    QStringList pl; for (const auto& v : m_set->get("places").toArray()) pl << v.toString();
    m_model->setPlaces(pl);
    m_musicIdx->setRoots(pl);
    applyGrid(m_set->flag("grid", false));
    const QString last = m_set->str("lastFolder");
    if (!last.isEmpty() && QDir(last).exists()) navigate(last); else m_model->setFolder({});
    showQueue(false);
}

QString Sidebar::parentOf(const QString& f) const {
    if (f.isEmpty()) return {};
    for (const QString& p : places()) if (QDir::cleanPath(p) == QDir::cleanPath(f)) return {};
    const QDir d(f);
    QDir up = d; if (!up.cdUp() || up.absolutePath() == d.absolutePath()) return {};
    return up.absolutePath();
}

QStringList Sidebar::places() const { QStringList l; for (const auto& v : m_set->get("places").toArray()) l << v.toString(); return l; }
void Sidebar::savePlaces(const QStringList& p) { m_set->set("places", QJsonArray::fromStringList(p)); m_model->setPlaces(p); m_musicIdx->setRoots(p); emit placesChanged(); update(); }
void Sidebar::addPlace(const QString& path) { QStringList l = places(); const QString c = QDir::cleanPath(path); if (!l.contains(c)) { l << c; savePlaces(l); } }
void Sidebar::removePlace(const QString& path) { QStringList l = places(); l.removeAll(QDir::cleanPath(path)); savePlaces(l); }

void Sidebar::navigate(const QString& folder) {
    m_model->setFolder(folder);
    m_set->set("lastFolder", folder);
    m_search->clear();
    m_list->verticalScrollBar()->setValue(0);
    showQueue(false);
    m_tabs->setCurrent(0);
    relayout();
    update();
}

void Sidebar::showQueue(bool q) { showTab(q ? 1 : 0); }

void Sidebar::showTab(int tab) {
    m_tab = qBound(0, tab, 2);
    m_queueTab = m_tab == 1;
    m_tabs->setCurrent(m_tab);
    if (m_tab == 2) { m_musicIdx->ensureScanned(); m_music->refresh(); }       // the tags are read only once you open Music
    relayout();
    update();
}

void Sidebar::showPlaylists() { showTab(2); m_music->showPlaylists(); }
void Sidebar::musicDrill(const QString& artist, const QString& album) { showTab(2); m_music->drill(artist, album); }
void Sidebar::refreshMusic() { m_music->refresh(); }
void Sidebar::clearMusicIndex() { m_musicIdx->clearAll(); m_music->refresh(); }
void Sidebar::forgetPlaces() { m_set->remove("lastFolder"); savePlaces({}); m_model->setFolder({}); relayout(); update(); }

void Sidebar::applyGrid(bool g) {
    m_grid = g;
    m_set->set("grid", g);
    m_libDel->setGrid(g);
    m_list->setGrid(g);
    m_viewBtn->setIcon(g ? Icon::List : Icon::Grid);
    m_list->doItemsLayout();
}

void Sidebar::focusSearch() { showQueue(false); m_search->setFocus(); m_search->selectAll(); }

void Sidebar::onActivated(const QModelIndex& i) {
    const QString path = i.data(LibraryModel::PathRole).toString();
    if (i.data(LibraryModel::IsDirRole).toBool()) { navigate(path); return; }
    const MediaKind k = MediaKind(i.data(LibraryModel::KindRole).toInt());
    if (!QFileInfo::exists(path)) { emit toast(tr("That file is gone")); m_model->refresh(); return; }
    const QStringList files = m_model->shownFiles(Media::family(k));
    if (m_ctl->queue()->stringList() == files) m_ctl->playIndex(files.indexOf(path));
    else m_ctl->setQueue(files, path);
}

void Sidebar::onContext(const QModelIndex& i, const QPoint& g) {
    const QString path = i.data(LibraryModel::PathRole).toString();
    const bool dir = i.data(LibraryModel::IsDirRole).toBool();
    const bool place = i.data(LibraryModel::IsPlaceRole).toBool();
    auto* m = new PopupMenu(this);
    if (dir) { m->addItem("open", tr("Open")); m->addItem("playall", tr("Play everything in this folder")); if (!place) m->addItem("addplace", tr("Add to places")); else m->addItem("rmplace", tr("Remove from places")); }
    else { m->addItem("play", tr("Play")); m->addItem("queue", tr("Add to queue")); }
    if (!dir) m->addItem("forget", tr("Forget this file"));
    m->addItem("newwin", tr("Open in new window"));
    m->addSeparator();
    m->addItem("reveal", tr("Show in Explorer"));
    connect(m, &PopupMenu::triggered, this, [this, path, i](const QString& id) {
        if (id == "open") navigate(path);
        else if (id == "playall") m_ctl->open({path});
        else if (id == "addplace") { addPlace(path); emit toast(tr("Added to your places")); }
        else if (id == "rmplace") removePlace(path);
        else if (id == "play") onActivated(i);
        else if (id == "queue") { m_ctl->enqueue({path}); emit toast(tr("Added to queue")); }
        else if (id == "forget") { m_ctl->forgetFile(path); m_thumbs->forget(path); emit toast(tr("Forgot this file: no resume point, marks, history or thumbnail left")); }
        else if (id == "newwin") emit openInNewWindow(path);
        else if (id == "reveal") revealInFileManager(path);
    });
    m->popup(g);
}

void Sidebar::resizeEvent(QResizeEvent*) { relayout(); }

void Sidebar::relayout() {
    const int w = width(), h = height();
    m_tabs->setGeometry(12, 50, m_tabs->sizeHint().width(), 30);
    const bool lib = m_tab == 0, q = m_tab == 1, mu = m_tab == 2;
    const bool inPlaces = m_model->folder().isEmpty();
    m_up->setVisible(lib); m_addPlace->setVisible(lib); m_list->setVisible(lib);
    m_filter->setVisible(lib && !inPlaces); m_search->setVisible(lib && !inPlaces);
    m_viewBtn->setVisible(lib && !inPlaces); m_sortBtn->setVisible(lib && !inPlaces);
    m_clearQ->setVisible(q); m_saveQ->setVisible(q); m_queueView->setVisible(q);
    m_music->setVisible(mu);
    m_viewBtn->move(w - 12 - 32, 49);
    m_sortBtn->move(w - 12 - 32 - 34, 49);
    m_up->move(12, 90);
    m_addPlace->move(w - 12 - 30, 90);
    m_filter->setGeometry(12, 128, qMin(w - 24, m_filter->sizeHint().width()), 30);
    m_search->setGeometry(12, 166, w - 24, 32);
    m_list->setGeometry(0, inPlaces ? 130 : 206, w, h - (inPlaces ? 130 : 206) - 8);
    m_clearQ->move(w - 12 - 30, 90);
    m_saveQ->move(w - 12 - 30 - 34, 90);
    m_queueView->setGeometry(0, 130, w, h - 130 - 8);
    m_music->setGeometry(0, 90, w, h - 90 - 8);
    m_up->setEnabled(!inPlaces);
}

void Sidebar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.fillRect(rect(), c.surface);
    p.setPen(c.border); p.drawLine(width() - 1, 0, width() - 1, height());
    p.setPen(c.text); p.setFont(Theme::i().font(17, QFont::DemiBold, true));
    p.drawText(QRectF(18, 0, width() - 36, 44), Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral(APP_NAME));
    const bool inPlaces = m_model->folder().isEmpty();
    if (m_tab == 0) {
        p.setFont(Theme::i().font(13, QFont::DemiBold)); p.setPen(c.text);
        QString label = inPlaces ? tr("Places") : QFileInfo(m_model->folder()).fileName();
        if (label.isEmpty()) label = m_model->folder();
        const int x = 48, wmax = width() - 48 - 48;
        p.drawText(QRectF(x, 90, wmax, 30), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(label, Qt::ElideMiddle, wmax));
        if (inPlaces && m_model->rowCount() == 0) {
            p.setPen(c.muted); p.setFont(Theme::i().font(14));
            p.drawText(QRectF(24, 150, width() - 48, 90), Qt::AlignTop | Qt::TextWordWrap, tr("Add the folders where you keep your movies, shows, music and pictures. They'll show up here."));
        } else if (m_model->rowCount() == 0 && !m_model->loading()) {
            p.setPen(c.muted); p.setFont(Theme::i().font(14));
            const QString msg = !m_model->error().isEmpty() ? m_model->error() : (m_model->fileCount() ? tr("Nothing matches your filter") : tr("No media in this folder"));
            p.drawText(QRectF(24, 226, width() - 48, 80), Qt::AlignTop | Qt::TextWordWrap, msg);
        } else if (m_model->loading()) {
            p.setPen(c.muted); p.setFont(Theme::i().font(14));
            p.drawText(QRectF(24, inPlaces ? 150 : 226, width() - 48, 30), Qt::AlignTop, tr("Loading…"));
        }
    } else if (m_tab == 1) {
        const int n = m_ctl->queue()->rowCount();
        p.setFont(Theme::i().font(13, QFont::DemiBold)); p.setPen(c.text);
        p.drawText(QRectF(16, 90, width() - 70, 30), Qt::AlignVCenter, tr("Up next · %1").arg(n));
        if (n == 0) { p.setPen(c.muted); p.setFont(Theme::i().font(14)); p.drawText(QRectF(24, 150, width() - 48, 80), Qt::AlignTop | Qt::TextWordWrap, tr("The queue is empty. Play something from your library and the rest of the folder lines up here.")); }
    }
}


int Sidebar::libraryRows() const { return m_model->rowCount(); }
QString Sidebar::currentFolder() const { return m_model->folder(); }
