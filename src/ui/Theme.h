#pragma once
#include <QColor>
#include <QEasingCurve>
#include <QFont>
#include <QObject>
#include <QPointF>

struct Palette {
    QColor bg, surface, sunken, raised, border, borderStrong;
    QColor text, muted, accent, onAccent, accentSubtle, danger, scrim, shadow;
    QColor stage;          // video letterbox
    QColor overlay;        // floating control bar background (always dark: sits on video)
    QColor overlayText, overlayMuted, overlayAccent;
    bool dark = false;
};

// Tokens taken from the owner's other apps (ropework / kanvaz): warm paper neutrals, teal accent, 8px radius.
class Theme : public QObject {
    Q_OBJECT
public:
    enum class Mode { System, Light, Dark };
    static Theme& i();

    const Palette& p() const { return m_pal; }
    Mode mode() const { return m_mode; }
    void setMode(Mode m);
    void setCompact(bool c);
    bool compact() const { return m_compact; }
    bool reduceMotion() const { return m_reduceMotion; }
    void setReduceMotion(bool r) { m_reduceMotion = r; }
    // 0 = follow the system "animation effects" setting, 1 = animations on, 2 = animations off
    void setMotionMode(int m);
    int motionMode() const { return m_motionMode; }
    void setAccentIndex(int i);
    int accentIndex() const { return m_accent; }
    static int accentCount();
    static QColor accentSample(int i, bool dark);
    void setGlass(bool g) { m_glass = g; emit changed(); }
    bool glass() const { return m_glass; }

    void loadFonts();
    QFont font(qreal px, int weight = QFont::Normal, bool heading = false) const;
    qreal radius() const { return 8; }
    int rowH() const { return m_compact ? 28 : 34; }
    int animMs(int ms) const { return m_reduceMotion ? 0 : ms; }
    // Motion curves. easeMac is Apple's standard "fast in, long soft landing" curve (cubic-bezier .32,.72,0,1);
    // easeSpring overshoots slightly and settles, for toggles and small controls.
    static QEasingCurve easeMac() { QEasingCurve c(QEasingCurve::BezierSpline); c.addCubicBezierSegment(QPointF(0.32, 0.72), QPointF(0.0, 1.0), QPointF(1.0, 1.0)); return c; }
    static QEasingCurve easeSpring() { QEasingCurve c(QEasingCurve::BezierSpline); c.addCubicBezierSegment(QPointF(0.34, 1.45), QPointF(0.64, 1.0), QPointF(1.0, 1.0)); return c; }

signals:
    void changed();

private:
    Theme();
    void apply();
    Mode m_mode = Mode::System;
    Palette m_pal;
    bool m_compact = false;
    bool m_reduceMotion = false;
    bool m_glass = true;
    int m_motionMode = 0;
    int m_accent = 0;
    QString m_body = QStringLiteral("Segoe UI");
    QString m_head = QStringLiteral("Segoe UI");
};

inline const Palette& pal() { return Theme::i().p(); }
