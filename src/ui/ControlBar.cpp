#include "ui/ControlBar.h"
#include "ui/Theme.h"
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QTime>
#include <QWheelEvent>
#include <cmath>

ControlBar::ControlBar(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    setAttribute(Qt::WA_TranslucentBackground);
    m_anim.setEasingCurve(QEasingCurve::OutCubic);
    connect(&m_anim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) {
        m_op = v.toReal(); update(); emit opacityChanged();
        if (m_op <= 0.001 && m_target == 0.0) hide();
    });
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

int ControlBar::preferredHeight() const { return 96; }

void ControlBar::reveal(bool on) {
    const qreal t = on ? 1.0 : 0.0;
    if (t == m_target && (on == isVisible() || !on)) return;
    m_target = t;
    if (on && !isVisible()) show();
    m_anim.stop();
    m_anim.setDuration(Theme::i().animMs(on ? 140 : 260));
    m_anim.setStartValue(m_op);
    m_anim.setEndValue(t);
    if (m_anim.duration() == 0) { m_op = t; update(); emit opacityChanged(); if (!on) hide(); } else m_anim.start();
}

QString ControlBar::fmt(double s) {
    if (s < 0 || !std::isfinite(s)) s = 0;
    const int t = int(s);
    const int h = t / 3600, m = (t / 60) % 60, sec = t % 60;
    return h ? QStringLiteral("%1:%2:%3").arg(h).arg(m, 2, 10, QLatin1Char('0')).arg(sec, 2, 10, QLatin1Char('0'))
             : QStringLiteral("%1:%2").arg(m).arg(sec, 2, 10, QLatin1Char('0'));
}

void ControlBar::setState(const State& s) { m_s = s; relayout(); update(); }
void ControlBar::setPosition(double pos, double dur) { m_s.pos = pos; m_s.dur = dur; if (!m_scrub && isVisible()) update(); }

void ControlBar::relayout() {
    m_btns.clear();
    const bool timeline = m_s.kind == MediaKind::Video || m_s.kind == MediaKind::Audio || m_s.kind == MediaKind::Gif;
    const bool picture = Media::isPicture(m_s.kind) && m_s.kind != MediaKind::Gif;
    const qreal W = width();
    const qreal cy = timeline ? 66 : 42;
    m_seek = timeline ? QRectF(70, 18, W - 140, 20) : QRectF();
    const qreal bs = 36;
    // centre transport
    QVector<Btn> centre;
    if (!picture && !m_compact) centre << Btn{"shuffle", Icon::Shuffle, {}, m_s.shuffle};
    centre << Btn{"prev", Icon::Prev, {}};
    if (!picture) centre << Btn{"play", m_s.playing ? Icon::Pause : Icon::Play, {}, false, true};
    centre << Btn{"next", Icon::Next, {}};
    if (!picture && !m_compact) centre << Btn{"repeat", m_s.repeat == 2 ? Icon::RepeatOne : Icon::Repeat, {}, m_s.repeat != 0};
    qreal total = 0;
    for (auto& b : centre) total += (b.big ? 48 : bs) + 4;
    qreal x = (W - total) / 2;
    for (auto& b : centre) { const qreal s = b.big ? 48 : bs; b.r = QRectF(x, cy - s / 2, s, s); x += s + 4; m_btns << b; }
    // left: volume
    if (!picture && !m_compact) {
        const Icon vi = (m_s.muted || m_s.volume < 1) ? Icon::Mute : m_s.volume < 50 ? Icon::Volume1 : Icon::Volume2;
        m_btns << Btn{"mute", vi, QRectF(14, cy - bs / 2, bs, bs)};
        m_vol = QRectF(14 + bs + 2, cy - 10, 84, 20);
    } else m_vol = QRectF();
    // right group
    qreal rx = W - 14;
    auto addR = [&](const QString& id, Icon ic, bool act = false) { rx -= bs; m_btns << Btn{id, ic, QRectF(rx, cy - bs / 2, bs, bs), act}; rx -= 2; };
    if (m_compact) addR("mini", Icon::Mini);
    else {
        addR("fullscreen", Icon::Fullscreen);
        addR("mini", Icon::Mini);
        addR("more", Icon::More);
        if (picture) addR("rotate", Icon::Redo);
        if (m_s.kind == MediaKind::Video || m_s.kind == MediaKind::Gif) addR("adjust", Icon::Adjust);
        if (m_s.kind == MediaKind::Audio) addR("eq", Icon::Equalizer);
        if (m_s.kind == MediaKind::Video) addR("tracks", Icon::Subtitles);
    }
    if (timeline && !m_compact) { rx -= 52; Btn b{"speed", Icon::Speed, QRectF(rx, cy - 14, 52, 28)}; b.label = m_s.speed == 1.0 ? QStringLiteral("1×") : QStringLiteral("%1×").arg(m_s.speed, 0, 'g', 3); b.active = m_s.speed != 1.0; m_btns << b; }
    if (picture && !m_s.zoomText.isEmpty()) { rx -= 60; Btn b{"zoom", Icon::Search, QRectF(rx, cy - 14, 60, 28)}; b.label = m_s.zoomText; m_btns << b; }
}

int ControlBar::hitBtn(const QPointF& p) const {
    for (int i = 0; i < m_btns.size(); ++i) if (m_btns[i].r.contains(p)) return i;
    return -1;
}

QRect ControlBar::buttonRect(const QString& id) const {
    for (const auto& b : m_btns) if (b.id == id) return QRect(mapToGlobal(b.r.topLeft().toPoint()), b.r.size().toSize());
    return QRect(mapToGlobal(QPoint(width() / 2, 0)), QSize(1, 1));
}

void ControlBar::paintEvent(QPaintEvent*) {
    if (m_op <= 0.001) return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setOpacity(m_op);
    const auto& c = pal();
    // floating panel: real glass is drawn by the GL layer underneath; without GL we fall back to a solid tint
    const QRectF panel = panelRect();
    if (!m_glass) {
        for (int i = 5; i >= 1; --i) { p.setPen(Qt::NoPen); p.setBrush(QColor(0, 0, 0, 14 * (6 - i))); p.drawRoundedRect(panel.adjusted(-i, -i + 3, i, i + 3), 16 + i, 16 + i); }
        p.setBrush(c.overlay); p.setPen(Qt::NoPen);
        p.drawRoundedRect(panel, 16, 16);
    }
    {   // polished-metal rim: bright at the top edge, fading down
        QLinearGradient rim(panel.topLeft(), panel.bottomLeft());
        rim.setColorAt(0, QColor(255, 255, 255, 120)); rim.setColorAt(0.5, QColor(255, 255, 255, 34)); rim.setColorAt(1, QColor(255, 255, 255, 60));
        p.setPen(QPen(QBrush(rim), 1.2)); p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(panel.adjusted(0.6, 0.6, -0.6, -0.6), 16, 16);
    }

    // timeline
    if (!m_seek.isNull()) {
        const double frac = m_scrub ? m_scrubFrac : (m_s.dur > 0 ? qBound(0.0, m_s.pos / m_s.dur, 1.0) : 0.0);
        p.setFont(Theme::i().font(12, QFont::Medium));
        p.setPen(c.overlayMuted);
        p.drawText(QRectF(14, m_seek.top(), 52, m_seek.height()), Qt::AlignVCenter | Qt::AlignRight, fmt(m_scrub ? m_scrubFrac * m_s.dur : m_s.pos));
        p.drawText(QRectF(m_seek.right() + 6, m_seek.top(), 60, m_seek.height()), Qt::AlignVCenter | Qt::AlignLeft, fmt(m_s.dur));
        const qreal th = (m_seekHot || m_scrub) ? 6 : 4;
        const QRectF tr(m_seek.left(), m_seek.center().y() - th / 2, m_seek.width(), th);
        p.setPen(Qt::NoPen); p.setBrush(QColor(255, 255, 255, 45)); p.drawRoundedRect(tr, th / 2, th / 2);
        if (m_hoverFrac >= 0 && !m_scrub) { p.setBrush(QColor(255, 255, 255, 40)); p.drawRoundedRect(QRectF(tr.left(), tr.top(), tr.width() * m_hoverFrac, th), th / 2, th / 2); }
        {   // liquid-metal fill: teal core with a travelling silver highlight
            const QRectF fr(tr.left(), tr.top(), qMax(th, tr.width() * frac), th);
            QLinearGradient lg(fr.topLeft(), fr.topRight());
            const qreal ph = std::fmod(QTime::currentTime().msecsSinceStartOfDay() / 3500.0, 1.0);
            lg.setColorAt(0, c.overlayAccent); lg.setColorAt(qBound(0.0, ph - 0.12, 1.0), c.overlayAccent);
            lg.setColorAt(qBound(0.0, ph, 1.0), QColor("#e9fbfc")); lg.setColorAt(qBound(0.0, ph + 0.12, 1.0), c.overlayAccent); lg.setColorAt(1, c.overlayAccent);
            p.setBrush(lg); p.drawRoundedRect(fr, th / 2, th / 2);
        }
        if (m_seekHot || m_scrub) {
            const QPointF kc(tr.left() + tr.width() * frac, tr.center().y());
            QRadialGradient rg(kc - QPointF(2, 2.5), 9); rg.setColorAt(0, QColor("#ffffff")); rg.setColorAt(0.6, QColor("#c9d1da")); rg.setColorAt(1, QColor("#8c97a4"));
            p.setBrush(rg); p.setPen(QPen(QColor(255, 255, 255, 120), 0.8)); p.drawEllipse(kc, 7.5, 7.5); p.setPen(Qt::NoPen);
        }
    }
    // volume slider
    if (!m_vol.isNull()) {
        const double vf = m_s.muted ? 0.0 : qBound(0.0, m_s.volume / 130.0, 1.0);
        const QRectF tr(m_vol.left(), m_vol.center().y() - 2, m_vol.width(), 4);
        p.setPen(Qt::NoPen); p.setBrush(QColor(255, 255, 255, 45)); p.drawRoundedRect(tr, 2, 2);
        p.setBrush(c.overlayText); p.drawRoundedRect(QRectF(tr.left(), tr.top(), tr.width() * vf, 4), 2, 2);
        p.drawEllipse(QPointF(tr.left() + tr.width() * vf, tr.center().y()), 5.5, 5.5);
    }
    // buttons
    for (int i = 0; i < m_btns.size(); ++i) {
        const Btn& b = m_btns[i];
        const bool hov = i == m_hover;
        if (!b.label.isEmpty()) {
            if (hov || b.active) { p.setPen(Qt::NoPen); p.setBrush(QColor(255, 255, 255, hov ? 30 : 18)); p.drawRoundedRect(b.r, 8, 8); }
            p.setFont(Theme::i().font(13, QFont::DemiBold)); p.setPen(b.active ? c.overlayAccent : c.overlayText);
            p.drawText(b.r, Qt::AlignCenter, b.label);
            continue;
        }
        if (b.big) {   // polished chrome play button
            QLinearGradient mg(b.r.topLeft(), b.r.bottomRight());
            mg.setColorAt(0, hov ? QColor("#ffffff") : QColor("#f2f5f9")); mg.setColorAt(0.42, QColor("#a9b2be")); mg.setColorAt(0.58, QColor("#e4e9ef")); mg.setColorAt(1, QColor("#7d8793"));
            p.setPen(QPen(QColor(255, 255, 255, 150), 1)); p.setBrush(mg); p.drawEllipse(b.r.adjusted(1, 1, -1, -1));
            QRadialGradient hl(b.r.center() - QPointF(0, b.r.height() * 0.28), b.r.width() * 0.5);
            hl.setColorAt(0, QColor(255, 255, 255, 150)); hl.setColorAt(1, QColor(255, 255, 255, 0));
            p.setPen(Qt::NoPen); p.setBrush(hl); p.drawEllipse(b.r.adjusted(4, 3, -4, -b.r.height() * 0.45));
            drawIcon(&p, b.icon, QRectF(b.r.center().x() - 13, b.r.center().y() - 13, 26, 26), QColor("#1a1c21"), 1.5);
            continue;
        }
        if (hov) { p.setPen(Qt::NoPen); p.setBrush(QColor(255, 255, 255, 30)); p.drawRoundedRect(b.r, 8, 8); }
        const qreal is = 22;
        QColor fg = b.active ? c.overlayAccent : c.overlayText;
        if (b.id == "fullscreen" && window()->isFullScreen()) { drawIcon(&p, Icon::ExitFullscreen, QRectF(b.r.center().x() - is / 2, b.r.center().y() - is / 2, is, is), fg); continue; }
        drawIcon(&p, b.icon, QRectF(b.r.center().x() - is / 2, b.r.center().y() - is / 2, is, is), fg);
    }
}

void ControlBar::mousePressEvent(QMouseEvent* e) {
    if (e->button() != Qt::LeftButton) return;
    const QPointF pt = e->position();
    if (!m_seek.isNull() && m_seek.adjusted(-6, -8, 6, 8).contains(pt)) {
        m_scrub = true; m_scrubFrac = qBound(0.0, (pt.x() - m_seek.left()) / m_seek.width(), 1.0);
        emit scrubStarted(); emit scrubbed(m_scrubFrac); update(); return;
    }
    if (!m_vol.isNull() && m_vol.adjusted(-6, -8, 6, 8).contains(pt)) {
        m_volDrag = true;
        emit volumeDragged(qBound(0.0, (pt.x() - m_vol.left()) / m_vol.width(), 1.0) * 130.0); return;
    }
    const int i = hitBtn(pt);
    if (i >= 0) emit action(m_btns[i].id);
}

void ControlBar::mouseMoveEvent(QMouseEvent* e) {
    const QPointF pt = e->position();
    if (m_scrub) { m_scrubFrac = qBound(0.0, (pt.x() - m_seek.left()) / m_seek.width(), 1.0); emit scrubbed(m_scrubFrac); emit hoverSeek(m_scrubFrac, e->globalPosition().toPoint()); update(); return; }
    if (m_volDrag) { emit volumeDragged(qBound(0.0, (pt.x() - m_vol.left()) / m_vol.width(), 1.0) * 130.0); return; }
    const int h = hitBtn(pt);
    const bool hot = !m_seek.isNull() && m_seek.adjusted(-6, -8, 6, 8).contains(pt);
    if (hot) { m_hoverFrac = qBound(0.0, (pt.x() - m_seek.left()) / m_seek.width(), 1.0); emit hoverSeek(m_hoverFrac, e->globalPosition().toPoint()); }
    else if (m_seekHot) { m_hoverFrac = -1; emit hoverLeft(); }
    if (h != m_hover || hot != m_seekHot) { m_hover = h; m_seekHot = hot; setCursor((h >= 0 || hot || (!m_vol.isNull() && m_vol.adjusted(-6, -8, 6, 8).contains(pt))) ? Qt::PointingHandCursor : Qt::ArrowCursor); }
    update();
}

void ControlBar::mouseReleaseEvent(QMouseEvent*) {
    if (m_scrub) { m_scrub = false; emit scrubFinished(m_scrubFrac); update(); }
    m_volDrag = false;
}

void ControlBar::leaveEvent(QEvent*) {
    m_hover = -1; m_seekHot = false; m_hoverFrac = -1;
    emit hoverLeft();
    update();
}

void ControlBar::wheelEvent(QWheelEvent* e) {
    emit volumeDragged(qBound(0.0, m_s.volume + (e->angleDelta().y() > 0 ? 5 : -5), 130.0));
    e->accept();
}
