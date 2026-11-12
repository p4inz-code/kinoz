// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include "ui/Icons.h"
#include <QImage>
#include <QPointF>
#include <QVector>
#include <QWidget>

class MpvPlayer;
class PlayerController;
class QLineEdit;

// Draw / highlight / type on a paused frame (or image), then export it at the media's native resolution.
class Annotator : public QWidget {
    Q_OBJECT
public:
    enum Tool { Pen, Highlighter, Arrow, Rect, Ellipse, Text };
    Annotator(PlayerController* ctl, QWidget* parent);
    void begin();
    void end();
    bool active() const { return isVisible(); }
signals:
    void message(const QString&, bool error);
    void exported(const QString& path);
    void finished();
protected:
    void paintEvent(QPaintEvent*) override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void resizeEvent(QResizeEvent*) override { relayout(); }
private:
    struct Stroke { Tool tool; QColor color; qreal width; QVector<QPointF> pts; QString text; };
    struct Item { QString id; QRectF r; Icon icon; QColor swatch; bool sep = false; };
    void relayout();
    QRectF videoRect() const;
    QPointF toNorm(const QPointF& p) const;
    QPointF fromNorm(const QPointF& n, const QRectF& vr) const;
    void drawStroke(QPainter& p, const Stroke& s, const QRectF& area, qreal widthBase) const;
    void clickItem(const QString& id);
    bool doExport(bool clipboard);
    void commitText();
    PlayerController* m_ctl;
    MpvPlayer* m_mpv;
    QVector<Stroke> m_strokes, m_redo;
    Stroke m_cur;
    bool m_drawing = false;
    Tool m_tool = Pen;
    QColor m_color{"#ff4d4f"};
    int m_widthIdx = 1;
    QVector<Item> m_items;
    QRectF m_bar;
    int m_hover = -1;
    QLineEdit* m_edit = nullptr;
    QPointF m_textAt;
};
