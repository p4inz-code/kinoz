#include "ui/SettingsPage.h"
#include "core/JsonStore.h"
#include "core/Paths.h"
#include "core/Updater.h"
#include "ui/Overlays.h"
#include "ui/Theme.h"
#include "ui/Widgets.h"

#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QPainter>
#include <QTextBrowser>
#include <QUrl>

namespace {
struct ActionInfo { const char* id; const char* label; const char* group; };
const ActionInfo kActions[] = {
    {"playpause", "Play / pause", "Playback"}, {"stop", "Stop", "Playback"}, {"next", "Next", "Playback"}, {"prev", "Previous (or restart)", "Playback"},
    {"seekfwd", "Forward (small step)", "Playback"}, {"seekback", "Back (small step)", "Playback"}, {"seekfwd30", "Forward (large step)", "Playback"}, {"seekback30", "Back (large step)", "Playback"},
    {"seekfwd10", "Forward 10 s", "Playback"}, {"seekback10", "Back 10 s", "Playback"}, {"speedup", "Faster", "Playback"}, {"speeddown", "Slower", "Playback"},
    {"speedreset", "Normal speed", "Playback"}, {"framestep", "Next frame", "Playback"}, {"framestepback", "Previous frame", "Playback"},
    {"shuffle", "Shuffle", "Playback"}, {"repeat", "Repeat (off / all / one)", "Playback"}, {"abloop", "A-B loop (A, then B, then clear)", "Playback"},
    {"volup", "Volume up", "Sound and picture"}, {"voldown", "Volume down", "Sound and picture"}, {"mute", "Mute", "Sound and picture"}, {"eq", "Equalizer", "Sound and picture"},
    {"adjust", "Adjust picture", "Sound and picture"}, {"subcycle", "Next subtitle track", "Sound and picture"}, {"subdelayminus", "Subtitles earlier", "Sound and picture"},
    {"subdelayplus", "Subtitles later", "Sound and picture"}, {"rotate", "Rotate image", "Sound and picture"}, {"zoomin", "Zoom in", "Sound and picture"},
    {"zoomout", "Zoom out", "Sound and picture"}, {"zoomreset", "Fit to window", "Sound and picture"},
    {"fullscreen", "Fullscreen", "Window"}, {"escape", "Leave fullscreen / mini / panel", "Window"}, {"mini", "Mini player", "Window"}, {"pin", "Always on top", "Window"},
    {"sidebar", "Library sidebar", "Window"}, {"newwindow", "New window", "Window"}, {"theme", "Switch theme", "Window"},
    {"palette", "Search and commands", "Library and tools"}, {"openfolder", "Add a folder", "Library and tools"}, {"openfile", "Open a file", "Library and tools"},
    {"annotate", "Annotate frame", "Library and tools"}, {"screenshot", "Save frame as image", "Library and tools"}, {"settings", "Settings", "Library and tools"}, {"quit", "Quit", "Library and tools"},
};
}

static QColor swatchColor(int i) { return Theme::accentSample(i, pal().dark); }

SettingsPage::SettingsPage(JsonStore* set, const QHash<QString, QString>& keys, Updater* updater, const QString& diagnostics, QWidget* parent)
    : QWidget(parent), m_set(set), m_up(updater), m_keys(keys), m_diag(diagnostics) {
    m_tabs = new Segmented({tr("General"), tr("Playback"), tr("Shortcuts"), tr("Info"), tr("About"), tr("Legal")}, this);
    m_close = new IconButton(Icon::Close, this, 32);
    connect(m_close, &QAbstractButton::clicked, this, &SettingsPage::closeRequested);
    connect(m_tabs, &Segmented::currentChanged, this, [this](int i) { showPage(i); });

    // ---- General ----
    addSeg(General, "theme", {tr("System"), tr("Light"), tr("Dark")}, {0, 1, 2}, 0, tr("Theme"), tr("Match Windows, or pick light or dark"));
    m_rows[General].push_back({"accent", tr("Accent colour"), tr("Used for highlights, sliders and buttons"), nullptr});
    addSeg(General, "motion", {tr("System"), tr("On"), tr("Off")}, {0, 1, 2}, 0, tr("Animations"), tr("Off removes every fade, slide and glow — nothing moves"));
    addSwitch(General, "glass", true, tr("Glass effect"), tr("Blurred backdrop behind the control bar"));
    addSeg(General, "compact", {tr("Comfortable"), tr("Compact")}, {0, 1}, 0, tr("Density"), tr("Row and control size in lists"));
    addSwitch(General, "hwdec", true, tr("Hardware decoding"), tr("Use the GPU for video (restart to apply)"));
    addSwitch(General, "fsSafe", false, tr("Safe fullscreen"), tr("Keep the taskbar visible (try this if your screen blinks)"));
    auto* b1 = new TextButton(tr("Open data folder"), this);
    connect(b1, &QAbstractButton::clicked, this, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::dataDir())); });
    auto* b2 = new TextButton(tr("Clear thumbnails"), this); connect(b2, &QAbstractButton::clicked, this, &SettingsPage::clearThumbs);
    auto* b3 = new TextButton(tr("Clear resume history"), this); connect(b3, &QAbstractButton::clicked, this, &SettingsPage::clearResume);
    m_pageWidgets[General] << b1 << b2 << b3;

    // ---- Playback ----
    addSwitch(Playback, "resume", true, tr("Resume where you left off"), tr("Continue each video or track from its last position"));
    addSwitch(Playback, "autoplayNext", true, tr("Play next automatically"), tr("Move on to the next item when one ends"));
    addSwitch(Playback, "siblings", true, tr("Queue the whole folder"), tr("Opening one file lines up the others beside it"));
    addSwitch(Playback, "normalize", false, tr("Normalize loudness"), tr("Even out quiet and loud tracks"));
    addSeg(Playback, "defaultSpeed", {tr("0.75×"), tr("1×"), tr("1.25×"), tr("1.5×")}, {0.75, 1.0, 1.25, 1.5}, 1.0, tr("Starting speed"), tr("Speed used when a file starts"));
    addSeg(Playback, "seekSmall", {tr("5 s"), tr("10 s"), tr("15 s")}, {5, 10, 15}, 5, tr("Small seek step"), tr("Left / Right arrows"));
    addSeg(Playback, "seekLarge", {tr("30 s"), tr("1 min"), tr("2 min")}, {30, 60, 120}, 30, tr("Large seek step"), tr("Shift + Left / Right"));
    addSeg(Playback, "volStep", {tr("2"), tr("5"), tr("10")}, {2, 5, 10}, 5, tr("Volume step"), tr("Up / Down arrows and the mouse wheel"));
    addSeg(Playback, "hideDelay", {tr("2 s"), tr("3 s"), tr("5 s"), tr("Never")}, {2, 3, 5, 0}, 3, tr("Hide controls after"), tr("While a video plays and the mouse is still"));
    addSeg(Playback, "slideshow", {tr("Off"), tr("3 s"), tr("5 s"), tr("10 s")}, {0, 3, 5, 10}, 0, tr("Image slideshow"), tr("Advance pictures automatically"));

    buildShortcuts();
    buildInfo();
    buildAbout();
    showPage(0);
}

void SettingsPage::addSwitch(Page p, const QString& key, bool def, const QString& title, const QString& desc) {
    auto* s = new Switch(this);
    s->setChecked(m_set->flag(key, def));
    connect(s, &QAbstractButton::toggled, this, [this, key](bool on) { m_set->set(key, on); emit changed(key); });
    m_rows[p].push_back({key, title, desc, s});
}

void SettingsPage::addSeg(Page p, const QString& key, const QStringList& labels, const QVector<double>& values, double def, const QString& title, const QString& desc) {
    auto* s = new Segmented(labels, this);
    const double cur = m_set->num(key, def);
    int idx = 0; double best = 1e18;
    for (int i = 0; i < values.size(); ++i) if (qAbs(values[i] - cur) < best) { best = qAbs(values[i] - cur); idx = i; }
    s->setCurrent(idx);
    connect(s, &Segmented::currentChanged, this, [this, key, values](int i) { m_set->set(key, values[i]); emit changed(key); });
    m_rows[p].push_back({key, title, desc, s});
}

void SettingsPage::buildShortcuts() {
    m_keysList = new RowList(this);
    m_keysList->setRowHeight(36);
    m_keysList->setShowIcons(false);
    QHash<QString, QStringList> byAction;
    for (auto it = m_keys.begin(); it != m_keys.end(); ++it) byAction[it.value()] << it.key();
    QVector<RowList::Row> rows;
    QString lastGroup;
    for (const auto& a : kActions) {
        const QString group = tr(a.group);
        QStringList k = byAction.value(QString::fromLatin1(a.id)); k.sort();
        if (k.isEmpty()) continue;
        if (group != lastGroup) { rows.push_back({Icon::File, group, {}, {}, {}, true}); lastGroup = group; }
        rows.push_back({Icon::Check, tr(a.label), {}, k.join("   ·   ").replace("Return", "Enter").replace("Esc", "Esc"), {}, false});
    }
    rows.push_back({Icon::File, tr("Mouse"), {}, {}, {}, true});
    const std::pair<const char*, const char*> mouse[] = {
        {"Click the picture", "Play / pause"}, {"Double-click the picture", "Fullscreen"}, {"Mouse wheel", "Volume  (zoom for images)"},
        {"Drag a zoomed image", "Pan"}, {"Right-click the picture", "Menu"}, {"Hover the seek bar", "Frame preview"}, {"Drag the seek bar", "Scrub"}, {"Double-click a slider", "Reset it"}};
    for (const auto& m : mouse) rows.push_back({Icon::Check, tr(m.first), {}, tr(m.second), {}, false});
    m_keysList->setRows(rows);
    m_keysList->setCurrent(-1);
    m_openKeymap = new TextButton(tr("Customize shortcuts…"), this);
    connect(m_openKeymap, &QAbstractButton::clicked, this, [] {
        const QString f = Paths::dataDir() + QStringLiteral("/keymap.json");
        if (!QFile::exists(f)) {
            QJsonArray ids; for (const auto& a : kActions) ids << QString::fromLatin1(a.id);
            QJsonObject o; o["_info"] = QStringLiteral("Add your own keys under \"keys\", e.g. \"Ctrl+Alt+P\": \"playpause\". Restart kinoz after editing.");
            o["keys"] = QJsonObject(); o["_available_actions"] = ids;
            QFile out(f); if (out.open(QIODevice::WriteOnly)) out.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
        }
        QDesktopServices::openUrl(QUrl::fromLocalFile(f));
    });
    m_pageWidgets[Shortcuts] << m_keysList << m_openKeymap;
}

void SettingsPage::buildInfo() {
    m_infoView = new QTextBrowser(this);
    m_infoView->setFrameShape(QFrame::NoFrame);
    m_infoView->setVerticalScrollBar(new ScrollBar(Qt::Vertical, m_infoView));
    auto style = [this] {
        const auto& c = pal();
        m_infoView->setStyleSheet(QStringLiteral("QTextBrowser{background:transparent;color:%1;border:none;selection-background-color:%2;selection-color:%3;}").arg(c.text.name(), c.accent.name(), c.onAccent.name()));
        m_infoView->setFont(Theme::i().font(13));
        QString html = QStringLiteral("<style>h3{font-size:14px;margin:14px 0 4px 0;color:%1} td{padding:2px 14px 2px 0;vertical-align:top} .k{color:%2}</style>").arg(c.accent.name(), c.muted.name());
        bool open = false;
        for (const QString& line : m_diag.split('\n')) {
            if (line.startsWith("## ")) { if (open) html += "</table>"; html += "<h3>" + line.mid(3).toHtmlEscaped() + "</h3><table>"; open = true; continue; }
            const int i = line.indexOf(": ");
            if (i > 0) html += "<tr><td class=\"k\">" + line.left(i).toHtmlEscaped() + "</td><td>" + line.mid(i + 2).toHtmlEscaped() + "</td></tr>";
        }
        if (open) html += "</table>";
        m_infoView->setHtml(html);
    };
    connect(&Theme::i(), &Theme::changed, this, style);
    style();
    m_copyDiag = new TextButton(tr("Copy diagnostics"), this, false, true, Icon::Copy);
    connect(m_copyDiag, &QAbstractButton::clicked, this, [this] { QApplication::clipboard()->setText(m_diag); m_copyDiag->setText(tr("Copied")); });
    m_pageWidgets[Info] << m_infoView << m_copyDiag;
}

void SettingsPage::buildAbout() {
    m_updBtn = new TextButton(tr("Check for updates"), this, true);
    connect(m_updBtn, &QAbstractButton::clicked, this, [this] {
        switch (m_up->state()) {
        case Updater::Available: m_up->download(); break;
        case Updater::Ready: m_up->installAndQuit(); break;
        case Updater::Downloading: break;
        default: m_up->checkNow(true); break;
        }
    });
    auto* src = new TextButton(tr("Source code"), this);
    connect(src, &QAbstractButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/p4inz-code/kinoz"))); });
    auto* lic = new TextButton(tr("License"), this);
    connect(lic, &QAbstractButton::clicked, this, [this] { m_tabs->setCurrent(Legal); showPage(Legal); });
    auto* rel = new TextButton(tr("Release notes"), this);
    connect(rel, &QAbstractButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/p4inz-code/kinoz/releases"))); });
    auto* autoUpd = new Switch(this);
    autoUpd->setChecked(m_set->flag("autoUpdate", true));
    connect(autoUpd, &QAbstractButton::toggled, this, [this](bool on) { m_set->set("autoUpdate", on); });
    m_pageWidgets[About] << m_updBtn << src << lic << rel << autoUpd;

    m_legal = new QTextBrowser(this);
    m_legal->setOpenExternalLinks(true);
    m_legal->setFrameShape(QFrame::NoFrame);
    m_legal->setVerticalScrollBar(new ScrollBar(Qt::Vertical, m_legal));
    auto styleLegal = [this] {
        const auto& c = pal();
        m_legal->setStyleSheet(QStringLiteral("QTextBrowser{background:transparent;color:%1;border:none;selection-background-color:%2;selection-color:%3;}").arg(c.text.name(), c.accent.name(), c.onAccent.name()));
        m_legal->setFont(Theme::i().font(14));
        const QString h = QStringLiteral("<style>a{color:%1;text-decoration:none} h3{font-size:16px;margin:18px 0 6px 0} p,li{line-height:150%} .m{color:%2}</style>").arg(c.accent.name(), c.muted.name());
        m_legal->setHtml(h + tr(
            "<h3>License</h3><p>kinoz is free software, released under the <b>GNU General Public License v3</b> (GPLv3). You can use it for anything, "
            "study it, share it and change it. If you share it or a modified version, the result must stay under the same license, its source must be available, "
            "and the credit below must be kept. The full text is in the <b>LICENSE</b> file next to the program and at "
            "<a href=\"https://www.gnu.org/licenses/gpl-3.0.html\">gnu.org/licenses/gpl-3.0</a>.</p>"
            "<h3>Credit &amp; additional terms</h3><p>Created by <b>Atharva Patil</b>, P4inz Interactive Labs "
            "(<a href=\"https://github.com/p4inz-code\">github.com/p4inz-code</a>). Copyright © 2026 Atharva Patil.<br>"
            "As allowed by GPLv3 section 7, these additional terms apply: keep the author attribution and copyright notices; "
            "mark modified versions as modified and do not present them as the original; do not use the name “kinoz” for a modified version without permission. "
            "See <b>ADDITIONAL_TERMS.md</b>.</p>"
            "<h3>No warranty</h3><p>kinoz is provided “as is”, without warranty of any kind, to the extent permitted by law (GPLv3 sections 15–17).</p>"
            "<h3>Privacy</h3><p>kinoz works fully offline. It has no accounts, no analytics and no crash reporting. Your files, folders and playback history stay on your computer, in the data folder. "
            "The only network request it can make is the <b>update check</b> to github.com (a plain request for the latest release; nothing about you or your files is sent). "
            "You can turn it off under About → “Check for updates automatically”. Details: <b>PRIVACY.md</b>.</p>"
            "<h3>Built with</h3><ul>"
            "<li><b>mpv / libmpv</b> and <b>FFmpeg</b> — playback engine (GPL/LGPL)</li>"
            "<li><b>Qt 6</b> — user interface (LGPL v3, dynamically linked)</li>"
            "<li><b>Atkinson Hyperlegible Next</b> and <b>Familjen Grotesk</b> — fonts (SIL Open Font License 1.1)</li></ul>"
            "<p class=\"m\">Full notices: <b>THIRD_PARTY_NOTICES.md</b>. kinoz is an independent project and is not affiliated with mpv, FFmpeg or Qt.</p>"
            "<h3>Source code</h3><p><a href=\"https://github.com/p4inz-code/kinoz\">github.com/p4inz-code/kinoz</a></p>"));
    };
    connect(&Theme::i(), &Theme::changed, this, styleLegal);
    styleLegal();
    m_pageWidgets[Legal] << m_legal;

    connect(m_up, &Updater::stateChanged, this, &SettingsPage::refreshUpdate);
    connect(m_up, &Updater::progress, this, [this](qint64, qint64) { refreshUpdate(); });
    refreshUpdate();
}

void SettingsPage::refreshUpdate() {
    switch (m_up->state()) {
    case Updater::Checking: m_updBtn->setText(tr("Checking…")); m_updStatus = m_up->info(); break;
    case Updater::Available: m_updBtn->setText(m_up->info().contains(tr("release page")) ? tr("Open release page") : tr("Update to %1").arg(m_up->latest())); m_updStatus = m_up->info(); break;
    case Updater::Downloading: {
        const int pct = m_up->total() > 0 ? int(100 * m_up->received() / m_up->total()) : 0;
        m_updBtn->setText(tr("Downloading… %1%").arg(pct)); m_updStatus = m_up->info(); break; }
    case Updater::Ready: m_updBtn->setText(tr("Install and restart")); m_updStatus = m_up->info(); break;
    case Updater::Failed: m_updBtn->setText(tr("Try again")); m_updStatus = m_up->error(); break;
    case Updater::UpToDate: m_updBtn->setText(tr("Check again")); m_updStatus = m_up->info(); break;
    default: m_updBtn->setText(tr("Check for updates")); m_updStatus.clear(); break;
    }
    m_updBtn->resize(m_updBtn->sizeHint());
    update();
}

void SettingsPage::showPage(int p) {
    m_page = p;
    for (int i = 0; i < 2; ++i) for (const Row& r : m_rows[i]) if (r.w) r.w->setVisible(p == i);
    for (int i = 0; i < PageCount; ++i) for (auto* w : m_pageWidgets[i]) w->setVisible(p == i);
    m_tabs->setCurrent(p);
    update();
}

QVector<QRectF> SettingsPage::swatchRects() const {
    QVector<QRectF> v;
    // find the accent row on the General page
    int y = 118;
    for (const Row& r : m_rows[General]) {
        if (r.key == "accent") {
            qreal x = width() - 28 - Theme::accentCount() * 30 + 4;
            for (int i = 0; i < Theme::accentCount(); ++i) { v << QRectF(x, y + 14, 24, 24); x += 30; }
            break;
        }
        y += 56;
    }
    return v;
}

void SettingsPage::mousePressEvent(QMouseEvent* e) {
    if (m_page == General) {
        const auto sw = swatchRects();
        for (int i = 0; i < sw.size(); ++i)
            if (sw[i].adjusted(-3, -3, 3, 3).contains(e->position())) { m_set->set("accent", i); emit changed("accent"); update(); return; }
    }
    QWidget::mousePressEvent(e);
}

void SettingsPage::resizeEvent(QResizeEvent*) {
    m_tabs->setGeometry(24, 64, m_tabs->sizeHint().width(), 30);
    m_close->move(width() - 16 - 32, 14);
    const int right = width() - 28;
    for (int pg = 0; pg < 2; ++pg) {
        const int rh = pg == General ? 56 : 48;
        int y = 118;
        for (const Row& r : m_rows[pg]) {
            if (r.w) { const QSize s = r.w->sizeHint(); r.w->setGeometry(right - s.width(), y + (rh - s.height()) / 2, s.width(), s.height()); }
            y += rh;
        }
        if (pg == General) {
            int x = 24; y += 8;
            for (auto* w : m_pageWidgets[General]) { const QSize s = w->sizeHint(); w->setGeometry(x, y, s.width(), s.height()); x += s.width() + 10; }
        }
    }
    const QRect content(24, 116, width() - 48, height() - 116 - 68);
    if (m_keysList) m_keysList->setGeometry(content.left() - 12, content.top(), content.width() + 24, content.height());
    if (m_openKeymap) m_openKeymap->setGeometry(24, height() - 56, m_openKeymap->sizeHint().width(), 36);
    if (m_infoView) m_infoView->setGeometry(content);
    if (m_copyDiag) m_copyDiag->setGeometry(24, height() - 56, m_copyDiag->sizeHint().width(), 36);
    if (m_legal) m_legal->setGeometry(24, 116, width() - 48, height() - 132);
    if (m_pageWidgets[About].size() == 5) {
        int x = 24; const int y = 504;
        m_updBtn->resize(m_updBtn->sizeHint()); m_updBtn->move(x, y); x += m_updBtn->width() + 10;
        for (int i = 1; i <= 3; ++i) { auto* w = m_pageWidgets[About][i]; const QSize s = w->sizeHint(); w->setGeometry(x, y, s.width(), s.height()); x += s.width() + 10; }
        m_pageWidgets[About][4]->move(width() - 28 - 38, 564);
    }
}

void SettingsPage::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.setPen(c.text); p.setFont(Theme::i().font(22, QFont::DemiBold, true));
    p.drawText(QRectF(24, 14, 300, 40), Qt::AlignVCenter, tr("Settings"));
    p.setPen(c.border); p.drawLine(24, 104, width() - 24, 104);

    if (m_page == General || m_page == Playback) {
        const int rh = m_page == General ? 56 : 48;
        int y = 118;
        for (const Row& r : m_rows[m_page]) {
            p.setPen(c.text); p.setFont(Theme::i().font(15, QFont::Medium)); p.drawText(QRectF(24, y + (rh == 56 ? 6 : 4), width() * 0.52, 22), Qt::AlignVCenter, r.title);
            p.setPen(c.muted); p.setFont(Theme::i().font(12)); p.drawText(QRectF(24, y + (rh == 56 ? 28 : 25), width() * 0.5, 18), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(r.desc, Qt::ElideRight, int(width() * 0.5)));
            y += rh;
        }
        if (m_page == General) {
            const auto sw = swatchRects();
            const int cur = int(m_set->num("accent", 0));
            for (int i = 0; i < sw.size(); ++i) {
                p.setPen(Qt::NoPen); p.setBrush(swatchColor(i)); p.drawEllipse(sw[i]);
                if (i == cur) { p.setBrush(Qt::NoBrush); p.setPen(QPen(c.text, 2)); p.drawEllipse(sw[i].adjusted(-4, -4, 4, 4)); }
            }
            p.setPen(c.muted); p.setFont(Theme::i().font(12));
            p.drawText(QRectF(24, y + 56, width() - 48, 40), Qt::AlignTop | Qt::TextWordWrap, tr("Everything kinoz saves — settings, thumbnails, resume points, exports — lives in the data folder. On Windows that is next to the program, so nothing is spread over C:."));
        }
    } else if (m_page == Shortcuts) {
        p.setPen(c.muted); p.setFont(Theme::i().font(12));
        p.drawText(QRectF(width() - 24 - 360, height() - 56, 360, 36), Qt::AlignVCenter | Qt::AlignRight, tr("Add or change keys in keymap.json, then restart."));
    } else if (m_page == About) {
        p.setPen(c.text); p.setFont(Theme::i().font(30, QFont::DemiBold, true)); p.drawText(QRectF(24, 118, width() - 48, 42), Qt::AlignVCenter, QStringLiteral(APP_NAME));
        p.setPen(c.muted); p.setFont(Theme::i().font(14));
        p.drawText(QRectF(24, 160, width() - 48, 24), Qt::AlignVCenter, tr("A calm, fast, offline player for videos, music, images and GIFs."));
        struct KV { QString k, v; };
        const QVector<KV> rows = {
            {tr("Version"), QStringLiteral(APP_VERSION)},
            {tr("License"), tr("GNU GPL v3 — free software, credit required")},
            {tr("Created by"), QStringLiteral("Atharva Patil · P4inz Interactive Labs")},
            {tr("Project"), QStringLiteral("github.com/p4inz-code/kinoz")},
            {tr("Copyright"), QStringLiteral("© 2026 Atharva Patil")},
            {tr("Data folder"), QDir::toNativeSeparators(Paths::dataDir())},
        };
        int y = 204;
        for (const auto& r : rows) {
            p.setFont(Theme::i().font(13)); p.setPen(c.muted); p.drawText(QRectF(24, y, 130, 24), Qt::AlignVCenter, r.k);
            p.setPen(c.text); p.drawText(QRectF(160, y, width() - 184, 24), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(r.v, Qt::ElideMiddle, width() - 184));
            y += 28;
        }
        p.setPen(c.border); p.drawLine(24, y + 8, width() - 24, y + 8);
        p.setPen(c.muted); p.setFont(Theme::i().font(13));
        p.drawText(QRectF(24, y + 20, width() - 48, 60), Qt::AlignTop | Qt::TextWordWrap, tr("Free and open source. Anyone may use and change it; versions they share must stay free and keep this credit. Engine, framework and font details are on the Info and Legal tabs."));
        if (!m_updStatus.isEmpty()) { p.setPen(m_up->state() == Updater::Failed ? c.danger : c.muted); p.drawText(QRectF(24, 474, width() - 48, 22), Qt::AlignVCenter, m_updStatus); }
        p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::Medium)); p.drawText(QRectF(24, 560, width() - 120, 26), Qt::AlignVCenter, tr("Check for updates automatically"));
        p.setPen(c.muted); p.setFont(Theme::i().font(12)); p.drawText(QRectF(24, 584, width() - 80, 20), Qt::AlignVCenter, tr("Once a day, asks github.com for the newest version. Sends nothing about you."));
    }
}
