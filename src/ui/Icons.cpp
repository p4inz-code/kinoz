// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/Icons.h"
#include <QPainterPath>
#include <QPolygonF>

namespace {
void line(QPainterPath& p, qreal x1, qreal y1, qreal x2, qreal y2) { p.moveTo(x1, y1); p.lineTo(x2, y2); }
void poly(QPainterPath& p, std::initializer_list<QPointF> pts) {
    bool first = true;
    for (const auto& pt : pts) { if (first) p.moveTo(pt); else p.lineTo(pt); first = false; }
}
QPainterPath rrect(qreal x, qreal y, qreal w, qreal h, qreal r) { QPainterPath p; p.addRoundedRect(x, y, w, h, r, r); return p; }
}

void drawIcon(QPainter* p, Icon ic, const QRectF& r, const QColor& c, qreal sw) {
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->translate(r.topLeft());
    p->scale(r.width() / 24.0, r.height() / 24.0);
    QPen pen(c, sw, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    p->setPen(pen);
    p->setBrush(Qt::NoBrush);
    QPainterPath s;       // stroked
    QPainterPath f;       // filled
    switch (ic) {
    case Icon::Play: poly(f, {{8, 5.5}, {19, 12}, {8, 18.5}}); f.closeSubpath(); break;
    case Icon::Pause: f.addRoundedRect(6.5, 5, 3.8, 14, 1.2, 1.2); f.addRoundedRect(13.7, 5, 3.8, 14, 1.2, 1.2); break;
    case Icon::Stop: f.addRoundedRect(6, 6, 12, 12, 2, 2); break;
    case Icon::Prev: line(s, 6.5, 5.5, 6.5, 18.5); poly(f, {{18, 5.5}, {9.5, 12}, {18, 18.5}}); f.closeSubpath(); break;
    case Icon::Next: line(s, 17.5, 5.5, 17.5, 18.5); poly(f, {{6, 5.5}, {14.5, 12}, {6, 18.5}}); f.closeSubpath(); break;
    case Icon::Shuffle:
        poly(s, {{3, 17}, {7, 17}, {15, 7}, {20, 7}}); poly(s, {{17, 4.5}, {20, 7}, {17, 9.5}});
        poly(s, {{3, 7}, {7, 7}, {9.5, 10}}); poly(s, {{13, 14.5}, {15, 17}, {20, 17}}); poly(s, {{17, 14.5}, {20, 17}, {17, 19.5}}); break;
    case Icon::Repeat: case Icon::RepeatOne:
        poly(s, {{4, 12}, {4, 8}, {20, 8}}); poly(s, {{17, 5.5}, {20, 8}, {17, 10.5}});
        poly(s, {{20, 12}, {20, 16}, {4, 16}}); poly(s, {{7, 13.5}, {4, 16}, {7, 18.5}});
        if (ic == Icon::RepeatOne) { poly(s, {{11, 11}, {12.4, 10}, {12.4, 14}}); }
        break;
    case Icon::Volume0: case Icon::Volume1: case Icon::Volume2: case Icon::Mute:
        poly(s, {{4, 9.5}, {8, 9.5}, {13, 5.5}, {13, 18.5}, {8, 14.5}, {4, 14.5}}); s.closeSubpath();
        if (ic == Icon::Volume1 || ic == Icon::Volume2) s.arcMoveTo(9, 8, 10, 8, 40), s.arcTo(9, 8, 10, 8, 40, -80), s.moveTo(0, 0);
        if (ic == Icon::Volume1 || ic == Icon::Volume2) { QPainterPath a; a.arcMoveTo(8.5, 8, 9, 8, 45); a.arcTo(8.5, 8, 9, 8, 45, -90); p->drawPath(a); }
        if (ic == Icon::Volume2) { QPainterPath a; a.arcMoveTo(6, 4.8, 15, 14.4, 45); a.arcTo(6, 4.8, 15, 14.4, 45, -90); p->drawPath(a); }
        if (ic == Icon::Mute) { line(s, 16.5, 9.5, 21, 14.5); line(s, 21, 9.5, 16.5, 14.5); }
        break;
    case Icon::Fullscreen:
        poly(s, {{4, 9}, {4, 4}, {9, 4}}); poly(s, {{15, 4}, {20, 4}, {20, 9}}); poly(s, {{20, 15}, {20, 20}, {15, 20}}); poly(s, {{9, 20}, {4, 20}, {4, 15}}); break;
    case Icon::ExitFullscreen:
        poly(s, {{9, 4}, {9, 9}, {4, 9}}); poly(s, {{15, 4}, {15, 9}, {20, 9}}); poly(s, {{20, 15}, {15, 15}, {15, 20}}); poly(s, {{4, 15}, {9, 15}, {9, 20}}); break;
    case Icon::Folder: poly(s, {{3, 7}, {9.5, 7}, {11.5, 9.5}, {21, 9.5}, {21, 19}, {3, 19}}); s.closeSubpath(); break;
    case Icon::FolderOpen: poly(s, {{3, 7}, {9.5, 7}, {11.5, 9.5}, {19, 9.5}, {19, 12}}); poly(s, {{3, 19}, {5.5, 12}, {22, 12}, {19.5, 19}}); s.closeSubpath(); break;
    case Icon::File: poly(s, {{6, 3.5}, {14, 3.5}, {19, 8.5}, {19, 20.5}, {6, 20.5}}); s.closeSubpath(); poly(s, {{14, 3.5}, {14, 8.5}, {19, 8.5}}); break;
    case Icon::Video: s.addRoundedRect(3.5, 5.5, 17, 13, 2.5, 2.5); poly(f, {{10.5, 9}, {15, 12}, {10.5, 15}}); f.closeSubpath(); break;
    case Icon::Music: s.addEllipse(QPointF(8, 17.5), 2.8, 2.5); s.addEllipse(QPointF(17, 15.5), 2.8, 2.5);
        poly(s, {{10.8, 17.5}, {10.8, 6}, {19.8, 4}, {19.8, 15.5}}); break;
    case Icon::Image: s.addRoundedRect(3.5, 4.5, 17, 15, 2.5, 2.5); s.addEllipse(QPointF(9, 10), 1.6, 1.6); poly(s, {{4, 17.5}, {9.5, 13}, {13, 16}, {16, 13.5}, {20, 17}}); break;
    case Icon::Gif: s.addRoundedRect(3, 6, 18, 12, 3, 3); p->setFont(QFont("Segoe UI", 1)); { QFont ft; ft.setPixelSize(7); ft.setBold(true); p->setFont(ft); p->drawText(QRectF(3, 6, 18, 12), Qt::AlignCenter, "GIF"); } break;
    case Icon::Search: s.addEllipse(QPointF(10.5, 10.5), 6, 6); line(s, 15, 15, 20, 20); break;
    case Icon::Close: line(s, 6, 6, 18, 18); line(s, 18, 6, 6, 18); break;
    case Icon::Minimize: line(s, 6, 12, 18, 12); break;
    case Icon::Maximize: s.addRoundedRect(6, 6, 12, 12, 1.5, 1.5); break;
    case Icon::Restore: s.addRoundedRect(5.5, 9, 9.5, 9.5, 1.5, 1.5); poly(s, {{9, 9}, {9, 6}, {18.5, 6}, {18.5, 15.5}, {15, 15.5}}); break;
    case Icon::Plus: line(s, 12, 5, 12, 19); line(s, 5, 12, 19, 12); break;
    case Icon::Minus: line(s, 5, 12, 19, 12); break;
    case Icon::Settings:
        line(s, 4, 7, 20, 7); line(s, 4, 17, 20, 17); s.addEllipse(QPointF(9, 7), 2.4, 2.4); s.addEllipse(QPointF(15, 17), 2.4, 2.4); break;
    case Icon::More: f.addEllipse(QPointF(5.5, 12), 1.7, 1.7); f.addEllipse(QPointF(12, 12), 1.7, 1.7); f.addEllipse(QPointF(18.5, 12), 1.7, 1.7); break;
    case Icon::List: line(s, 9, 7, 20, 7); line(s, 9, 12, 20, 12); line(s, 9, 17, 20, 17); f.addEllipse(QPointF(5, 7), 1.2, 1.2); f.addEllipse(QPointF(5, 12), 1.2, 1.2); f.addEllipse(QPointF(5, 17), 1.2, 1.2); break;
    case Icon::Grid: s.addRoundedRect(4, 4, 6.5, 6.5, 1.5, 1.5); s.addRoundedRect(13.5, 4, 6.5, 6.5, 1.5, 1.5); s.addRoundedRect(4, 13.5, 6.5, 6.5, 1.5, 1.5); s.addRoundedRect(13.5, 13.5, 6.5, 6.5, 1.5, 1.5); break;
    case Icon::Sidebar: s.addRoundedRect(3.5, 5, 17, 14, 2.5, 2.5); line(s, 9.5, 5, 9.5, 19); break;
    case Icon::ChevronLeft: poly(s, {{14.5, 6}, {8.5, 12}, {14.5, 18}}); break;
    case Icon::ChevronRight: poly(s, {{9.5, 6}, {15.5, 12}, {9.5, 18}}); break;
    case Icon::ChevronUp: poly(s, {{6, 14.5}, {12, 8.5}, {18, 14.5}}); break;
    case Icon::ChevronDown: poly(s, {{6, 9.5}, {12, 15.5}, {18, 9.5}}); break;
    case Icon::Up: line(s, 12, 19, 12, 6); poly(s, {{6.5, 11.5}, {12, 6}, {17.5, 11.5}}); break;
    case Icon::Mini: s.addRoundedRect(3.5, 5, 17, 14, 2.5, 2.5); f.addRoundedRect(11.5, 11.5, 7, 5, 1.2, 1.2); break;
    case Icon::Pen: poly(s, {{4, 20}, {5, 15.5}, {16, 4.5}, {19.5, 8}, {8.5, 19}}); s.closeSubpath(); line(s, 14, 6.5, 17.5, 10); break;
    case Icon::Highlighter: poly(s, {{5, 15}, {14, 6}, {18, 10}, {9, 19}, {5, 19}}); s.closeSubpath(); line(s, 4, 21, 20, 21); break;
    case Icon::Arrow: line(s, 5, 19, 19, 5); poly(s, {{9.5, 5}, {19, 5}, {19, 14.5}}); break;
    case Icon::Rect: s.addRoundedRect(4.5, 6.5, 15, 11, 1.5, 1.5); break;
    case Icon::Ellipse: s.addEllipse(QPointF(12, 12), 8, 6); break;
    case Icon::Text: line(s, 6, 6, 18, 6); line(s, 12, 6, 12, 19); line(s, 9.5, 19, 14.5, 19); break;
    case Icon::Undo: poly(s, {{8.5, 5.5}, {4.5, 9.5}, {8.5, 13.5}}); { QPainterPath a; a.moveTo(5, 9.5); a.lineTo(14, 9.5); a.cubicTo(18.5, 9.5, 20, 12, 20, 14.5); a.cubicTo(20, 17.5, 18, 19, 14, 19); a.lineTo(10, 19); p->drawPath(a); } break;
    case Icon::Redo: poly(s, {{15.5, 5.5}, {19.5, 9.5}, {15.5, 13.5}}); { QPainterPath a; a.moveTo(19, 9.5); a.lineTo(10, 9.5); a.cubicTo(5.5, 9.5, 4, 12, 4, 14.5); a.cubicTo(4, 17.5, 6, 19, 10, 19); a.lineTo(14, 19); p->drawPath(a); } break;
    case Icon::Trash: line(s, 4.5, 7, 19.5, 7); poly(s, {{9, 7}, {9, 4.5}, {15, 4.5}, {15, 7}}); poly(s, {{6.5, 7}, {7.5, 20}, {16.5, 20}, {17.5, 7}}); line(s, 10, 10.5, 10, 16.5); line(s, 14, 10.5, 14, 16.5); break;
    case Icon::Export: line(s, 12, 4.5, 12, 15); poly(s, {{7.5, 10.5}, {12, 15}, {16.5, 10.5}}); poly(s, {{4.5, 15}, {4.5, 19.5}, {19.5, 19.5}, {19.5, 15}}); break;
    case Icon::Copy: s.addRoundedRect(8.5, 8.5, 11, 11, 2, 2); poly(s, {{15.5, 8.5}, {15.5, 5.5}, {5.5, 5.5}, {5.5, 15.5}, {8.5, 15.5}}); break;
    case Icon::Equalizer: line(s, 6, 4, 6, 20); line(s, 12, 4, 12, 20); line(s, 18, 4, 18, 20); f.addEllipse(QPointF(6, 14), 2.2, 2.2); f.addEllipse(QPointF(12, 8), 2.2, 2.2); f.addEllipse(QPointF(18, 15.5), 2.2, 2.2); break;
    case Icon::Speed: { QPainterPath a; a.arcMoveTo(3.5, 5.5, 17, 17, 200); a.arcTo(3.5, 5.5, 17, 17, 200, -220); p->drawPath(a); } line(s, 12, 14, 16.5, 9); break;
    case Icon::Sun: s.addEllipse(QPointF(12, 12), 4, 4); for (int i = 0; i < 8; ++i) { const qreal a = i * M_PI / 4; line(s, 12 + 6.8 * qCos(a), 12 + 6.8 * qSin(a), 12 + 9 * qCos(a), 12 + 9 * qSin(a)); } break;
    case Icon::Moon: { QPainterPath a; a.moveTo(18.5, 15.5); a.cubicTo(11, 18.5, 5, 11.5, 9, 4.5); a.cubicTo(3, 6, 2.5, 14, 8, 18.5); a.cubicTo(12, 22, 17, 19.5, 18.5, 15.5); s = a; } break;
    case Icon::Back10: case Icon::Fwd10: {
        const bool fw = ic == Icon::Fwd10;
        QPainterPath a; a.arcMoveTo(4.5, 5.5, 15, 15, fw ? 70 : 110); a.arcTo(4.5, 5.5, 15, 15, fw ? 70 : 110, fw ? -300 : 300); p->drawPath(a);
        if (fw) poly(s, {{12, 2.5}, {15, 5.2}, {12, 8}}); else poly(s, {{12, 2.5}, {9, 5.2}, {12, 8}});
        QFont ft; ft.setPixelSize(8); ft.setBold(true); p->setFont(ft); p->drawText(QRectF(4.5, 6.5, 15, 15), Qt::AlignCenter, "10"); break; }
    case Icon::Check: poly(s, {{5, 12.5}, {10, 17.5}, {19, 7}}); break;
    case Icon::Queue: line(s, 4, 7, 15, 7); line(s, 4, 12, 15, 12); line(s, 4, 17, 11, 17); poly(f, {{16.5, 14}, {21, 17}, {16.5, 20}}); f.closeSubpath(); break;
    case Icon::Home: poly(s, {{4, 11}, {12, 4.5}, {20, 11}}); poly(s, {{6, 10}, {6, 19.5}, {18, 19.5}, {18, 10}}); break;
    case Icon::Subtitles: s.addRoundedRect(3.5, 5.5, 17, 13, 2.5, 2.5); line(s, 7, 11, 11, 11); line(s, 13, 11, 17, 11); line(s, 7, 14.5, 9.5, 14.5); line(s, 11.5, 14.5, 17, 14.5); break;
    case Icon::Audio: poly(s, {{4, 9.5}, {8, 9.5}, {13, 5.5}, {13, 18.5}, {8, 14.5}, {4, 14.5}}); s.closeSubpath(); line(s, 17, 9.5, 17, 14.5); line(s, 20, 7, 20, 17); break;
    case Icon::Adjust: s.addEllipse(QPointF(12, 12), 8, 8); { QPainterPath h; h.moveTo(12, 4); h.arcTo(4, 4, 16, 16, 90, -180); h.closeSubpath(); p->fillPath(h, c); } break;
    case Icon::Pin: poly(s, {{9, 4.5}, {15, 4.5}, {14.5, 10}, {17.5, 13}, {6.5, 13}, {9.5, 10}}); s.closeSubpath(); line(s, 12, 13, 12, 20); break;
    case Icon::Camera: s.addRoundedRect(3.5, 7, 17, 12, 2.5, 2.5); poly(s, {{8.5, 7}, {10, 4.5}, {14, 4.5}, {15.5, 7}}); s.addEllipse(QPointF(12, 13), 3.2, 3.2); break;
    case Icon::Eraser: poly(s, {{4, 15}, {13, 5.5}, {19.5, 11.5}, {12, 19}, {7.5, 19}}); s.closeSubpath(); line(s, 8.5, 10, 15, 16.5); break;
    case Icon::Reveal: poly(s, {{3, 7}, {9.5, 7}, {11.5, 9.5}, {21, 9.5}, {21, 19}, {3, 19}}); s.closeSubpath(); poly(s, {{11, 16}, {15.5, 12.5}}); poly(s, {{12.5, 12.5}, {15.5, 12.5}, {15.5, 15.5}}); break;
    case Icon::Drive: s.addRoundedRect(3.5, 8, 17, 8, 2, 2); f.addEllipse(QPointF(7.5, 12), 1, 1); line(s, 11, 12, 18, 12); break;
    }
    if (!f.isEmpty()) { p->setBrush(c); p->setPen(QPen(c, sw * 0.55, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin)); p->drawPath(f); p->setBrush(Qt::NoBrush); p->setPen(pen); }
    if (!s.isEmpty()) p->drawPath(s);
    p->restore();
}
