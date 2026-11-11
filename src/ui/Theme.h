#pragma once
#include <QColor>
#include <QFont>
#include <QObject>

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

    void loadFonts();
    QFont font(qreal px, int weight = QFont::Normal, bool heading = false) const;
    qreal radius() const { return 8; }
    int rowH() const { return m_compact ? 28 : 34; }
    int animMs(int ms) const { return m_reduceMotion ? 0 : ms; }

signals:
    void changed();

private:
    Theme();
    void apply();
    Mode m_mode = Mode::System;
    Palette m_pal;
    bool m_compact = false;
    bool m_reduceMotion = false;
    QString m_body = QStringLiteral("Segoe UI");
    QString m_head = QStringLiteral("Segoe UI");
};

inline const Palette& pal() { return Theme::i().p(); }
