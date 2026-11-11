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
    m_rows.clear();
    const qreal cw = qMin<qreal>(520, width() - 48), x = (width() - cw) / 2;
    qreal y = height() / 2.0 - 70;
    m_btn = QRectF(x, y + 96, 150, 36);
    qreal ry = y + 170;
    for (int i = 0; i < qMin(5, m_recent.size()); ++i) { m_rows << QRectF(x, ry, cw, 34); ry += 36; }
}

void EmptyState::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.fillRect(rect(), c.bg);
    const qreal cw = qMin<qreal>(520, width() - 48), x = (width() - cw) / 2, y = height() / 2.0 - 70;
    drawIcon(&p, Icon::Video, QRectF(x, y - 6, 34, 34), c.accent, 1.5);
    p.setPen(c.text); p.setFont(Theme::i().font(26, QFont::DemiBold, true));
    p.drawText(QRectF(x + 46, y - 8, cw, 40), Qt::AlignVCenter, tr("Drop something to play"));
    p.setPen(c.muted); p.setFont(Theme::i().font(14));
    p.drawText(QRectF(x, y + 42, cw, 44), Qt::AlignTop | Qt::TextWordWrap, tr("Videos, music, images and GIFs. Drop files or a folder here, or browse your library in the sidebar."));
    const bool hb = m_hover == 100;
    p.setPen(Qt::NoPen); p.setBrush(hb ? c.accent.lighter(110) : c.accent);
    p.drawRoundedRect(m_btn, 8, 8);
    p.setPen(c.onAccent); p.setFont(Theme::i().font(14, QFont::DemiBold));
    p.drawText(m_btn, Qt::AlignCenter, tr("Add a folder"));
    if (!m_recent.isEmpty()) {
        p.setPen(c.muted); p.setFont(Theme::i().font(11, QFont::DemiBold));
        p.drawText(QRectF(x, y + 146, cw, 20), Qt::AlignVCenter, tr("RECENT"));
    }
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
    for (int i = 0; i < m_rows.size(); ++i) if (m_rows[i].contains(e->position())) h = i;
    if (h != m_hover) { m_hover = h; setCursor(h >= 0 ? Qt::PointingHandCursor : Qt::ArrowCursor); update(); }
}
void EmptyState::mousePressEvent(QMouseEvent* e) {
    if (m_btn.contains(e->position())) { emit openFolderRequested(); return; }
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
