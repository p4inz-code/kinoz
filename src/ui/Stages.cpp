// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/Stages.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include <QFileInfo>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>

EmptyState::EmptyState(QWidget* parent) : QWidget(parent) {
    setMouseTracking(true);
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

void EmptyState::relayout() {
    m_rows.clear(); m_folderRows.clear();
    const qreal cw = qMin<qreal>(560, width() - 48), x = (width() - cw) / 2;
    const int nf = qMin(4, int(m_folders.size())), nr = qMin(nf > 0 ? 3 : 5, int(m_recent.size()));
    qreal y = height() / 2.0 - 70 - (nf + nr) * 14;
    if (y < 24) y = 24;
    m_btn = QRectF(x, y + 96, 150, 38);
    m_btn2 = QRectF(x + 160, y + 96, 170, 38);
    m_btn3 = QRectF(x + 340, y + 96, qMax<qreal>(100, cw - 340), 38);
    qreal ry = y + 170;
    if (nf > 0) ry += 22;
    for (int i = 0; i < nf; ++i) { m_folderRows << QRectF(x, ry, cw, 38); ry += 40; }
    if (nr > 0) ry += (nf > 0 ? 28 : 22);
    for (int i = 0; i < nr; ++i) { m_rows << QRectF(x, ry, cw, 34); ry += 36; }
}

void EmptyState::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.fillRect(rect(), c.bg);
    const qreal cw = qMin<qreal>(560, width() - 48), x = (width() - cw) / 2;
    const int nf = qMin(4, int(m_folders.size()));
    qreal y = height() / 2.0 - 70 - (nf + int(m_rows.size())) * 14;
    if (y < 24) y = 24;
    drawIcon(&p, Icon::Video, QRectF(x, y - 6, 34, 34), c.accent, 1.5);
    p.setPen(c.text); p.setFont(Theme::i().font(26, QFont::DemiBold, true));
    p.drawText(QRectF(x + 46, y - 8, cw, 40), Qt::AlignVCenter, tr("Drop a folder to play it"));
    p.setPen(c.muted); p.setFont(Theme::i().font(14));
    p.drawText(QRectF(x, y + 42, cw, 48), Qt::AlignTop | Qt::TextWordWrap, tr("Everything inside plays, subfolders too. Or drop files, paste a folder path, or browse your library in the sidebar."));
    auto button = [&](const QRectF& r, int id, const QString& text, bool primary) {
        const bool hot = m_hover == id;
        if (primary) { p.setPen(Qt::NoPen); p.setBrush(hot ? c.accent.lighter(110) : c.accent); p.drawRoundedRect(r, 8, 8); p.setPen(c.onAccent); }
        else { p.setPen(QPen(hot ? c.accent : c.border, 1)); p.setBrush(hot ? c.accentSubtle : Qt::transparent); p.drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8); p.setPen(c.text); }
        p.setFont(Theme::i().font(14, QFont::DemiBold)); p.drawText(r, Qt::AlignCenter, text);
    };
    button(m_btn, 100, tr("Play a folder"), true);
    button(m_btn2, 101, tr("Paste a path"), false);
    button(m_btn3, 102, tr("Make default"), false);
    auto heading = [&](qreal ty, const QString& t) { p.setPen(c.muted); p.setFont(Theme::i().font(11, QFont::DemiBold)); p.drawText(QRectF(x, ty, cw, 20), Qt::AlignVCenter, t); };
    if (!m_folderRows.isEmpty()) heading(m_folderRows.first().top() - 24, tr("RECENT FOLDERS"));
    for (int i = 0; i < m_folderRows.size(); ++i) {
        const QRectF r = m_folderRows[i];
        if (m_hover == 200 + i) { p.setPen(Qt::NoPen); p.setBrush(c.surface); p.drawRoundedRect(r, 8, 8); }
        drawIcon(&p, Icon::Folder, QRectF(r.left() + 10, r.center().y() - 10, 20, 20), c.accent, 1.5);
        const QString name = QFileInfo(m_folders[i].path).fileName().isEmpty() ? m_folders[i].path : QFileInfo(m_folders[i].path).fileName();
        p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::Medium));
        const qreal tw = r.width() - 48;
        if (m_folders[i].counts.isEmpty()) p.drawText(QRectF(r.left() + 42, r.top(), tw, r.height()), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(name, Qt::ElideRight, int(tw)));
        else {
            p.drawText(QRectF(r.left() + 42, r.top() + 2, tw, 20), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(name, Qt::ElideRight, int(tw)));
            p.setPen(c.muted); p.setFont(Theme::i().font(12));
            p.drawText(QRectF(r.left() + 42, r.top() + 20, tw, 16), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(m_folders[i].counts, Qt::ElideRight, int(tw)));
        }
    }
    if (!m_rows.isEmpty()) heading(m_rows.first().top() - 24, tr("RECENT FILES"));
    for (int i = 0; i < m_rows.size(); ++i) {
        const QRectF r = m_rows[i];
        if (i == m_hover) { p.setPen(Qt::NoPen); p.setBrush(c.surface); p.drawRoundedRect(r, 8, 8); }
        const QFileInfo fi(m_recent[i]);
        p.setPen(c.text); p.setFont(Theme::i().font(14));
        p.drawText(r.adjusted(12, 0, -12, 0), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(fi.completeBaseName(), Qt::ElideRight, int(r.width() - 24)));
    }
}

void EmptyState::mouseMoveEvent(QMouseEvent* e) {
    int h = -1;
    if (m_btn.contains(e->position())) h = 100;
    if (m_btn2.contains(e->position())) h = 101;
    if (m_btn3.contains(e->position())) h = 102;
    for (int i = 0; i < m_folderRows.size(); ++i) if (m_folderRows[i].contains(e->position())) h = 200 + i;
    for (int i = 0; i < m_rows.size(); ++i) if (m_rows[i].contains(e->position())) h = i;
    if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
}
void EmptyState::mousePressEvent(QMouseEvent* e) {
    if (m_btn.contains(e->position())) { emit openFolderRequested(); return; }
    if (m_btn2.contains(e->position())) { emit pastePathRequested(); return; }
    if (m_btn3.contains(e->position())) { emit defaultPlayerRequested(); return; }
    for (int i = 0; i < m_folderRows.size(); ++i) if (m_folderRows[i].contains(e->position())) { emit folderClicked(m_folders[i].path); return; }
    for (int i = 0; i < m_rows.size(); ++i) if (m_rows[i].contains(e->position())) { emit recentClicked(m_recent[i]); return; }
}

AudioStage::AudioStage(QWidget* parent) : QWidget(parent) {
    setAttribute(Qt::WA_TransparentForMouseEvents);
    connect(&Theme::i(), &Theme::changed, this, qOverload<>(&QWidget::update));
}

void AudioStage::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    if (!m_cover) {
        QLinearGradient g(0, 0, 0, height());
        g.setColorAt(0, c.dark ? c.surface : c.bg); g.setColorAt(1, c.dark ? c.sunken : c.surface);
        p.fillRect(rect(), g);
        const qreal s = qMin(width(), height()) * 0.28;
        const QRectF art(width() / 2.0 - s / 2, height() * 0.36 - s / 2, s, s);
        p.setPen(Qt::NoPen); p.setBrush(c.accentSubtle); p.drawRoundedRect(art, s * 0.12, s * 0.12);
        drawIcon(&p, Icon::Music, art.adjusted(s * 0.25, s * 0.25, -s * 0.25, -s * 0.25), c.accent, 1.3);
        p.setPen(c.text); p.setFont(Theme::i().font(28, QFont::DemiBold, true));
        p.drawText(QRectF(40, art.bottom() + 28, width() - 80, 42), Qt::AlignHCenter | Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(m_title, Qt::ElideRight, width() - 80));
        p.setPen(c.muted); p.setFont(Theme::i().font(16));
        p.drawText(QRectF(40, art.bottom() + 74, width() - 80, 26), Qt::AlignHCenter | Qt::AlignVCenter, m_artist);
        p.setFont(Theme::i().font(14));
        p.drawText(QRectF(40, art.bottom() + 102, width() - 80, 24), Qt::AlignHCenter | Qt::AlignVCenter, m_album);
        return;
    }
    // cover rendered by mpv underneath: only add a legible title block at the bottom
    QLinearGradient g(0, height() - 250, 0, height());
    g.setColorAt(0, QColor(0, 0, 0, 0)); g.setColorAt(1, QColor(0, 0, 0, 200));
    p.fillRect(QRect(0, height() - 250, width(), 250), g);
    p.setPen(QColor("#ffffff")); p.setFont(Theme::i().font(24, QFont::DemiBold, true));
    p.drawText(QRectF(32, height() - 190, width() - 64, 36), Qt::AlignLeft | Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(m_title, Qt::ElideRight, width() - 64));
    p.setPen(QColor(255, 255, 255, 190)); p.setFont(Theme::i().font(15));
    p.drawText(QRectF(32, height() - 152, width() - 64, 24), Qt::AlignLeft | Qt::AlignVCenter, m_artist + (m_album.isEmpty() ? QString() : QStringLiteral("  ·  ") + m_album));
}
