// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/Widgets.h"
#include "ui/HoverTip.h"
#include "ui/Theme.h"
#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QScreen>
#include <QTimer>
#include <QWheelEvent>
#include <cmath>

// Buttons shrink a touch while held, like a physical key (no timer: it follows the pointer press exactly).
static void pressScale(QPainter& p, const QRectF& r, bool down, qreal s = 0.95) {
    if (!down) return;
    p.translate(r.center()); p.scale(s, s); p.translate(-r.center());
}

static QColor withA(QColor c, qreal a) { c.setAlphaF(a); return c; }

// ================= IconButton =================
IconButton::IconButton(Icon ic, QWidget* parent, int size) : QAbstractButton(parent), m_icon(ic), m_size(size) {
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(size, size);
    m_anim.setDuration(Theme::i().animMs(120));
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_hover = v.toReal(); update(); });
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
    connect(this, &QAbstractButton::pressed, this, [] { HoverTip::cancel(); });
}
bool IconButton::event(QEvent* e) {
    if (e->type() == QEvent::ToolTip) return true;                              // the themed label (HoverTip) replaces Qt's own tooltip
    return QAbstractButton::event(e);
}
void IconButton::enterEvent(QEnterEvent*) {
    if (!toolTip().isEmpty() && isEnabled()) HoverTip::request(QRect(mapToGlobal(QPoint(0, 0)), size()), toolTip());
    m_anim.stop(); m_anim.setDuration(Theme::i().animMs(120));
    if (m_anim.duration() == 0) { m_hover = 1; update(); return; }
    m_anim.setStartValue(m_hover); m_anim.setEndValue(1.0); m_anim.start();
}
void IconButton::leaveEvent(QEvent*) {
    HoverTip::cancel();
    m_anim.stop(); m_anim.setDuration(Theme::i().animMs(120));
    if (m_anim.duration() == 0) { m_hover = 0; update(); return; }
    m_anim.setStartValue(m_hover); m_anim.setEndValue(0.0); m_anim.start();
}
void IconButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    const QColor base = m_overlay ? c.overlayText : c.text;
    QColor fg = (m_active || isChecked()) ? (m_overlay ? c.overlayAccent : c.accent) : base;
    if (!isEnabled()) fg.setAlphaF(0.35);
    const QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
    pressScale(p, r, isDown() && Theme::i().animMs(1) > 0, 0.92);
    QColor bg = m_overlay ? QColor(255, 255, 255) : c.text;
    if (isDown()) bg.setAlphaF(0.18); else bg.setAlphaF(0.10 * m_hover);
    p.setPen(Qt::NoPen);
    p.setBrush(bg);
    p.drawRoundedRect(r, Theme::i().radius(), Theme::i().radius());
    const qreal ic = qMin(width(), height()) * 0.62;
    drawIcon(&p, m_icon, QRectF((width() - ic) / 2, (height() - ic) / 2, ic, ic), fg);
}

// ================= TextButton =================
TextButton::TextButton(const QString& text, QWidget* parent, bool primary, bool hasIcon, Icon ic)
    : QAbstractButton(parent), m_primary(primary), m_hasIcon(hasIcon), m_icon(ic) {
    setText(text);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
}
QSize TextButton::sizeHint() const {
    QFontMetrics fm(Theme::i().font(14, QFont::DemiBold));
    return {fm.horizontalAdvance(text()) + 32 + (m_hasIcon ? 24 : 0), 36};
}
void TextButton::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    pressScale(p, r, isDown() && Theme::i().animMs(1) > 0, 0.97);
    QColor fg = c.text, bg = Qt::transparent;
    if (m_primary) { bg = m_danger ? c.danger : c.accent; fg = c.onAccent; if (m_hot || isDown()) bg = bg.lighter(m_hot ? 112 : 95); }
    else { bg = m_hot ? c.surface : Qt::transparent; if (isDown()) bg = c.sunken; if (m_danger) fg = c.danger; }
    p.setPen(m_primary ? Qt::NoPen : QPen(c.border, 1)); p.setBrush(bg);
    p.drawRoundedRect(r, 8, 8);
    if (!isEnabled()) p.setOpacity(0.45);
    p.setFont(Theme::i().font(14, QFont::DemiBold));
    p.setPen(fg);
    QRectF tr = r;
    if (m_hasIcon) { drawIcon(&p, m_icon, QRectF(r.left() + 12, r.center().y() - 9, 18, 18), fg, 1.8); tr.adjust(26, 0, 0, 0); }
    p.drawText(tr, Qt::AlignCenter, text());
}

// ================= Slider =================
Slider::Slider(QWidget* parent) : QWidget(parent) {
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(22);
    setMouseTracking(true);
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
}
void Slider::setValue(double v, bool emitSig) {
    v = qBound(m_min, v, m_max);
    if (qFuzzyCompare(v + 1, m_val + 1)) return;
    m_val = v;
    update();
    if (emitSig) emit valueChanged(v);
}
void Slider::setFromX(qreal x) {
    const qreal t = qBound(0.0, (x - 8) / qMax(1.0, width() - 16.0), 1.0);
    const double v = m_min + t * (m_max - m_min);
    m_val = v; update(); emit valueChanged(v);
}
void Slider::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    const qreal t = (m_max > m_min) ? (m_val - m_min) / (m_max - m_min) : 0;
    const QRectF tr(8, height() / 2.0 - 2, width() - 16, 4);
    p.setPen(Qt::NoPen);
    p.setBrush(c.sunken.darker(m_hot ? 108 : 100));
    p.setBrush(c.border);
    p.drawRoundedRect(tr, 2, 2);
    p.setBrush(c.accent);
    p.drawRoundedRect(QRectF(tr.left(), tr.top(), tr.width() * t, tr.height()), 2, 2);
    const qreal kr = (m_hot || m_down) ? 7 : 6;
    const QPointF kc(tr.left() + tr.width() * t, height() / 2.0);
    p.setBrush(withA(c.shadow, 0.5)); p.drawEllipse(kc + QPointF(0, 1), kr, kr);
    p.setBrush(c.raised); p.setPen(QPen(c.accent, 2)); p.drawEllipse(kc, kr - 1, kr - 1);
}
void Slider::mousePressEvent(QMouseEvent* e) { m_down = true; setFromX(e->position().x()); }
void Slider::mouseMoveEvent(QMouseEvent* e) { if (m_down) setFromX(e->position().x()); }
void Slider::mouseReleaseEvent(QMouseEvent*) { m_down = false; update(); }
void Slider::mouseDoubleClickEvent(QMouseEvent*) { m_val = m_def; update(); emit valueChanged(m_def); }
void Slider::wheelEvent(QWheelEvent* e) {
    const double step = (m_max - m_min) / 50.0 * (e->angleDelta().y() > 0 ? 1 : -1);
    m_val = qBound(m_min, m_val + step, m_max); update(); emit valueChanged(m_val); e->accept();
}

// ================= Switch =================
Switch::Switch(QWidget* parent) : QAbstractButton(parent) {
    setCheckable(true);
    setCursor(Qt::PointingHandCursor);
    setFixedSize(38, 22);
    m_anim.setEasingCurve(Theme::easeSpring());              // the knob overshoots a hair and settles
    m_anim.setDuration(Theme::i().animMs(260));
    connect(&m_anim, &QVariantAnimation::valueChanged, this, qOverload<>(&QWidget::update));
    connect(this, &QAbstractButton::toggled, this, [this](bool on) {
        m_anim.stop(); m_anim.setDuration(Theme::i().animMs(260));
        if (m_anim.duration() == 0) { update(); return; }
        m_anim.setStartValue(on ? 0.0 : 1.0); m_anim.setEndValue(on ? 1.0 : 0.0); m_anim.start();
    });
}
void Switch::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    const bool running = m_anim.state() == QAbstractAnimation::Running;  // (Switch)
    const qreal t = running ? m_anim.currentValue().toReal() : (isChecked() ? 1.0 : 0.0);     // may overshoot 1 for the knob
    const qreal tc = qBound(0.0, t, 1.0);
    QColor bg = QColor::fromRgbF(c.border.redF() + (c.accent.redF() - c.border.redF()) * tc, c.border.greenF() + (c.accent.greenF() - c.border.greenF()) * tc, c.border.blueF() + (c.accent.blueF() - c.border.blueF()) * tc);
    p.setPen(Qt::NoPen); p.setBrush(bg);
    p.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 10, 10);
    p.setBrush(tc > 0.5 ? c.onAccent : c.raised);
    const qreal stretch = running ? 2.5 * std::sin(qBound(0.0, tc, 1.0) * 3.14159265) : (isDown() ? 2.5 : 0.0);   // the knob stretches while it travels, like on macOS
    const qreal cx = 11 + 16 * t;
    p.drawRoundedRect(QRectF(cx - 7.5 - stretch / 2, 3.5, 15 + stretch, 15), 7.5, 7.5);
}

// ================= Segmented =================
Segmented::Segmented(const QStringList& labels, QWidget* parent) : QWidget(parent), m_labels(labels) {
    setCursor(Qt::PointingHandCursor);
    m_slide.setStartValue(0.0); m_slide.setEndValue(1.0); m_slide.setEasingCurve(Theme::easeMac());
    connect(&m_slide, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_t = v.toReal(); update(); });
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
    m_fromR = segRect(0);
}
QRectF Segmented::segRect(int i) const {
    QFontMetrics fm(Theme::i().font(13, QFont::Medium));
    qreal x = 4;
    for (int k = 0; k < m_labels.size(); ++k) {
        const qreal w = fm.horizontalAdvance(m_labels[k]) + 26;
        if (k == i) return QRectF(x, 3, w, height() - 6);
        x += w;
    }
    return {};
}
void Segmented::slideTo(int i) {
    const QRectF cur = m_slide.state() == QAbstractAnimation::Running ? QRectF(m_fromR.left() + (segRect(m_cur).left() - m_fromR.left()) * m_t, 3, m_fromR.width() + (segRect(m_cur).width() - m_fromR.width()) * m_t, height() - 6) : segRect(m_cur);
    m_fromR = cur;
    m_cur = i; m_t = 0.0;
    m_slide.stop(); m_slide.setDuration(Theme::i().animMs(320));
    if (m_slide.duration() == 0) { m_t = 1.0; m_fromR = segRect(i); update(); } else m_slide.start();
}
QSize Segmented::sizeHint() const {
    QFontMetrics fm(Theme::i().font(13, QFont::Medium));
    int w = 8;
    for (const auto& l : m_labels) w += fm.horizontalAdvance(l) + 26;
    return {w, 30};
}
void Segmented::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.setPen(Qt::NoPen); p.setBrush(c.sunken);
    p.drawRoundedRect(rect(), 8, 8);
    p.setFont(Theme::i().font(13, QFont::Medium));
    QFontMetrics fm(p.font());
    qreal x = 4;
    {   // the selection pill, mid-glide when the choice just changed
        const QRectF to = segRect(m_cur);
        const qreal t = qBound(0.0, m_t, 1.2);
        const QRectF pill(m_fromR.left() + (to.left() - m_fromR.left()) * t, 3, m_fromR.width() + (to.width() - m_fromR.width()) * t, height() - 6);
        p.setBrush(c.raised); p.setPen(QPen(c.border, 1)); p.drawRoundedRect(pill, 6, 6);
    }
    for (int i = 0; i < m_labels.size(); ++i) {
        const qreal w = fm.horizontalAdvance(m_labels[i]) + 26;
        const QRectF r(x, 3, w, height() - 6);
        p.setPen(i == m_cur ? c.text : c.muted);
        p.drawText(r, Qt::AlignCenter, m_labels[i]);
        x += w;
    }
}
void Segmented::mousePressEvent(QMouseEvent* e) {
    QFontMetrics fm(Theme::i().font(13, QFont::Medium));
    qreal x = 4;
    for (int i = 0; i < m_labels.size(); ++i) {
        const qreal w = fm.horizontalAdvance(m_labels[i]) + 26;
        if (e->position().x() >= x && e->position().x() < x + w) { if (i != m_cur) { slideTo(i); emit currentChanged(i); } return; }
        x += w;
    }
}

// ================= Stepper =================
void Stepper::change(int d) { const int n = qBound(m_min, m_v + d, m_max); if (n == m_v) return; m_v = n; update(); emit valueChanged(m_v); }
void Stepper::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.setPen(Qt::NoPen); p.setBrush(c.sunken); p.drawRoundedRect(rect(), 8, 8);
    p.setFont(Theme::i().font(15, QFont::Medium)); p.setPen(c.muted);
    p.drawText(QRectF(0, 0, 36, height()), Qt::AlignCenter, QStringLiteral("\u2212"));
    p.drawText(QRectF(width() - 36, 0, 36, height()), Qt::AlignCenter, QStringLiteral("+"));
    p.setFont(Theme::i().font(13, QFont::Medium)); p.setPen(c.text);
    p.drawText(QRectF(36, 0, width() - 72, height()), Qt::AlignCenter, QString::number(m_v) + QLatin1Char(' ') + m_suffix);
}
void Stepper::mousePressEvent(QMouseEvent* e) {
    const int big = (e->modifiers() & Qt::ShiftModifier) ? 5 : 1;
    if (e->position().x() < 40) change(-big); else if (e->position().x() > width() - 40) change(big);
}
void Stepper::wheelEvent(QWheelEvent* e) { change(e->angleDelta().y() > 0 ? 1 : -1); e->accept(); }

// ================= ScrollBar =================
ScrollBar::ScrollBar(Qt::Orientation o, QWidget* parent) : QScrollBar(o, parent) { setAttribute(Qt::WA_OpaquePaintEvent, false); }
void ScrollBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const int range = maximum() - minimum();
    if (range <= 0) return;
    const bool v = orientation() == Qt::Vertical;
    const qreal len = v ? height() : width();
    const qreal page = pageStep();
    const qreal handle = qMax(28.0, len * page / (range + page));
    const qreal pos = (len - handle) * (value() - minimum()) / range;
    QRectF r = v ? QRectF(width() - 7, pos, 4, handle) : QRectF(pos, height() - 7, handle, 4);
    if (m_hot || isSliderDown()) r.adjust(v ? -1 : 0, v ? 0 : -1, 0, 0);
    p.setPen(Qt::NoPen);
    p.setBrush(withA(pal().text, m_hot ? 0.38 : 0.2));
    p.drawRoundedRect(r, 3, 3);
}

// ================= PopupMenu =================
PopupMenu::PopupMenu(QWidget* parent) : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint | Qt::NoDropShadowWindowHint) {
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_DeleteOnClose);
    setMouseTracking(true);
}
int PopupMenu::itemH(const Item& it) const { return it.sep ? 9 : it.header ? 26 : Theme::i().rowH() ; }
int PopupMenu::itemY(int i) const { int y = kPad + 6; for (int k = 0; k < i; ++k) y += itemH(m_items[k]); return y; }
void PopupMenu::addItem(const QString& id, const QString& text, bool checked, const QString& hint, bool enabled) {
    Item it; it.id = id; it.text = text; it.checked = checked; it.hint = hint; it.enabled = enabled; m_items << it;
}
void PopupMenu::addSeparator() { Item it; it.sep = true; m_items << it; }
void PopupMenu::addHeader(const QString& t) { Item it; it.header = true; it.text = t; m_items << it; }
int PopupMenu::itemAt(const QPoint& pt) const {
    int y = kPad + 6;
    for (int i = 0; i < m_items.size(); ++i) {
        const int h = itemH(m_items[i]);
        if (pt.y() >= y && pt.y() < y + h && pt.x() >= kPad && pt.x() < width() - kPad) return (m_items[i].sep || m_items[i].header) ? -1 : i;
        y += h;
    }
    return -1;
}
void PopupMenu::popup(const QPoint& g) {
    QFontMetrics fm(Theme::i().font(14));
    int w = 180;
    for (const auto& it : m_items) w = qMax(w, fm.horizontalAdvance(it.text) + fm.horizontalAdvance(it.hint) + 78);
    int h = 12;
    for (const auto& it : m_items) h += itemH(it);
    m_w = w;
    resize(w + kPad * 2, h + kPad * 2);
    QRect avail = QGuiApplication::screenAt(g) ? QGuiApplication::screenAt(g)->availableGeometry() : QRect(0, 0, 1920, 1080);
    QPoint pos = g - QPoint(kPad, kPad);
    if (pos.x() + width() > avail.right()) pos.setX(avail.right() - width());
    if (pos.y() + height() > avail.bottom()) pos.setY(avail.bottom() - height());
    pos.setX(qMax(avail.left(), pos.x())); pos.setY(qMax(avail.top(), pos.y()));
    move(pos);
    // macOS-style appearance: a soft fade with a small rise (skipped when animations are off)
    const int ms = Theme::i().animMs(190);
    if (ms > 0) {
        setWindowOpacity(0.0);
        auto* a = new QVariantAnimation(this);
        a->setDuration(ms); a->setStartValue(0.0); a->setEndValue(1.0); a->setEasingCurve(Theme::easeMac());
        connect(a, &QVariantAnimation::valueChanged, this, [this, pos](const QVariant& v) { const qreal t = v.toReal(); setWindowOpacity(t); move(pos + QPoint(0, int((1.0 - t) * 8))); });
        connect(a, &QVariantAnimation::finished, a, &QObject::deleteLater);
        show();
        a->start();
    } else show();
    setFocus();
}
void PopupMenu::popupAbove(const QRect& a) {
    QFontMetrics fm(Theme::i().font(14));
    int h = 12; for (const auto& it : m_items) h += itemH(it);
    popup(QPoint(a.center().x() - 90, a.top() - h - 14));
}
void PopupMenu::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    const QRectF box = QRectF(kPad, kPad, width() - 2 * kPad, height() - 2 * kPad);
    for (int i = 6; i >= 1; --i) { p.setPen(Qt::NoPen); p.setBrush(withA(c.shadow, 0.035 * (7 - i))); p.drawRoundedRect(box.adjusted(-i, -i + 3, i, i + 3), 10 + i, 10 + i); }
    p.setBrush(c.raised); p.setPen(QPen(c.border, 1));
    p.drawRoundedRect(box, 10, 10);
    int y = kPad + 6;
    for (int i = 0; i < m_items.size(); ++i) {
        const Item& it = m_items[i];
        const int h = itemH(it);
        const QRectF r(kPad + 6, y, width() - 2 * kPad - 12, h);
        if (it.sep) { p.setPen(c.border); p.drawLine(QPointF(r.left() + 4, y + 4.5), QPointF(r.right() - 4, y + 4.5)); }
        else if (it.header) { p.setPen(c.muted); p.setFont(Theme::i().font(11, QFont::DemiBold)); p.drawText(r.adjusted(8, 4, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, it.text.toUpper()); }
        else {
            if (i == m_hover && it.enabled) { p.setPen(Qt::NoPen); p.setBrush(c.accentSubtle); p.drawRoundedRect(r, 6, 6); }
            QColor fg = it.enabled ? c.text : withA(c.text, 0.4);
            if (it.checked) drawIcon(&p, Icon::Check, QRectF(r.left() + 6, r.center().y() - 8, 16, 16), c.accent, 2);
            p.setPen(fg); p.setFont(Theme::i().font(14));
            p.drawText(QRectF(r.left() + 30, r.top(), r.width() - 36, r.height()), Qt::AlignVCenter | Qt::AlignLeft, it.text);
            if (!it.hint.isEmpty()) { p.setPen(c.muted); p.setFont(Theme::i().font(12)); p.drawText(r.adjusted(0, 0, -10, 0), Qt::AlignVCenter | Qt::AlignRight, Theme::keyText(it.hint)); }
        }
        y += h;
    }
}
void PopupMenu::mouseMoveEvent(QMouseEvent* e) { const int i = itemAt(e->pos()); if (i != m_hover) { m_hover = i; update(); } }
void PopupMenu::mousePressEvent(QMouseEvent* e) { if (!QRect(kPad, kPad, width() - 2 * kPad, height() - 2 * kPad).contains(e->pos())) close(); }
void PopupMenu::mouseReleaseEvent(QMouseEvent* e) {
    const int i = itemAt(e->pos());
    if (i >= 0 && m_items[i].enabled) { const QString id = m_items[i].id; close(); emit triggered(id); }
}
void PopupMenu::keyPressEvent(QKeyEvent* e) {
    auto step = [&](int d) { int i = m_hover; for (int n = 0; n < m_items.size(); ++n) { i = (i + d + m_items.size()) % m_items.size(); if (!m_items[i].sep && !m_items[i].header && m_items[i].enabled) { m_hover = i; update(); return; } } };
    if (e->key() == Qt::Key_Down) step(1);
    else if (e->key() == Qt::Key_Up) step(-1);
    else if ((e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) && m_hover >= 0) { const QString id = m_items[m_hover].id; close(); emit triggered(id); }
    else if (e->key() == Qt::Key_Escape) close();
}

// ================= Toast =================
Toast::Toast(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    hide();
    m_timer = new QTimer(this); m_timer->setSingleShot(true);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_op = v.toReal(); update(); if (m_op <= 0.01 && m_anim.endValue().toReal() == 0.0) hide(); });
    connect(m_timer, &QTimer::timeout, this, [this] {
        m_anim.stop(); m_anim.setDuration(Theme::i().animMs(200));
        if (m_anim.duration() == 0) { m_op = 0; hide(); return; }
        m_anim.setStartValue(m_op); m_anim.setEndValue(0.0); m_anim.start();
    });
}
void Toast::show(const QString& text, int ms, bool error) {
    m_text = text; m_error = error;
    QFontMetrics fm(Theme::i().font(14, QFont::Medium));
    const int w = qMin(parentWidget()->width() - 40, fm.horizontalAdvance(text) + 44);
    resize(w, 40);
    move((parentWidget()->width() - w) / 2, parentWidget()->height() - 150);
    raise(); QWidget::show();
    m_anim.stop(); m_anim.setDuration(Theme::i().animMs(180));
    if (m_anim.duration() == 0) { m_op = 1; update(); } else { m_anim.setStartValue(m_op); m_anim.setEndValue(1.0); m_anim.start(); }
    m_timer->start(ms);
}
void Toast::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_op);
    p.translate(0, (1.0 - m_op) * 8);                       // rises a few pixels as it fades in
    p.setPen(QPen(QColor(255, 255, 255, 36), 1)); p.setBrush(pal().overlay);
    p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 10, 10);
    if (m_error) { p.setPen(Qt::NoPen); p.setBrush(QColor("#ff6b61")); p.drawRoundedRect(QRectF(0, 0, 4, height()), 2, 2); }
    p.setPen(pal().overlayText); p.setFont(Theme::i().font(14, QFont::Medium));
    p.drawText(rect().adjusted(m_error ? 10 : 0, 0, -6, 0), Qt::AlignCenter, QFontMetrics(p.font()).elidedText(m_text, Qt::ElideRight, width() - 30));
}

// ================= FadeCover =================
FadeCover::FadeCover(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_op = v.toReal(); update(); });
    connect(&m_anim, &QVariantAnimation::finished, this, [this] {
        if (m_op <= 0.001) hide();
        if (m_done) { auto d = m_done; m_done = nullptr; d(); }
    });
}
void FadeCover::run(qreal from, qreal to, int ms, std::function<void()> done) {
    m_anim.stop();
    m_done = std::move(done);
    ms = Theme::i().animMs(ms);
    m_op = from;
    if (ms <= 0) { m_op = to; if (to <= 0.001) hide(); update(); if (m_done) { auto d = m_done; m_done = nullptr; d(); } return; }
    setGeometry(parentWidget()->rect());
    raise(); show();
    m_anim.setDuration(ms); m_anim.setStartValue(from); m_anim.setEndValue(to); m_anim.start();
}
void FadeCover::paintEvent(QPaintEvent*) {
    QPainter p(this);
    QColor c = pal().bg; c.setAlphaF(qBound(0.0, m_op, 1.0));
    p.fillRect(rect(), c);
}
