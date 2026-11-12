// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/Overlays.h"
#include "core/JsonStore.h"
#include "core/Paths.h"
#include "core/player/MpvPlayer.h"
#include "core/player/PlayerController.h"
#include "core/Updater.h"
#include "library/LibraryIndex.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QApplication>
#include <QCollator>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTextBrowser>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

static QColor withA(QColor c, qreal a) { c.setAlphaF(a); return c; }

// ============================================================ RowList
RowList::RowList(QWidget* parent) : QWidget(parent) { setMouseTracking(true); setFocusPolicy(Qt::NoFocus); connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update)); }
void RowList::setRows(const QVector<Row>& r, bool keepScroll) {
    const int oldScroll = m_scroll, oldCur = m_cur;
    m_rows = r; m_scroll = 0; m_cur = -1; m_hover = -1;
    for (int i = 0; i < r.size(); ++i) if (!r[i].header) { m_cur = i; break; }
    if (keepScroll) {                                    // a live refresh (e.g. while tags are still being read) must not throw the list back to the top
        m_scroll = qBound(0, oldScroll, qMax(0, contentHeight() - height()));
        if (oldCur >= 0 && oldCur < r.size() && !r[oldCur].header) m_cur = oldCur;
    }
    update();
}
void RowList::setCurrent(int i) { m_cur = i; ensureVisible(i); update(); }
int RowList::contentHeight() const { int h = 0; for (const auto& r : m_rows) h += r.header ? 30 : m_rowH; return h; }
void RowList::move(int dir) {
    if (m_rows.isEmpty()) return;
    int i = m_cur;
    for (int n = 0; n < m_rows.size(); ++n) { i = (i + dir + m_rows.size()) % m_rows.size(); if (!m_rows[i].header) { setCurrent(i); return; } }
}
void RowList::ensureVisible(int idx) {
    int y = 0;
    for (int i = 0; i < m_rows.size(); ++i) {
        const int h = m_rows[i].header ? 30 : m_rowH;
        if (i == idx) { if (y < m_scroll) m_scroll = y; else if (y + h > m_scroll + height()) m_scroll = y + h - height(); break; }
        y += h;
    }
}
int RowList::rowAt(const QPoint& p) const {
    int y = -m_scroll;
    for (int i = 0; i < m_rows.size(); ++i) {
        const int h = m_rows[i].header ? 30 : m_rowH;
        if (p.y() >= y && p.y() < y + h) return m_rows[i].header ? -1 : i;
        y += h;
    }
    return -1;
}
void RowList::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    if (m_rows.isEmpty()) { p.setPen(c.muted); p.setFont(Theme::i().font(14)); p.drawText(rect().adjusted(16, 16, -16, 0), Qt::AlignTop | Qt::AlignLeft | Qt::TextWordWrap, m_empty); return; }
    int y = -m_scroll;
    for (int i = 0; i < m_rows.size(); ++i) {
        const Row& r = m_rows[i];
        const int h = r.header ? 30 : m_rowH;
        if (y + h >= 0 && y <= height()) {
            const QRectF rc(8, y + 1, width() - 16, h - 2);
            if (r.header) { p.setPen(c.muted); p.setFont(Theme::i().font(11, QFont::DemiBold)); p.drawText(rc.adjusted(8, 8, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, r.title.toUpper()); }
            else {
                if (i == m_cur || i == m_hover) { p.setPen(Qt::NoPen); p.setBrush(i == m_cur ? c.accentSubtle : c.surface); p.drawRoundedRect(rc, 8, 8); }
                if (m_icons) drawIcon(&p, r.icon, QRectF(rc.left() + 12, rc.center().y() - 10, 20, 20), i == m_cur ? c.accent : c.muted, 1.6);
                const qreal tx0 = m_icons ? 44 : 12;
                p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::Medium));
                const qreal hw = r.hint.isEmpty() ? 0 : qMin<qreal>(rc.width() * 0.55, QFontMetrics(Theme::i().font(12)).horizontalAdvance(r.hint) + 28);
                const qreal tw = rc.width() - tx0 - 12 - hw;
                if (r.subtitle.isEmpty()) p.drawText(QRectF(rc.left() + tx0, rc.top(), tw, rc.height()), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(r.title, Qt::ElideMiddle, int(tw)));
                else {
                    p.drawText(QRectF(rc.left() + tx0, rc.top() + 4, tw, 20), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(r.title, Qt::ElideMiddle, int(tw)));
                    p.setPen(c.muted); p.setFont(Theme::i().font(12));
                    p.drawText(QRectF(rc.left() + tx0, rc.top() + 23, tw, 16), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(r.subtitle, Qt::ElideMiddle, int(tw)));
                }
                if (!r.hint.isEmpty()) { p.setPen(c.muted); p.setFont(Theme::i().font(12)); p.drawText(QRectF(rc.right() - hw - 4, rc.top(), hw - 4, rc.height()), Qt::AlignVCenter | Qt::AlignRight, QFontMetrics(p.font()).elidedText(r.hint, Qt::ElideLeft, int(hw - 8))); }
            }
        }
        y += h;
    }
    const int total = contentHeight();
    if (total > height()) {
        const qreal th = qMax(28.0, height() * qreal(height()) / total), ty = (height() - th) * m_scroll / qMax(1, total - height());
        p.setPen(Qt::NoPen); p.setBrush(withA(c.text, 0.22)); p.drawRoundedRect(QRectF(width() - 7, ty, 4, th), 2, 2);
    }
}
void RowList::mouseMoveEvent(QMouseEvent* e) { const int i = rowAt(e->pos()); if (i != m_hover) { m_hover = i; setCursor(i >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); } }
void RowList::mouseReleaseEvent(QMouseEvent* e) { const int i = rowAt(e->pos()); if (e->button() == Qt::LeftButton && i >= 0) { m_cur = i; emit activated(i); } }
void RowList::wheelEvent(QWheelEvent* e) { m_scroll = qBound(0, m_scroll - e->angleDelta().y() / 2, qMax(0, contentHeight() - height())); update(); e->accept(); }

// ============================================================ Sheet
Sheet::Sheet(QWidget* parent) : QWidget(parent) {
    hide();
    setFocusPolicy(Qt::StrongFocus);
    m_anim.setEasingCurve(Theme::easeMac());
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_op = qBound(0.0, v.toReal(), 1.0); update();
        if (m_content) m_content->move(cardRect().topLeft().toPoint());
        if (m_closing && m_op <= 0.001) { hide(); m_closing = false; emit closed(); deleteLater(); }
    });
}
void Sheet::setContent(QWidget* w, const QSize& s) { m_content = w; m_size = s; w->setParent(this); w->resize(s); w->show(); }
QRectF Sheet::cardRect() const {
    const QSize s(qMin(m_size.width(), width() - 32), qMin(m_size.height(), height() - 32));
    if (m_content && m_content->size() != s) m_content->resize(s);
    return QRectF((width() - s.width()) / 2.0, (height() - s.height()) / 2.0 + (1.0 - m_op) * 22, s.width(), s.height());
}
void Sheet::openSheet() {
    setGeometry(parentWidget()->rect());
    raise(); show(); setFocus();
    m_anim.stop(); m_anim.setDuration(Theme::i().animMs(320)); m_anim.setStartValue(0.0); m_anim.setEndValue(1.0);
    if (m_anim.duration() == 0) { m_op = 1; update(); } else m_anim.start();
    if (m_content) m_content->move(cardRect().topLeft().toPoint());
}
void Sheet::closeSheet() {
    if (m_closing) return;
    m_closing = true;
    m_anim.stop(); m_anim.setDuration(Theme::i().animMs(200)); m_anim.setStartValue(m_op); m_anim.setEndValue(0.0);
    if (m_anim.duration() == 0) { hide(); m_closing = false; emit closed(); deleteLater(); } else m_anim.start();
}
void Sheet::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), withA(pal().scrim, pal().scrim.alphaF() * m_op));
    const QRectF r = cardRect();
    for (int i = 8; i >= 1; --i) { p.setPen(Qt::NoPen); p.setBrush(withA(pal().shadow, 0.03 * m_op * (9 - i))); p.drawRoundedRect(r.adjusted(-i, -i + 4, i, i + 4), 14 + i, 14 + i); }
    p.setBrush(pal().bg); p.setPen(QPen(pal().border, 1));
    p.drawRoundedRect(r, 14, 14);
}
void Sheet::mousePressEvent(QMouseEvent* e) { if (!cardRect().contains(e->position())) closeSheet(); }
void Sheet::keyPressEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Escape) closeSheet(); else QWidget::keyPressEvent(e); }

// ============================================================ ConfirmBox
ConfirmBox::ConfirmBox(const QString& title, const QString& body, const QString& okText, bool danger, QWidget* parent, const QString& cancelText) : QWidget(parent), m_title(title), m_body(body) {
    setFocusPolicy(Qt::StrongFocus);
    m_ok = new TextButton(okText, this, true);
    m_ok->setDanger(danger);
    m_cancel = new TextButton(cancelText.isEmpty() ? tr("Cancel") : cancelText, this);
    connect(m_ok, &QAbstractButton::clicked, this, &ConfirmBox::accepted);
    connect(m_cancel, &QAbstractButton::clicked, this, &ConfirmBox::cancelled);
}
void ConfirmBox::resizeEvent(QResizeEvent*) {
    const int y = height() - 36 - 22;
    m_ok->setGeometry(width() - 24 - m_ok->sizeHint().width(), y, m_ok->sizeHint().width(), 36);
    m_cancel->setGeometry(m_ok->x() - 10 - m_cancel->sizeHint().width(), y, m_cancel->sizeHint().width(), 36);
}
void ConfirmBox::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) emit accepted();
    else if (e->key() == Qt::Key_Escape) emit cancelled();
    else QWidget::keyPressEvent(e);
}
void ConfirmBox::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(pal().text); p.setFont(Theme::i().font(20, QFont::DemiBold, true));
    p.drawText(QRectF(24, 20, width() - 48, 32), Qt::AlignVCenter, m_title);
    p.setPen(pal().muted); p.setFont(Theme::i().font(14));
    p.drawText(QRectF(24, 62, width() - 48, height() - 62 - 80), Qt::AlignTop | Qt::TextWordWrap, m_body);
}

// ============================================================ InputBox
InputBox::InputBox(const QString& title, const QString& label, const QString& text, const QString& okText, QWidget* parent) : QWidget(parent), m_title(title), m_label(label) {
    setFocusPolicy(Qt::StrongFocus);
    m_edit = new QLineEdit(text, this);
    m_edit->setMaxLength(80);
    const auto& c = pal();
    m_edit->setStyleSheet(QStringLiteral("QLineEdit{background:%1;border:1px solid %2;border-radius:8px;padding:0 10px;color:%3;selection-background-color:%4;selection-color:%5;font-size:15px;}QLineEdit:focus{border:1px solid %6;}")
                              .arg(c.raised.name(), c.border.name(), c.text.name(), c.accent.name(), c.onAccent.name(), c.accent.name()));
    m_ok = new TextButton(okText, this, true);
    m_cancel = new TextButton(tr("Cancel"), this);
    connect(m_ok, &QAbstractButton::clicked, this, [this] { if (!m_edit->text().trimmed().isEmpty()) emit accepted(m_edit->text().trimmed()); });
    connect(m_cancel, &QAbstractButton::clicked, this, &InputBox::cancelled);
    connect(m_edit, &QLineEdit::returnPressed, m_ok, &QAbstractButton::click);
}
void InputBox::focusEdit() { m_edit->setFocus(); m_edit->selectAll(); }
void InputBox::resizeEvent(QResizeEvent*) {
    m_edit->setGeometry(24, 96, width() - 48, 40);
    const int y = height() - 36 - 22;
    m_ok->setGeometry(width() - 24 - m_ok->sizeHint().width(), y, m_ok->sizeHint().width(), 36);
    m_cancel->setGeometry(m_ok->x() - 10 - m_cancel->sizeHint().width(), y, m_cancel->sizeHint().width(), 36);
}
void InputBox::keyPressEvent(QKeyEvent* e) { if (e->key() == Qt::Key_Escape) emit cancelled(); else QWidget::keyPressEvent(e); }
void InputBox::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(pal().text); p.setFont(Theme::i().font(20, QFont::DemiBold, true));
    p.drawText(QRectF(24, 20, width() - 48, 32), Qt::AlignVCenter, m_title);
    p.setPen(pal().muted); p.setFont(Theme::i().font(14));
    p.drawText(QRectF(24, 60, width() - 48, 28), Qt::AlignVCenter, m_label);
}

// ============================================================ FolderPicker
FolderPicker::FolderPicker(Mode m, const QString& startDir, QWidget* parent) : QWidget(parent), m_mode(m) {
    m_list = new RowList(this);
    m_up = new IconButton(Icon::ChevronLeft, this, 32);
    m_ok = new TextButton(m == PickFolder ? tr("Use this folder") : tr("Open"), this, true);
    m_cancel = new TextButton(tr("Cancel"), this);
    m_ok->setVisible(m == PickFolder);
    connect(m_cancel, &QAbstractButton::clicked, this, &FolderPicker::cancelled);
    connect(m_ok, &QAbstractButton::clicked, this, [this] { if (!m_dir.isEmpty()) emit chosen(m_dir); });
    connect(m_up, &QAbstractButton::clicked, this, [this] {
        QDir d(m_dir); if (m_dir.isEmpty() || !d.cdUp() || d.absolutePath() == QDir(m_dir).absolutePath()) go({}); else go(d.absolutePath());
    });
    connect(m_list, &RowList::activated, this, [this](int i) {
        const auto& r = m_list->rows()[i];
        if (r.icon == Icon::File || r.icon == Icon::Video || r.icon == Icon::Music || r.icon == Icon::Image || r.icon == Icon::Gif) emit chosen(r.data);
        else go(r.data);
    });
    go(startDir);
}
void FolderPicker::go(const QString& dir) {
    m_err.clear();
    QVector<RowList::Row> rows;
    if (dir.isEmpty()) {
        m_dir.clear();
        rows.push_back({Icon::Drive, tr("This PC"), {}, {}, {}, true});
        for (const QFileInfo& d : QDir::drives()) rows.push_back({Icon::Drive, QDir::toNativeSeparators(d.absoluteFilePath()), {}, {}, d.absoluteFilePath(), false});
        const QString home = QDir::homePath();
        rows.push_back({Icon::Home, tr("Quick access"), {}, {}, {}, true});
        for (const char* n : {"Videos", "Music", "Pictures", "Downloads", "Desktop"}) {
            const QString p = home + '/' + QString::fromLatin1(n);
            if (QDir(p).exists()) rows.push_back({Icon::Folder, QString::fromLatin1(n), {}, {}, p, false});
        }
    } else {
        QDir d(dir);
        if (!d.exists() || !QFileInfo(dir).isReadable()) { m_err = tr("Can't open that folder"); return; }
        m_dir = d.absolutePath();
        QDir::Filters f = QDir::Dirs | QDir::NoDotAndDotDot;
        if (m_mode == PickFile) f |= QDir::Files;
        QCollator col; col.setNumericMode(true); col.setCaseSensitivity(Qt::CaseInsensitive);
        auto list = d.entryInfoList(f, QDir::NoSort);
        std::sort(list.begin(), list.end(), [&](const QFileInfo& a, const QFileInfo& b) -> bool { if (a.isDir() != b.isDir()) return a.isDir(); return col.compare(a.fileName(), b.fileName()) < 0; });
        for (const QFileInfo& fi : list) {
            if (fi.fileName().startsWith('$')) continue;
            if (fi.isDir()) rows.push_back({Icon::Folder, fi.fileName(), {}, {}, fi.absoluteFilePath(), false});
            else { const MediaKind k = Media::kindFor(fi.fileName()); if (k == MediaKind::Unknown) continue;
                rows.push_back({k == MediaKind::Video ? Icon::Video : k == MediaKind::Audio ? Icon::Music : k == MediaKind::Gif ? Icon::Gif : Icon::Image, fi.completeBaseName(), {}, {}, fi.absoluteFilePath(), false}); }
        }
    }
    m_list->setEmptyText(m_mode == PickFolder ? tr("No subfolders here") : tr("Nothing playable here"));
    m_list->setRows(rows);
    m_up->setEnabled(!m_dir.isEmpty());
    update();
}
void FolderPicker::resizeEvent(QResizeEvent*) {
    m_up->move(16, 14);
    m_list->setGeometry(8, 62, width() - 16, height() - 62 - 68);
    m_cancel->setGeometry(width() - 16 - m_cancel->sizeHint().width(), height() - 52, m_cancel->sizeHint().width(), 36);
    m_ok->setGeometry(m_cancel->x() - 10 - m_ok->sizeHint().width(), height() - 52, m_ok->sizeHint().width(), 36);
}
void FolderPicker::paintEvent(QPaintEvent*) {
    QPainter p(this);
    const auto& c = pal();
    p.setPen(c.text); p.setFont(Theme::i().font(15, QFont::DemiBold, true));
    const QString t = m_dir.isEmpty() ? (m_mode == PickFolder ? tr("Choose a folder") : tr("Open a file")) : QDir::toNativeSeparators(m_dir);
    p.drawText(QRectF(58, 12, width() - 74, 36), Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(t, Qt::ElideMiddle, width() - 74));
    p.setPen(c.border); p.drawLine(0, 58, width(), 58); p.drawLine(0, height() - 62, width(), height() - 62);
    if (!m_err.isEmpty()) { p.setPen(c.danger); p.setFont(Theme::i().font(13)); p.drawText(QRectF(16, height() - 52, width() / 2.0, 36), Qt::AlignVCenter, m_err); }
}

// ============================================================ CommandPalette
namespace {
struct Cmd { const char* id; const char* title; const char* hint; Icon icon; const char* keywords; };
const Cmd kCmds[] = {
    {"playpause", "Play / pause", "Space", Icon::Play, "toggle"}, {"next", "Next", "Ctrl+F", Icon::Next, "skip"}, {"prev", "Previous", "Ctrl+B", Icon::Prev, "back"},
    {"stop", "Stop", "Ctrl+S", Icon::Stop, ""}, {"shuffle", "Toggle shuffle", "Ctrl+H", Icon::Shuffle, "random"}, {"repeat", "Cycle repeat", "Ctrl+T", Icon::Repeat, "loop"},
    {"mute", "Mute", "M", Icon::Mute, "volume sound"}, {"fullscreen", "Fullscreen", "F", Icon::Fullscreen, "full screen"}, {"mini", "Mini player", "Ctrl+M", Icon::Mini, "pip floating small"},
    {"pin", "Always on top", "Ctrl+Shift+T", Icon::Pin, "float pin"}, {"sidebar", "Toggle library sidebar", "Tab or `",Icon::Sidebar, "library queue"},
    {"openfolder", "Open folder…", "Ctrl+O", Icon::Folder, "browse add"}, {"openfile", "Open file…", "", Icon::File, "browse"},
    {"adjust", "Adjust picture", "Ctrl+J", Icon::Adjust, "brightness contrast saturation gamma hue color"}, {"eq", "Equalizer", "Ctrl+E", Icon::Equalizer, "audio bass treble normalize"},
    {"annotate", "Annotate frame", "A", Icon::Pen, "draw markup highlight text note"}, {"screenshot", "Save frame as image", "Ctrl+Shift+S", Icon::Camera, "screenshot capture"},
    {"theme", "Switch theme", "Ctrl+D", Icon::Moon, "dark light"}, {"settings", "Settings", "Ctrl+,", Icon::Settings, "preferences options"},
    {"speedup", "Faster", "]", Icon::Speed, "speed"}, {"speeddown", "Slower", "[", Icon::Speed, "speed"}, {"speedreset", "Normal speed", "Backspace", Icon::Speed, "speed 1x"},
    {"framestep", "Next frame", ".", Icon::Next, "step"}, {"framestepback", "Previous frame", ",", Icon::Prev, "step"}, {"rotate", "Rotate image", "R", Icon::Redo, "turn"},
    {"music", "Music: artists, albums, songs", "", Icon::Music, "library artist album song tracks"}, {"playlists", "Playlists", "", Icon::Queue, "saved queue m3u"},
    {"saveplaylist", "Save the queue as a playlist…", "", Icon::Export, "playlist m3u store"},
    {"keep", "Keep this file (mark)", "Y", Icon::Check, "favorite star like good"}, {"reject", "Mark this file to reject", "N", Icon::Close, "bad skip no discard"},
    {"trash", "Delete this file (Recycle Bin)", "Delete", Icon::Trash, "remove recycle"}, {"trashrejected", "Delete all rejected files…", "", Icon::Trash, "remove recycle cleanup rejected"},
    {"nextunwatched", "Next file I haven't seen", "U", Icon::Next, "unwatched unseen new"},
    {"defaultplayer", "Make kinoz my default player", "", Icon::Play, "default app open with association file type"},
#ifdef Q_OS_WIN
    {"doctor", "Doctor: check and repair kinoz", "", Icon::Settings, "repair fix health setup"},
    {"uninstall", "Uninstall kinoz…", "", Icon::Trash, "remove delete uninstall data"},
#endif
    {"zoomreset", "Fit to window", "0", Icon::Search, "zoom reset"},{"subcycle", "Next subtitle track", "V", Icon::Subtitles, "captions cc"}, {"quit", "Quit", "Ctrl+Q", Icon::Close, "exit close"}};
}

CommandPalette::CommandPalette(PlayerController* ctl, LibraryIndex* idx, QWidget* parent) : QWidget(parent), m_ctl(ctl), m_idx(idx) {
    hide();
    m_input = new QLineEdit(this);
    m_input->setPlaceholderText(tr("Search your library or type a command…"));
    m_input->setFrame(false);
    m_input->installEventFilter(this);
    m_list = new RowList(this);
    m_list->setRowHeight(48);
    connect(m_input, &QLineEdit::textChanged, this, [this] { refresh(); });
    connect(m_list, &RowList::activated, this, &CommandPalette::activate);
    connect(idx, &LibraryIndex::updated, this, [this] { if (isVisible()) refresh(); });
    connect(&Theme::i(), &Theme::changed, this, [this] {
        const auto& c = pal();
        m_input->setStyleSheet(QStringLiteral("QLineEdit{background:transparent;color:%1;font-size:17px;selection-background-color:%2;selection-color:%3;}").arg(c.text.name(), c.accent.name(), c.onAccent.name()));
        m_input->setFont(Theme::i().font(17));
    });
    Theme::i().changed();
}

void CommandPalette::openPalette() {
    setGeometry(parentWidget()->rect());
    raise(); show();
    m_input->clear(); refresh(); layoutCard();
    m_input->setFocus();
}
void CommandPalette::closePalette() { hide(); if (parentWidget()) parentWidget()->setFocus(); }

void CommandPalette::refresh() {
    QVector<RowList::Row> rows;
    const QString q = m_input->text().trimmed().toLower();
    const QStringList toks = q.split(' ', Qt::SkipEmptyParts);
    int nCmd = 0;
    QVector<RowList::Row> cmds;
    for (const auto& c : kCmds) {
        const QString hay = (QString::fromLatin1(c.title) + ' ' + QString::fromLatin1(c.keywords)).toLower();
        bool ok = true; for (const auto& t : toks) if (!hay.contains(t)) { ok = false; break; }
        if (!ok) continue;
        cmds.push_back({c.icon, tr(c.title), {}, QString::fromLatin1(c.hint), QStringLiteral("cmd:") + QString::fromLatin1(c.id), false});
        if (++nCmd >= (q.isEmpty() ? 6 : 5)) break;
    }
    QVector<RowList::Row> files;
    if (q.isEmpty()) {
        for (const QString& p : m_ctl->recent().mid(0, 6)) {
            const MediaKind k = Media::kindFor(p);
            files.push_back({k == MediaKind::Video ? Icon::Video : k == MediaKind::Audio ? Icon::Music : k == MediaKind::Gif ? Icon::Gif : Icon::Image, QFileInfo(p).completeBaseName(), QFileInfo(p).dir().dirName(), {}, p, false});
        }
        if (!files.isEmpty()) rows.push_back({Icon::File, tr("Recent"), {}, {}, {}, true});
        rows += files;
        rows.push_back({Icon::Settings, tr("Commands"), {}, {}, {}, true});
        rows += cmds;
    } else {
        const auto hits = m_idx->search(q, 30);
        for (const auto& e : hits)
            files.push_back({e.kind == MediaKind::Video ? Icon::Video : e.kind == MediaKind::Audio ? Icon::Music : e.kind == MediaKind::Gif ? Icon::Gif : Icon::Image, e.name, QFileInfo(e.path).dir().dirName(), {}, e.path, false});
        if (!cmds.isEmpty()) { rows.push_back({Icon::Settings, tr("Commands"), {}, {}, {}, true}); rows += cmds; }
        if (!files.isEmpty()) { rows.push_back({Icon::File, tr("Your library"), {}, {}, {}, true}); rows += files; }
        if (files.isEmpty() && cmds.isEmpty()) m_list->setEmptyText(m_idx->busy() ? tr("Still indexing your folders…") : (m_idx->count() == 0 ? tr("Add folders in the sidebar to search them here.") : tr("No matches")));
    }
    m_list->setRows(rows);
    layoutCard();
    update();
}

void CommandPalette::layoutCard() {
    const int w = qMin(640, width() - 40);
    const int listH = qBound(80, m_list->contentHeight() + 8, qMin(420, height() - 160));
    m_card = QRect((width() - w) / 2, qMax(40, height() / 7), w, 60 + listH);
    m_input->setGeometry(m_card.left() + 52, m_card.top() + 8, w - 72, 44);
    m_list->setGeometry(m_card.left() + 4, m_card.top() + 60, w - 8, listH);
}

void CommandPalette::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.fillRect(rect(), c.scrim);
    for (int i = 8; i >= 1; --i) { p.setPen(Qt::NoPen); p.setBrush(withA(c.shadow, 0.03 * (9 - i))); p.drawRoundedRect(QRectF(m_card).adjusted(-i, -i + 4, i, i + 4), 14 + i, 14 + i); }
    p.setBrush(c.bg); p.setPen(QPen(c.border, 1)); p.drawRoundedRect(QRectF(m_card), 14, 14);
    drawIcon(&p, Icon::Search, QRectF(m_card.left() + 18, m_card.top() + 18, 24, 24), c.muted, 1.8);
    p.setPen(c.border); p.drawLine(m_card.left() + 1, m_card.top() + 59, m_card.right() - 1, m_card.top() + 59);
}

void CommandPalette::mousePressEvent(QMouseEvent* e) { if (!m_card.contains(e->pos())) closePalette(); }

bool CommandPalette::eventFilter(QObject* o, QEvent* e) {
    if (o == m_input && e->type() == QEvent::KeyPress) {
        auto* k = static_cast<QKeyEvent*>(e);
        switch (k->key()) {
        case Qt::Key_Down: m_list->move(1); return true;
        case Qt::Key_Up: m_list->move(-1); return true;
        case Qt::Key_Escape: closePalette(); return true;
        case Qt::Key_Return: case Qt::Key_Enter: activate(m_list->current()); return true;
        default: break;
        }
    }
    return QWidget::eventFilter(o, e);
}

void CommandPalette::activate(int row) {
    if (row < 0 || row >= m_list->rows().size()) return;
    const QString d = m_list->rows()[row].data;
    closePalette();
    if (d.startsWith("cmd:")) emit command(d.mid(4));
    else if (!d.isEmpty()) emit playFile(d);
}

// ============================================================ SidePanel
static const char* kBandNames[10] = {"31", "62", "125", "250", "500", "1k", "2k", "4k", "8k", "16k"};
struct Preset { const char* name; double g[10]; };
static const Preset kPresets[] = {
    {"Flat", {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}}, {"Bass boost", {6, 5, 4, 2, 0, 0, 0, 0, 0, 0}}, {"Treble boost", {0, 0, 0, 0, 0, 1, 2, 4, 5, 6}},
    {"Vocal", {-2, -2, -1, 1, 3, 3, 2, 1, 0, -1}}, {"Rock", {5, 4, 2, -1, -2, 0, 2, 4, 5, 5}}, {"Pop", {-1, 2, 4, 4, 2, 0, -1, -1, 2, 3}},
    {"Classical", {4, 3, 2, 1, 0, 0, 0, 1, 3, 4}}, {"Jazz", {3, 2, 1, 2, -2, -2, 0, 1, 3, 4}}, {"Loudness", {6, 4, 0, 0, -2, 0, -1, 0, 4, 2}}};

SidePanel::SidePanel(PlayerController* ctl, JsonStore* set, QWidget* parent) : QWidget(parent), m_ctl(ctl), m_set(set) {
    hide();
    m_close = new IconButton(Icon::Close, this, 30);
    connect(m_close, &QAbstractButton::clicked, this, [this] { slideOut(); emit closedByUser(); });
    m_eqTimer = new QTimer(this); m_eqTimer->setSingleShot(true); m_eqTimer->setInterval(120);
    connect(m_eqTimer, &QTimer::timeout, this, [this] { m_ctl->applyEq(); });
    m_anim.setEasingCurve(Theme::easeMac());
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_x = v.toReal(); move(parentWidget()->width() - int(kWidth * (1.0 - m_x)), 0); if (m_x >= 0.999 && !m_open) hide(); });
    build();
}

void SidePanel::build() {
    // --- Adjust picture
    static const char* props[] = {"brightness", "contrast", "saturation", "gamma", "hue"};
    static const char* names[] = {"Brightness", "Contrast", "Saturation", "Gamma", "Hue"};
    for (int i = 0; i < 5; ++i) {
        auto* s = new Slider(this); s->setRange(-100, 100); s->setDefault(0); s->setValue(m_ctl->colorValue(QString::fromLatin1(props[i])));
        connect(s, &Slider::valueChanged, this, [this, i](double v) { m_ctl->setColor(QString::fromLatin1(props[i]), int(v)); update(); });
        m_colorSliders << s; m_adjustWidgets << s; Q_UNUSED(names)
    }
    m_reset = new TextButton(tr("Reset"), this);
    connect(m_reset, &QAbstractButton::clicked, this, [this] { m_ctl->resetColor(); for (int i = 0; i < m_colorSliders.size(); ++i) m_colorSliders[i]->setValue(0); update(); });
    m_adjustWidgets << m_reset;
    // --- Equalizer
    m_eqOn = new Switch(this);
    m_eqOn->setChecked(m_set->flag("eq.enabled", false));
    connect(m_eqOn, &QAbstractButton::toggled, this, [this](bool on) { m_set->set("eq.enabled", on); applyEqSoon(); });
    m_eqWidgets << m_eqOn;
    m_preset = new TextButton(tr("Presets"), this);
    connect(m_preset, &QAbstractButton::clicked, this, [this] {
        auto* m = new PopupMenu(this);
        const QString cur = m_set->str("eq.preset", "Flat");
        for (const auto& p : kPresets) m->addItem(QString::fromLatin1(p.name), tr(p.name), cur == QString::fromLatin1(p.name));
        connect(m, &PopupMenu::triggered, this, [this](const QString& id) {
            for (const auto& p : kPresets) if (id == QString::fromLatin1(p.name)) { QJsonArray a; for (double g : p.g) a.append(g); m_set->set("eq.bands", a); m_set->set("eq.preset", id); m_set->set("eq.enabled", true); m_set->set("eq.preamp", 0.0); loadEqUi(); applyEqSoon(); }
        });
        m->popup(m_preset->mapToGlobal(QPoint(0, m_preset->height() + 4)));
    });
    m_eqWidgets << m_preset;
    m_pre = new Slider(this); m_pre->setRange(-12, 12); m_pre->setDefault(0);
    connect(m_pre, &Slider::valueChanged, this, [this](double v) { m_set->set("eq.preamp", v); m_set->set("eq.enabled", true); m_eqOn->setChecked(true); applyEqSoon(); update(); });
    m_eqWidgets << m_pre;
    for (int i = 0; i < 10; ++i) {
        auto* s = new Slider(this); s->setRange(-12, 12); s->setDefault(0);
        connect(s, &Slider::valueChanged, this, [this, i](double v) {
            QJsonArray a = m_set->get("eq.bands").toArray(); while (a.size() < 10) a.append(0.0);
            a[i] = v; m_set->set("eq.bands", a); m_set->set("eq.preset", "Custom"); m_set->set("eq.enabled", true); m_eqOn->setChecked(true); applyEqSoon(); update();
        });
        m_bands << s; m_eqWidgets << s;
    }
    m_norm = new Switch(this);
    m_norm->setChecked(m_set->flag("normalize", false));
    connect(m_norm, &QAbstractButton::toggled, this, [this](bool on) { m_set->set("normalize", on); applyEqSoon(); });
    m_eqWidgets << m_norm;
    m_device = new TextButton(tr("Output device"), this);
    connect(m_device, &QAbstractButton::clicked, this, &SidePanel::showDevices);
    m_eqWidgets << m_device;
    loadEqUi();
    for (auto* w : m_adjustWidgets) w->hide();
    for (auto* w : m_eqWidgets) w->hide();
}

void SidePanel::loadEqUi() {
    const QJsonArray a = m_set->get("eq.bands").toArray();
    for (int i = 0; i < m_bands.size(); ++i) m_bands[i]->setValue(i < a.size() ? a[i].toDouble() : 0.0);
    m_pre->setValue(m_set->num("eq.preamp", 0));
    m_eqOn->setChecked(m_set->flag("eq.enabled", false));
    update();
}

void SidePanel::applyEqSoon() { m_eqTimer->start(); }

void SidePanel::showDevices() {
    auto* m = new PopupMenu(this);
    const QVariantList devs = m_ctl->mpv()->getProp("audio-device-list").toList();
    const QString cur = m_ctl->mpv()->getProp("audio-device").toString();
    m->addHeader(tr("Output device"));
    for (const auto& d : devs) { const auto dm = d.toMap(); m->addItem(dm["name"].toString(), dm["description"].toString(), dm["name"].toString() == cur); }
    connect(m, &PopupMenu::triggered, this, [this](const QString& id) { m_ctl->mpv()->setProp("audio-device", id); emit toast(tr("Output device changed")); });
    m->popup(m_device->mapToGlobal(QPoint(0, m_device->height() + 4)));
}

void SidePanel::showPage(const QString& page) {
    m_page = page;
    for (auto* w : m_adjustWidgets) w->setVisible(page == "adjust");
    for (auto* w : m_eqWidgets) w->setVisible(page == "eq");
    if (page == "adjust") for (int i = 0; i < m_colorSliders.size(); ++i) { static const char* props[] = {"brightness", "contrast", "saturation", "gamma", "hue"}; m_colorSliders[i]->setValue(m_ctl->colorValue(QString::fromLatin1(props[i]))); }
    if (page == "eq") loadEqUi();
    setFixedWidth(kWidth);
    setFixedHeight(parentWidget()->height());
    layoutPage();
    m_open = true;
    raise(); show();
    m_anim.stop(); m_anim.setDuration(Theme::i().animMs(220)); m_anim.setStartValue(m_x); m_anim.setEndValue(0.0);
    if (m_anim.duration() == 0) { m_x = 0; move(parentWidget()->width() - kWidth, 0); } else m_anim.start();
    update();
}

void SidePanel::slideOut() {
    m_open = false;
    m_anim.stop(); m_anim.setDuration(Theme::i().animMs(180)); m_anim.setStartValue(m_x); m_anim.setEndValue(1.0);
    if (m_anim.duration() == 0) { m_x = 1; hide(); } else m_anim.start();
}

void SidePanel::resizeEvent(QResizeEvent*) { layoutPage(); }

void SidePanel::layoutPage() {
    m_close->move(width() - 12 - 30, 56);
    const int w = width() - 40;
    if (m_page == "adjust") {
        int y = 130;
        for (auto* s : m_colorSliders) { s->setGeometry(20, y + 24, w, 22); y += 62; }
        m_reset->setGeometry(20, y + 10, 90, 36);
    } else if (m_page == "eq") {
        m_eqOn->move(width() - 20 - 38, 118);
        m_preset->setGeometry(20, 164, 110, 36);
        m_device->setGeometry(140, 164, qMin(160, w - 120), 36);
        m_pre->setGeometry(20 + 56, 214, w - 56 - 40, 22);
        int y = 248;
        for (auto* s : m_bands) { s->setGeometry(20 + 56, y, w - 56 - 40, 22); y += 28; }
        m_norm->move(width() - 20 - 38, y + 22);
    }
}

void SidePanel::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.fillRect(rect(), c.bg);
    p.setPen(c.border); p.drawLine(0, 0, 0, height());
    p.setPen(c.text); p.setFont(Theme::i().font(18, QFont::DemiBold, true));
    p.drawText(QRectF(20, 52, width() - 70, 38), Qt::AlignVCenter, m_page == "eq" ? tr("Equalizer") : tr("Adjust picture"));
    if (m_page == "adjust") {
        static const char* names[] = {"Brightness", "Contrast", "Saturation", "Gamma", "Hue"};
        static const char* props[] = {"brightness", "contrast", "saturation", "gamma", "hue"};
        int y = 130;
        for (int i = 0; i < 5; ++i) {
            p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::Medium)); p.drawText(QRectF(20, y, 160, 22), Qt::AlignVCenter, tr(names[i]));
            p.setPen(c.muted); p.setFont(Theme::i().font(13)); p.drawText(QRectF(width() - 80, y, 60, 22), Qt::AlignVCenter | Qt::AlignRight, QString::number(m_ctl->colorValue(QString::fromLatin1(props[i]))));
            y += 62;
        }
        p.setPen(c.muted); p.setFont(Theme::i().font(12));
        p.drawText(QRectF(20, y + 60, width() - 40, 40), Qt::AlignTop | Qt::TextWordWrap, tr("Double-click a slider to reset it. Changes apply live and are remembered."));
    } else if (m_page == "eq") {
        p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::Medium)); p.drawText(QRectF(20, 114, 200, 30), Qt::AlignVCenter, tr("Enable equalizer"));
        p.setPen(c.muted); p.setFont(Theme::i().font(12)); p.drawText(QRectF(20, 142, 280, 18), Qt::AlignVCenter, tr("Preset: %1").arg(m_set->str("eq.preset", "Flat")));
        p.setPen(c.text); p.setFont(Theme::i().font(12, QFont::Medium));
        p.drawText(QRectF(20, 214, 56, 22), Qt::AlignVCenter, tr("Preamp"));
        int y = 248;
        for (int i = 0; i < 10; ++i) {
            p.setPen(c.muted); p.drawText(QRectF(20, y, 50, 22), Qt::AlignVCenter, QString::fromLatin1(kBandNames[i]) + " Hz");
            p.drawText(QRectF(width() - 44, y, 30, 22), Qt::AlignVCenter | Qt::AlignRight, QString::number(m_bands[i]->value(), 'f', 0));
            y += 28;
        }
        p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::Medium)); p.drawText(QRectF(20, y + 18, 220, 30), Qt::AlignVCenter, tr("Normalize loudness"));
    }
}

// ============================================================ PreviewTip
PreviewTip::PreviewTip(QWidget* parent) : QWidget(parent) { setAttribute(Qt::WA_TransparentForMouseEvents); hide(); }
void PreviewTip::showFrame(const QImage& img, const QString& t, const QPoint& a) {
    m_img = img; m_time = t;
    const int w = img.width() / img.devicePixelRatio() + 12, h = img.height() / img.devicePixelRatio() + 36;
    resize(w, h);
    move(qBound(8, a.x() - w / 2, parentWidget()->width() - w - 8), a.y() - h);
    raise(); show(); update();
}
void PreviewTip::showTimeOnly(const QString& t, const QPoint& a) {
    m_img = QImage(); m_time = t;
    const int w = 64, h = 28;
    resize(w, h);
    move(qBound(8, a.x() - w / 2, parentWidget()->width() - w - 8), a.y() - h);
    raise(); show(); update();
}
void PreviewTip::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(QColor(255, 255, 255, 40), 1)); p.setBrush(pal().overlay);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    if (!m_img.isNull()) {
        QPainterPath clip; clip.addRoundedRect(QRectF(6, 6, width() - 12, height() - 36), 6, 6);
        p.save(); p.setClipPath(clip); p.drawImage(QRectF(6, 6, width() - 12, height() - 36), m_img); p.restore();
    }
    p.setPen(pal().overlayText); p.setFont(Theme::i().font(12, QFont::DemiBold));
    p.drawText(QRectF(0, m_img.isNull() ? 0 : height() - 30, width(), m_img.isNull() ? height() : 28), Qt::AlignCenter, m_time);
}


