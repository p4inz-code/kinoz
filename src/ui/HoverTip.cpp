// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/HoverTip.h"
#include "ui/Theme.h"
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QScreen>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

namespace {
class Bubble : public QWidget {
public:
    Bubble() : QWidget(nullptr, Qt::ToolTip | Qt::FramelessWindowHint | Qt::WindowDoesNotAcceptFocus | Qt::NoDropShadowWindowHint) {
        setAttribute(Qt::WA_ShowWithoutActivating);
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_TranslucentBackground);
        m_delay.setSingleShot(true);
        m_life.setSingleShot(true); m_life.setInterval(4500);                                       // a label never lingers
        QObject::connect(&m_life, &QTimer::timeout, [this] { cancel(); });
        QObject::connect(qApp, &QGuiApplication::applicationStateChanged, [this](Qt::ApplicationState st) { if (st != Qt::ApplicationActive) cancel(); });
        QObject::connect(&m_delay, &QTimer::timeout, [this] { present(); });
        m_fade.setStartValue(0.0); m_fade.setEndValue(1.0); m_fade.setEasingCurve(Theme::easeMac());
        QObject::connect(&m_fade, &QVariantAnimation::valueChanged, [this](const QVariant& v) { setWindowOpacity(qBound(0.0, v.toReal(), 1.0)); });
    }
    void request(const QRect& anchor, const QString& text) {
        static const bool quiet = qEnvironmentVariableIsSet("KINOZ_SILENT") || qEnvironmentVariableIsSet("KINOZ_OFFSCREEN");   // tests never touch the real screen
        if (quiet) return;
        m_anchor = anchor; m_text = text;
        const bool warm = isVisible() || (m_sinceHide.isValid() && m_sinceHide.elapsed() < 600);   // sliding between icons: no wait
        if (isVisible()) hide();
        m_delay.start(warm ? 40 : 380);
    }
    void cancel() {
        m_delay.stop(); m_life.stop();
        if (isVisible()) { hide(); m_sinceHide.start(); }
    }
protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const auto& c = pal();
        const QRectF r = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        p.setPen(QPen(QColor(255, 255, 255, 40), 1)); p.setBrush(c.overlay);
        p.drawRoundedRect(r, 8, 8);
        const QString label = m_text.section('\t', 0, 0), key = Theme::keyText(m_text.section('\t', 1));
        p.setFont(Theme::i().font(13, QFont::Medium)); p.setPen(c.overlayText);
        const QFontMetrics fm(p.font());
        qreal x = 11;
        p.drawText(QRectF(x, 0, fm.horizontalAdvance(label), height()), Qt::AlignVCenter, label);
        if (!key.isEmpty()) {
            x += fm.horizontalAdvance(label) + 10;
            p.setFont(Theme::i().font(11, QFont::DemiBold));
            const QFontMetrics kf(p.font());
            const QRectF chip(x, (height() - 20) / 2.0, kf.horizontalAdvance(key) + 12, 20);
            p.setPen(Qt::NoPen); p.setBrush(QColor(255, 255, 255, 30)); p.drawRoundedRect(chip, 5, 5);
            p.setPen(c.overlayMuted); p.drawText(chip, Qt::AlignCenter, key);
        }
    }
private:
    void present() {
        if (m_text.isEmpty()) return;
        const QString label = m_text.section('\t', 0, 0), key = Theme::keyText(m_text.section('\t', 1));
        const QFontMetrics fm(Theme::i().font(13, QFont::Medium)), kf(Theme::i().font(11, QFont::DemiBold));
        int w = 22 + fm.horizontalAdvance(label) + (key.isEmpty() ? 0 : 10 + kf.horizontalAdvance(key) + 12), h = 30;
        QScreen* sc = QGuiApplication::screenAt(m_anchor.center());
        const QRect avail = sc ? sc->availableGeometry() : QRect(0, 0, 1920, 1080);
        int x = m_anchor.center().x() - w / 2, y = m_anchor.bottom() + 8;
        if (y + h > avail.bottom()) y = m_anchor.top() - h - 8;                 // no room below: go above
        x = qBound(avail.left() + 4, x, avail.right() - w - 4);
        setGeometry(x, y, w, h);
        setWindowOpacity(Theme::i().animMs(1) > 0 ? 0.0 : 1.0);
        show();
        m_life.start();
        m_fade.stop(); m_fade.setDuration(Theme::i().animMs(140));
        if (m_fade.duration() > 0) m_fade.start();
    }
    QTimer m_delay, m_life;
    QVariantAnimation m_fade;
    QElapsedTimer m_sinceHide;
    QRect m_anchor;
    QString m_text;
};

Bubble& bubble() { static Bubble* b = new Bubble; return *b; }       // lives as long as the app; never deleted on purpose
}

namespace HoverTip {
void request(const QRect& anchorGlobal, const QString& text) { bubble().request(anchorGlobal, text); }
void cancel() { bubble().cancel(); }
}
