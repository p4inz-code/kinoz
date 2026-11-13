// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/TopBar.h"
#include "ui/HoverTip.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>

TopBar::TopBar(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setFixedHeight(44);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_op = v.toReal(); update(); emit opacityChanged(); });
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

QVector<TopBar::B> TopBar::buttons() const {
    QVector<B> v;
    const qreal w = 46, h = height();
    qreal x = width();
#ifdef Q_OS_MACOS
    // macOS convention: close, minimise and "zoom" are three round buttons at the top left; the rest sit on the right.
    v << B{"close", QRectF(14, (h - 14) / 2, 14, 14)} << B{"min", QRectF(34, (h - 14) / 2, 14, 14)} << B{"max", QRectF(54, (h - 14) / 2, 14, 14)};
    x -= 44; v << B{"pin", QRectF(x, 4, 36, h - 8)};
    x -= 40; v << B{"more", QRectF(x, 4, 36, h - 8)};
    x -= 40; v << B{"settings", QRectF(x, 4, 36, h - 8)};
    v << B{"sidebar", QRectF(84, 4, 36, h - 8)};
    Q_UNUSED(w)
    return v;
#else
    for (const char* id : {"close", "max", "min"}) { x -= w; v << B{QString::fromLatin1(id), QRectF(x, 0, w, h)}; }
    x -= 40; v << B{"pin", QRectF(x, 4, 36, h - 8)};
    x -= 40; v << B{"more", QRectF(x, 4, 36, h - 8)};          // always visible, even before anything is playing
    x -= 40; v << B{"settings", QRectF(x, 4, 36, h - 8)};
    v << B{"sidebar", QRectF(8, 4, 36, h - 8)};
    return v;
#endif
}

QRect TopBar::globalButtonRect(const QString& id) const {
    for (const auto& b : buttons()) if (b.id == id) return QRect(mapToGlobal(b.r.topLeft().toPoint()), b.r.size().toSize());
    return QRect(mapToGlobal(QPoint(width() - 40, height())), QSize(1, 1));
}

QString TopBar::tipFor(const QString& id) const {
    if (id == "close") return tr("Close");
#ifdef Q_OS_MACOS
    if (id == "max") return tr("Full screen\tF");
#else
    if (id == "max") return m_max ? tr("Restore") : tr("Maximize");
#endif
    if (id == "min") return tr("Minimize");
    if (id == "pin") return m_pinned ? tr("Always on top: on\tCtrl+Shift+T") : tr("Always on top\tCtrl+Shift+T");
    if (id == "more") return tr("More");
    if (id == "settings") return tr("Settings\tCtrl+,");
    if (id == "sidebar") return tr("Library\tTab");
    return {};
}

bool TopBar::isCaption(const QPoint& l) const {
    if (!isVisible() || m_op < 0.05) return false;
    for (const auto& b : buttons()) if (b.r.contains(l)) return false;
    return true;
}

void TopBar::reveal(bool on) {
    const qreal t = on ? 1.0 : 0.0;
    if (t == m_target) return;
    m_target = t;
    m_anim.stop();
    m_anim.setDuration(Theme::i().animMs(on ? 140 : 260));
    m_anim.setStartValue(m_op); m_anim.setEndValue(t);
    if (m_anim.duration() == 0) { m_op = t; update(); emit opacityChanged(); } else m_anim.start();
}

void TopBar::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.setOpacity(m_op);
    if (m_over) {
        if (!m_glass) {
            QLinearGradient g(0, 0, 0, height());
            g.setColorAt(0, QColor(0, 0, 0, 150)); g.setColorAt(1, QColor(0, 0, 0, 0));
            p.fillRect(rect(), g);
        } else {   // glass strip drawn by GL underneath: add a metal hairline at the bottom
            QLinearGradient hl(0, 0, width(), 0);
            hl.setColorAt(0, QColor(255, 255, 255, 10)); hl.setColorAt(0.5, QColor(255, 255, 255, 70)); hl.setColorAt(1, QColor(255, 255, 255, 10));
            p.setPen(QPen(QBrush(hl), 1)); p.drawLine(0, height() - 1, width(), height() - 1);
        }
    } else {
        p.fillRect(rect(), c.bg);
        p.setPen(c.border); p.drawLine(0, height() - 1, width(), height() - 1);
    }
    const QColor fg = m_over ? c.overlayText : c.text;
    const QColor mu = m_over ? c.overlayMuted : c.muted;
    const auto bs = buttons();
    for (int i = 0; i < bs.size(); ++i) {
        const auto& b = bs[i];
        const bool hov = i == m_hover;
#ifdef Q_OS_MACOS
        if (b.id == "close" || b.id == "min" || b.id == "max") {                      // traffic lights: grey when the window is not active
            const bool active = window() && window()->isActiveWindow();
            const QColor col = !active ? QColor(120, 120, 120, 150) : b.id == "close" ? QColor("#ff5f57") : b.id == "min" ? QColor("#febc2e") : QColor("#28c840");
            p.setPen(QPen(QColor(0, 0, 0, 40), 0.8)); p.setBrush(col); p.drawEllipse(b.r);
            const bool group = m_hover >= 0 && m_hover < 3;                              // symbols appear while the pointer is over the group
            if (group && active) {
                p.setPen(QPen(QColor(0, 0, 0, 150), 1.3, Qt::SolidLine, Qt::RoundCap));
                const QPointF c0 = b.r.center();
                if (b.id == "close") { p.drawLine(c0 + QPointF(-2.6, -2.6), c0 + QPointF(2.6, 2.6)); p.drawLine(c0 + QPointF(-2.6, 2.6), c0 + QPointF(2.6, -2.6)); }
                else if (b.id == "min") p.drawLine(c0 + QPointF(-3, 0), c0 + QPointF(3, 0));
                else { p.drawLine(c0 + QPointF(-3, 0), c0 + QPointF(3, 0)); p.drawLine(c0 + QPointF(0, -3), c0 + QPointF(0, 3)); }
            }
            continue;
        }
#endif
        if (b.id == "close" && hov) { p.fillRect(b.r, QColor("#c42b1c")); drawIcon(&p, Icon::Close, QRectF(b.r.center().x() - 8, b.r.center().y() - 8, 16, 16), Qt::white); continue; }
        if (hov) { p.setPen(Qt::NoPen); p.setBrush(m_over ? QColor(255, 255, 255, 30) : QColor(c.text.red(), c.text.green(), c.text.blue(), 22)); if (b.id == "min" || b.id == "max") p.drawRect(b.r); else p.drawRoundedRect(b.r, 8, 8); }
        Icon ic = b.id == "close" ? Icon::Close : b.id == "max" ? (m_max ? Icon::Restore : Icon::Maximize) : b.id == "min" ? Icon::Minimize
                : b.id == "pin" ? Icon::Pin : b.id == "more" ? Icon::More : b.id == "settings" ? Icon::Settings : Icon::Sidebar;
        const qreal s = (b.id == "sidebar" || b.id == "pin" || b.id == "more" || b.id == "settings") ? 20 : 16;
        drawIcon(&p, ic, QRectF(b.r.center().x() - s / 2, b.r.center().y() - s / 2, s, s), (b.id == "pin" && m_pinned) ? (m_over ? c.overlayAccent : c.accent) : fg, 1.6);
    }
    // title block
#ifdef Q_OS_MACOS
    const qreal left = 130, right = width() - 44 - 40 * 2 - 12;
#else
    const qreal left = 54, right = width() - 46 * 3 - 52 - 80;
#endif
    const qreal rightEff = right - (m_priv ? 84 : 0);
    p.setPen(fg);
    p.setFont(Theme::i().font(14, QFont::DemiBold, true));
    QFontMetrics fm(p.font());
    const QString t = fm.elidedText(m_title.isEmpty() ? QStringLiteral(APP_NAME) : m_title, Qt::ElideRight, int(rightEff - left));
    p.drawText(QRectF(left, 0, rightEff - left, height()), Qt::AlignVCenter | Qt::AlignLeft, t);
    if (!m_sub.isEmpty() && !m_title.isEmpty()) {
        p.setPen(mu); p.setFont(Theme::i().font(12));
        p.drawText(QRectF(left + fm.horizontalAdvance(t) + 10, 1, rightEff - left - fm.horizontalAdvance(t) - 10, height()), Qt::AlignVCenter | Qt::AlignLeft, m_sub);
    }
    if (m_priv) {                                                       // private mode is on: nothing you play is remembered
        const QRectF pill(right - 76, (height() - 22) / 2.0, 70, 22);
        p.setPen(Qt::NoPen); p.setBrush(m_over ? QColor(255, 255, 255, 36) : c.accentSubtle); p.drawRoundedRect(pill, 11, 11);
        p.setPen(m_over ? c.overlayAccent : c.accent); p.setFont(Theme::i().font(12, QFont::DemiBold));
        p.drawText(pill, Qt::AlignCenter, tr("Private"));
    }
}

void TopBar::mouseMoveEvent(QMouseEvent* e) {
    int h = -1; const auto bs = buttons();
    for (int i = 0; i < bs.size(); ++i) if (bs[i].r.contains(e->position())) h = i;
    if (h != m_hover) {
        m_hover = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update();
        HoverTip::cancel();
        if (h >= 0) HoverTip::request(globalButtonRect(bs[h].id), tipFor(bs[h].id));
    }
}
void TopBar::leaveEvent(QEvent*) { m_hover = -1; HoverTip::cancel(); update(); }
void TopBar::mousePressEvent(QMouseEvent* e) {
    HoverTip::cancel();
    if (e->button() != Qt::LeftButton) return;
    for (const auto& b : buttons()) if (b.r.contains(e->position())) { emit action(b.id); return; }
}
void TopBar::mouseDoubleClickEvent(QMouseEvent*) {}
