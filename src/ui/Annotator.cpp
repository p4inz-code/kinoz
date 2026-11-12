// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/Annotator.h"
#include "core/Paths.h"
#include "core/player/MpvPlayer.h"
#include "core/player/PlayerController.h"
#include "ui/Theme.h"
#include <QClipboard>
#include <QDateTime>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

static const QColor kSwatches[] = {QColor("#ff4d4f"), QColor("#ff9f1a"), QColor("#ffd60a"), QColor("#30d158"), QColor("#4fd1d6"), QColor("#ffffff")};
static const qreal kWidths[] = {0.0035, 0.0065, 0.011};     // fraction of media width

Annotator::Annotator(PlayerController* ctl, QWidget* parent) : QWidget(parent), m_ctl(ctl), m_mpv(ctl->mpv()) {
    hide();
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::CrossCursor);
    m_edit = new QLineEdit(this);
    m_edit->hide();
    connect(m_edit, &QLineEdit::returnPressed, this, &Annotator::commitText);
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

void Annotator::begin() {
    m_ctl->pause();
    m_ctl->resetView();
    m_strokes.clear(); m_redo.clear(); m_drawing = false;
    setGeometry(parentWidget()->rect());
    relayout();
    raise(); show(); setFocus();
    update();
}

void Annotator::end() { m_edit->hide(); hide(); emit finished(); }

QRectF Annotator::videoRect() const {
    const int vw = m_mpv->dispW(), vh = m_mpv->dispH();
    if (vw <= 0 || vh <= 0) return QRectF(rect());
    const qreal s = qMin(qreal(width()) / vw, qreal(height()) / vh);
    const qreal w = vw * s, h = vh * s;
    return QRectF((width() - w) / 2, (height() - h) / 2, w, h);
}
QPointF Annotator::toNorm(const QPointF& p) const { const QRectF r = videoRect(); return QPointF((p.x() - r.left()) / r.width(), (p.y() - r.top()) / r.height()); }
QPointF Annotator::fromNorm(const QPointF& n, const QRectF& r) const { return QPointF(r.left() + n.x() * r.width(), r.top() + n.y() * r.height()); }

void Annotator::relayout() {
    m_items.clear();
    const struct { const char* id; Icon ic; } tools[] = {{"t0", Icon::Pen}, {"t1", Icon::Highlighter}, {"t2", Icon::Arrow}, {"t3", Icon::Rect}, {"t4", Icon::Ellipse}, {"t5", Icon::Text}};
    const struct { const char* id; Icon ic; } acts[] = {{"undo", Icon::Undo}, {"redo", Icon::Redo}, {"clear", Icon::Trash}};
    const struct { const char* id; Icon ic; } outs[] = {{"save", Icon::Export}, {"copy", Icon::Copy}, {"done", Icon::Close}};
    const qreal total = 6 * 36 + 10 + 6 * 26 + 10 + 3 * 26 + 10 + 3 * 36 + 10 + 3 * 36 + 28;
    qreal x = (width() - total) / 2, y = 62;
    const qreal h = 48;
    m_bar = QRectF(x, y, total, h);
    x += 14;
    const qreal cy = y + h / 2;
    for (const auto& t : tools) { m_items.push_back({t.id, QRectF(x, cy - 16, 32, 32), t.ic, {}}); x += 36; }
    m_items.push_back({"", QRectF(x + 3, cy - 12, 1, 24), Icon::Pen, {}, true}); x += 10;
    for (int i = 0; i < 6; ++i) { m_items.push_back({QStringLiteral("c%1").arg(i), QRectF(x, cy - 11, 22, 22), Icon::Pen, kSwatches[i]}); x += 26; }
    m_items.push_back({"", QRectF(x + 3, cy - 12, 1, 24), Icon::Pen, {}, true}); x += 10;
    for (int i = 0; i < 3; ++i) { m_items.push_back({QStringLiteral("w%1").arg(i), QRectF(x, cy - 11, 22, 22), Icon::Pen, {}}); x += 26; }
    m_items.push_back({"", QRectF(x + 3, cy - 12, 1, 24), Icon::Pen, {}, true}); x += 10;
    for (const auto& a : acts) { m_items.push_back({a.id, QRectF(x, cy - 16, 32, 32), a.ic, {}}); x += 36; }
    m_items.push_back({"", QRectF(x + 3, cy - 12, 1, 24), Icon::Pen, {}, true}); x += 10;
    for (const auto& o : outs) { m_items.push_back({o.id, QRectF(x, cy - 16, 32, 32), o.ic, {}}); x += 36; }
}

void Annotator::drawStroke(QPainter& p, const Stroke& s, const QRectF& area, qreal) const {
    if (s.pts.isEmpty()) return;
    QColor col = s.color;
    qreal w = s.width * area.width();
    if (s.tool == Highlighter) { col.setAlpha(95); w *= 3.4; }
    QPen pen(col, qMax(1.0, w), Qt::SolidLine, s.tool == Highlighter ? Qt::FlatCap : Qt::RoundCap, Qt::RoundJoin);
    p.setPen(pen); p.setBrush(Qt::NoBrush);
    auto P = [&](int i) { return fromNorm(s.pts[i], area); };
    switch (s.tool) {
    case Pen: case Highlighter: {
        QPainterPath path; path.moveTo(P(0));
        for (int i = 1; i < s.pts.size(); ++i) { const QPointF a = P(i - 1), b = P(i); path.quadTo(a, (a + b) / 2); }
        path.lineTo(P(s.pts.size() - 1));
        if (s.pts.size() == 1) p.drawPoint(P(0)); else p.drawPath(path);
        break; }
    case Arrow: {
        if (s.pts.size() < 2) break;
        const QPointF a = P(0), b = P(s.pts.size() - 1);
        p.drawLine(a, b);
        const QLineF l(b, a); const qreal len = qMax(12.0, w * 4.5);
        QLineF h1 = l; h1.setLength(len); h1.setAngle(l.angle() + 28);
        QLineF h2 = l; h2.setLength(len); h2.setAngle(l.angle() - 28);
        p.drawLine(b, h1.p2()); p.drawLine(b, h2.p2());
        break; }
    case Rect: if (s.pts.size() >= 2) p.drawRoundedRect(QRectF(P(0), P(s.pts.size() - 1)).normalized(), w, w); break;
    case Ellipse: if (s.pts.size() >= 2) p.drawEllipse(QRectF(P(0), P(s.pts.size() - 1)).normalized()); break;
    case Text: {
        QFont f = Theme::i().font(qMax(10.0, s.width * 9 * area.width()), QFont::DemiBold);
        p.setFont(f); p.setPen(col);
        p.drawText(P(0) + QPointF(0, 0), s.text);
        break; }
    }
}

void Annotator::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRectF vr = videoRect();
    p.save(); p.setClipRect(vr);
    for (const auto& s : m_strokes) drawStroke(p, s, vr, 1);
    if (m_drawing) drawStroke(p, m_cur, vr, 1);
    p.restore();
    // toolbar (glass-like pill)
    const auto& c = pal();
    for (int i = 5; i >= 1; --i) { p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 12 * (6 - i))); p.drawRoundedRect(m_bar.adjusted(-i, -i + 3, i, i + 3), 24 + i, 24 + i); }
    p.setBrush(QColor(22, 22, 18, 225)); p.setPen(QPen(QColor(255, 255, 255, 38), 1)); p.drawRoundedRect(m_bar, 24, 24);
    for (int i = 0; i < m_items.size(); ++i) {
        const Item& it = m_items[i];
        if (it.sep) { p.setPen(QColor(255, 255, 255, 40)); p.drawLine(it.r.topLeft(), it.r.bottomLeft()); continue; }
        const bool hov = i == m_hover;
        bool on = false;
        if (it.id.startsWith('t')) on = it.id.mid(1).toInt() == int(m_tool);
        if (it.id.startsWith('w')) on = it.id.mid(1).toInt() == m_widthIdx;
        if (it.swatch.isValid()) {
            p.setPen(Qt::NoPen); p.setBrush(it.swatch); p.drawEllipse(it.r.adjusted(2, 2, -2, -2));
            if (it.swatch == m_color) { p.setBrush(Qt::NoBrush); p.setPen(QPen(Qt::white, 2)); p.drawEllipse(it.r.adjusted(-1, -1, 1, 1)); }
            continue;
        }
        if (it.id.startsWith('w')) {
            const qreal d = 5 + it.id.mid(1).toInt() * 4;
            p.setPen(Qt::NoPen); p.setBrush(on ? c.overlayAccent : QColor(255, 255, 255, hov ? 230 : 160)); p.drawEllipse(it.r.center(), d / 2, d / 2);
            continue;
        }
        if (on || hov) { p.setPen(Qt::NoPen); p.setBrush(on ? QColor(79, 209, 214, 46) : QColor(255, 255, 255, 28)); p.drawRoundedRect(it.r, 9, 9); }
        QColor fg = on ? c.overlayAccent : c.overlayText;
        if ((it.id == "undo" && m_strokes.isEmpty()) || (it.id == "redo" && m_redo.isEmpty()) || (it.id == "clear" && m_strokes.isEmpty())) fg.setAlpha(80);
        drawIcon(&p, it.icon, it.r.adjusted(5, 5, -5, -5), fg, 1.7);
    }
}

void Annotator::clickItem(const QString& id) {
    if (id.startsWith('t')) { m_tool = Tool(id.mid(1).toInt()); m_edit->hide(); }
    else if (id.startsWith('c')) m_color = kSwatches[id.mid(1).toInt()];
    else if (id.startsWith('w')) m_widthIdx = id.mid(1).toInt();
    else if (id == "undo") { if (!m_strokes.isEmpty()) { m_redo << m_strokes.takeLast(); } }
    else if (id == "redo") { if (!m_redo.isEmpty()) m_strokes << m_redo.takeLast(); }
    else if (id == "clear") { m_redo = m_strokes; m_strokes.clear(); }
    else if (id == "save") doExport(false);
    else if (id == "copy") doExport(true);
    else if (id == "done") end();
    update();
}

void Annotator::mousePressEvent(QMouseEvent* e) {
    setFocus();
    if (e->button() != Qt::LeftButton) return;
    for (const auto& it : m_items) if (!it.sep && it.r.contains(e->position())) { clickItem(it.id); return; }
    if (m_bar.contains(e->position())) return;
    if (!videoRect().contains(e->position())) return;
    if (m_tool == Text) {
        commitText();
        m_textAt = e->position();
        m_edit->setStyleSheet(QStringLiteral("QLineEdit{background:rgba(20,20,15,200);color:%1;border:1px solid %2;border-radius:6px;padding:2px 8px;font-size:18px;}").arg(m_color.name(), m_color.name()));
        m_edit->setGeometry(int(e->position().x()), int(e->position().y()) - 18, 240, 36);
        m_edit->clear(); m_edit->show(); m_edit->setFocus();
        return;
    }
    m_drawing = true; m_redo.clear();
    m_cur = Stroke{m_tool, m_color, kWidths[m_widthIdx], {toNorm(e->position())}, {}};
    update();
}

void Annotator::mouseMoveEvent(QMouseEvent* e) {
    int h = -1;
    for (int i = 0; i < m_items.size(); ++i) if (!m_items[i].sep && m_items[i].r.contains(e->position())) h = i;
    if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::CrossCursor); update(); }
    if (!m_drawing) return;
    const QPointF n = toNorm(e->position());
    if (m_cur.tool == Pen || m_cur.tool == Highlighter) m_cur.pts << n;
    else { if (m_cur.pts.size() < 2) m_cur.pts << n; else m_cur.pts[1] = n; }
    update();
}

void Annotator::mouseReleaseEvent(QMouseEvent*) {
    if (!m_drawing) return;
    m_drawing = false;
    if (m_cur.pts.size() >= 1) m_strokes << m_cur;
    update();
}

void Annotator::commitText() {
    if (!m_edit->isVisible()) return;
    const QString t = m_edit->text().trimmed();
    m_edit->hide();
    if (t.isEmpty()) return;
    m_strokes << Stroke{Text, m_color, kWidths[m_widthIdx], {toNorm(m_textAt + QPointF(8, 8))}, t};
    m_redo.clear();
    setFocus(); update();
}

void Annotator::keyPressEvent(QKeyEvent* e) {
    if (e->key() == Qt::Key_Escape) { if (m_edit->isVisible()) { m_edit->hide(); setFocus(); } else end(); return; }
    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Z) { clickItem((e->modifiers() & Qt::ShiftModifier) ? "redo" : "undo"); return; }
    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_Y) { clickItem("redo"); return; }
    if ((e->modifiers() & Qt::ControlModifier) && e->key() == Qt::Key_S) { clickItem("save"); return; }
    if (e->key() >= Qt::Key_1 && e->key() <= Qt::Key_6) { m_tool = Tool(e->key() - Qt::Key_1); update(); return; }
    QWidget::keyPressEvent(e);
}

bool Annotator::doExport(bool clipboard) {
    const QString tmp = Paths::tmpDir() + QStringLiteral("/annot-src.png");
    QFile::remove(tmp);
    if (!m_mpv->commandSync({QStringLiteral("screenshot-to-file"), tmp, QStringLiteral("video")})) { emit message(tr("Couldn't capture the frame"), true); return false; }
    QImage img(tmp);
    if (img.isNull()) { emit message(tr("Couldn't read the captured frame"), true); return false; }
    img = img.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing);
    for (const auto& s : m_strokes) drawStroke(p, s, QRectF(img.rect()), 1);
    p.end();
    QFile::remove(tmp);
    if (clipboard) { QGuiApplication::clipboard()->setImage(img); emit message(tr("Copied to clipboard"), false); return true; }
    const QString out = Paths::exportsDir() + QStringLiteral("/annotated-") + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss") + QStringLiteral(".png");
    if (!img.save(out, "PNG")) { emit message(tr("Couldn't save the image"), true); return false; }
    emit message(tr("Saved to exports folder"), false);
    emit exported(out);
    return true;
}
