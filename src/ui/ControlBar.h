#pragma once
#include "ui/Icons.h"
#include "media/MediaKind.h"
#include <QVariantAnimation>
#include <QWidget>

// Floating, fully custom-painted transport bar. One widget, no children: fades by painting with opacity.
class ControlBar : public QWidget {
    Q_OBJECT
public:
    explicit ControlBar(QWidget* parent = nullptr);

    struct State {
        MediaKind kind = MediaKind::Unknown;
        double pos = 0, dur = 0, volume = 100, speed = 1;
        bool playing = false, muted = false, shuffle = false, buffering = false;
        int repeat = 0;
        QString zoomText;
    };
    void setState(const State& s);
    void setPosition(double pos, double dur);
    void reveal(bool on);
    bool isRevealed() const { return m_target > 0.5; }
    void setGlass(bool g) { m_glass = g; update(); }
    qreal opacity() const { return m_op; }
    QRectF panelRect() const { return QRectF(rect()).adjusted(0, 6, 0, -2); }
    void setCompact(bool c) { m_compact = c; relayout(); update(); }
    bool busy() const { return m_scrub || m_volDrag || m_hover >= 0; }
    int preferredHeight() const;
    QRect buttonRect(const QString& id) const;   // in global coords (for anchoring menus)

signals:
    void opacityChanged();
    void action(const QString& id);
    void scrubStarted();
    void scrubbed(double frac);          // while dragging
    void scrubFinished(double frac);
    void hoverSeek(double frac, const QPoint& globalPos);
    void hoverLeft();
    void volumeDragged(double vol);      // 0..130

protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void resizeEvent(QResizeEvent*) override { relayout(); }

private:
    struct Btn { QString id; Icon icon; QRectF r; bool active = false; bool big = false; QString label; };
    void relayout();
    int hitBtn(const QPointF& p) const;
    static QString fmt(double s);

    State m_s;
    QVector<Btn> m_btns;
    QRectF m_seek, m_vol;
    int m_hover = -1;
    bool m_scrub = false, m_volDrag = false, m_seekHot = false;
    double m_scrubFrac = 0, m_hoverFrac = -1;
    qreal m_op = 0, m_target = 0;
    bool m_glass = false, m_compact = false;
    QVariantAnimation m_anim;
};
