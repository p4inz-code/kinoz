// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QVariantAnimation>
#include <QWidget>

// Custom title bar (no OS chrome): sidebar toggle, title, window buttons. Drag area is resolved by the window's hit-test.
class TopBar : public QWidget {
    Q_OBJECT
public:
    explicit TopBar(QWidget* parent = nullptr);
    void setTitle(const QString& t) { m_title = t; update(); }
    void setSubtitle(const QString& t) { m_sub = t; update(); }
    void setOverVideo(bool v) { m_over = v; update(); }
    void setMaximized(bool m) { m_max = m; update(); }
    void setPinned(bool p) { m_pinned = p; update(); }
    void reveal(bool on);
    bool isRevealed() const { return m_target > 0.5; }
    void setGlass(bool g) { m_glass = g; update(); }
    qreal opacity() const { return m_op; }
    bool overVideo() const { return m_over; }
    // true if local point is a draggable caption area (not a button)
    bool isCaption(const QPoint& local) const;
    bool busy() const { return m_hover >= 0; }
signals:
    void opacityChanged();
    void action(const QString& id);   // sidebar | min | max | close | pin
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void leaveEvent(QEvent*) override { m_hover = -1; update(); }
    void mouseDoubleClickEvent(QMouseEvent*) override;
private:
    struct B { QString id; QRectF r; };
    QVector<B> buttons() const;
    QString m_title, m_sub;
    bool m_over = false, m_max = false, m_pinned = false, m_glass = false;
    int m_hover = -1;
    qreal m_op = 1, m_target = 1;
    QVariantAnimation m_anim;
};
