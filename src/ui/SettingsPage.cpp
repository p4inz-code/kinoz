// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
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
#include <QDirIterator>
#include <QFileInfo>
#include <QLocale>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QProcess>
#include <QTimer>
#include <QTextBrowser>
#include <QUrl>

namespace {
struct ActionInfo { const char* id; const char* label; const char* group; };
const ActionInfo kActions[] = {
    {"playpause", "Play / pause", "Playback"}, {"stop", "Stop", "Playback"}, {"next", "Next", "Playback"}, {"prev", "Previous (or restart)", "Playback"}, {"nextfile", "Next file", "Playback"}, {"prevfile", "Previous file", "Playback"},
    {"seekfwd", "Forward (small step)", "Playback"}, {"seekback", "Back (small step)", "Playback"}, {"seekfwd30", "Forward (large step)", "Playback"}, {"seekback30", "Back (large step)", "Playback"},
    {"skipfwd", "Skip forward (your step)", "Playback"}, {"skipback", "Skip back (your step)", "Playback"}, {"seekfwd10", "Forward 10 s", "Playback"}, {"seekback10", "Back 10 s", "Playback"}, {"speedup", "Faster", "Playback"}, {"speeddown", "Slower", "Playback"},
    {"speedreset", "Normal speed", "Playback"}, {"framestep", "Next frame", "Playback"}, {"framestepback", "Previous frame", "Playback"},
    {"shuffle", "Shuffle", "Playback"}, {"repeat", "Repeat (off / all / one)", "Playback"}, {"abloop", "A-B loop (A, then B, then clear)", "Playback"},
    {"seekpct1", "Jump to 10% … 90% (videos and tracks)", "Playback"}, {"keep", "Keep this file (mark)", "Skimming"}, {"reject", "Mark this file to reject", "Skimming"},
    {"nextunwatched", "Next file I haven't seen", "Skimming"}, {"trash", "Delete this file (to the Recycle Bin, asks first)", "Skimming"},
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
    setFocusPolicy(Qt::StrongFocus);
    m_tabs = new Segmented({tr("General"), tr("Playback"), tr("Privacy"), tr("Shortcuts"), tr("Info"), tr("About"), tr("Legal")}, this);
    m_close = new IconButton(Icon::Close, this, 32);
    m_close->setToolTip(tr("Close\tEsc"));
    connect(m_close, &QAbstractButton::clicked, this, &SettingsPage::closeRequested);
    connect(m_tabs, &Segmented::currentChanged, this, [this](int i) { showPage(i); });

    // ---- General ----
    addSeg(General, "theme", {tr("System"), tr("Light"), tr("Dark")}, {0, 1, 2}, 0, tr("Theme"), tr("Match Windows, or pick light or dark"));
    m_rows[General].push_back({"accent", tr("Accent colour"), tr("Used for highlights, sliders and buttons"), nullptr});
    addSeg(General, "motion", {tr("System"), tr("On"), tr("Off")}, {0, 1, 2}, 0, tr("Animations"), tr("Off removes every fade, slide and glow — nothing moves"));
    addSwitch(General, "glass", true, tr("Glass effect"), tr("Blurred backdrop behind the control bar"));
    addSeg(General, "compact", {tr("Comfortable"), tr("Compact")}, {0, 1}, 0, tr("Density"), tr("Row and control size in lists"));
    if (!m_set->has("decode")) m_set->set("decode", m_set->flag("hwdec", true) ? 0 : 2);        // from the old on/off switch
    addSeg(General, "decode", {tr("Auto"), tr("GPU"), tr("CPU")}, {0, 1, 2}, 0, tr("Video decoding"), tr("Auto starts every file instantly and uses the GPU for 1440p and larger"));
    addSwitch(General, "fsSafe", false, tr("Safe fullscreen"), tr("Keep the taskbar visible (try this if your screen blinks)"));
    auto* defBtn = new TextButton(tr("Make default…"), this);
    connect(defBtn, &QAbstractButton::clicked, this, &SettingsPage::makeDefault);
    m_rows[General].push_back({"defaultPlayer", tr("Default player"), tr("Open your videos, music and pictures with kinoz"), defBtn});
    auto* b1 = new TextButton(tr("Open data folder"), this);
    connect(b1, &QAbstractButton::clicked, this, [] { QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::dataDir())); });
    m_pageWidgets[General] << b1;
#ifdef Q_OS_WIN
    auto* b4 = new TextButton(tr("Uninstall…"), this);
    b4->setDanger(true);
    connect(b4, &QAbstractButton::clicked, this, [] { QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--uninstall")}); });
    m_pageWidgets[General] << b4;
#endif

    // ---- Playback ----
    addSwitch(Playback, "resume", true, tr("Resume where you left off"), tr("Continue each video or track from its last position"));
    addSwitch(Playback, "autoplayNext", true, tr("Play next automatically"), tr("Move on to the next item when one ends"));
    addSwitch(Playback, "siblings", true, tr("Queue the whole folder"), tr("Opening one file lines up the others beside it"));
    addSwitch(Playback, "rememberMute", true, tr("Remember mute"), tr("Quit muted, start muted. Clean identity resets it"));
    addSwitch(Playback, "normalize", false, tr("Normalize loudness"), tr("Even out quiet and loud tracks"));
    addSeg(Playback, "defaultSpeed", {tr("0.75×"), tr("1×"), tr("1.25×"), tr("1.5×")}, {0.75, 1.0, 1.25, 1.5}, 1.0, tr("Starting speed"), tr("Speed used when a file starts"));
    {   // Skip step: the two skip buttons and J / L. 5, 10, 15 or any number you like.
        m_skipSeg = new Segmented({tr("5 s"), tr("10 s"), tr("15 s"), tr("Custom")}, this);
        m_skipStepper = new Stepper(1, 600, tr("s"), this);
        const int cur = qBound(1, int(m_set->num("skipStep", 10)), 600);
        m_skipStepper->setValue(cur);
        m_skipSeg->setCurrent(cur == 5 ? 0 : cur == 10 ? 1 : cur == 15 ? 2 : 3);
        m_skipStepper->setVisible(false);
        connect(m_skipSeg, &Segmented::currentChanged, this, [this](int i) {
            if (i < 3) { m_set->set("skipStep", i == 0 ? 5 : i == 1 ? 10 : 15); emit changed("skipStep"); }
            else { m_set->set("skipStep", m_skipStepper->value()); emit changed("skipStep"); }
            m_skipStepper->setVisible(m_page == Playback && i == 3);
            resizeEvent(nullptr);
        });
        connect(m_skipStepper, &Stepper::valueChanged, this, [this](int v) { m_set->set("skipStep", v); emit changed("skipStep"); });
        m_rows[Playback].push_back({"skipStep", tr("Skip step"), tr("The skip buttons and J / L"), m_skipSeg});
    }
    addSeg(Playback, "seekSmall", {tr("5 s"), tr("10 s"), tr("15 s")}, {5, 10, 15}, 5, tr("Small seek step"), tr("Ctrl + Left / Right"));
    addSeg(Playback, "seekLarge", {tr("30 s"), tr("1 min"), tr("2 min")}, {30, 60, 120}, 30, tr("Large seek step"), tr("Shift + Left / Right"));
    addSeg(Playback, "volStep", {tr("2"), tr("5"), tr("10")}, {2, 5, 10}, 5, tr("Volume step"), tr("Up / Down arrows and the mouse wheel"));
    addSeg(Playback, "hideDelay", {tr("2 s"), tr("3 s"), tr("5 s"), tr("Never")}, {2, 3, 5, 0}, 2, tr("Hide controls after"), tr("While a video plays and the mouse is still"));
    addSwitch(Playback, "autoHideLibrary", true, tr("Hide the library with the controls"), tr("Move to the left edge to bring it back"));
    addSeg(Playback, "slideshow", {tr("Off"), tr("3 s"), tr("5 s"), tr("10 s")}, {0, 3, 5, 10}, 0, tr("Image slideshow"), tr("Advance pictures automatically"));

    buildPrivacy();
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

void SettingsPage::buildPrivacy() {
    addSwitch(Privacy, "privateMode", false, tr("Private mode"), tr("No recent list, resume points or saved thumbnails"));
    addSwitch(Privacy, "forgetOnExit", false, tr("Clean identity when kinoz closes"), tr("Wipes your history every time you close kinoz"));
    struct PB { const char* id; QString label; bool primary; };
    const PB items[] = {
        {"clearRecent", tr("Clear recent files"), false}, {"clearResume", tr("Clear resume points"), false}, {"clearMarks", tr("Clear watched marks"), false},
        {"clearThumbs", tr("Clear thumbnails"), false}, {"clearMusic", tr("Clear music index"), false}, {"clearLogs", tr("Clear logs"), false},
        {"cleanIdentity", tr("Clean identity…"), true}, {"forgetFolders", tr("Forget my folders…"), false}, {"openData", tr("Open data folder"), false}};
    for (const PB& it : items) {
        auto* b = new TextButton(it.label, this, it.primary);
        if (it.primary) b->setDanger(true);
        const QString id = QString::fromLatin1(it.id);
        connect(b, &QAbstractButton::clicked, this, [this, id] { emit privacyAction(id); if (id != "openData") QTimer::singleShot(900, this, [this] { refreshPrivacy(); }); });
        m_pageWidgets[Privacy] << b;
    }
}

// What kinoz keeps on disk, with sizes, so nothing is a mystery.
void SettingsPage::refreshPrivacy() {
    auto sizeOf = [](const QString& dir, const QStringList& filters, int* count) {
        qint64 total = 0; int n = 0;
        for (const QFileInfo& f : QDir(dir).entryInfoList(filters, QDir::Files)) { total += f.size(); ++n; }
        if (count) *count = n;
        return total;
    };
    auto keysIn = [](const QString& file) { QFile f(file); if (!f.open(QIODevice::ReadOnly)) return 0; return int(QJsonDocument::fromJson(f.readAll()).object().size()); };
    const QString d = Paths::dataDir();
    int nThumb = 0;
    const qint64 thumbs = sizeOf(Paths::thumbsDir(), {QStringLiteral("*.jpg")}, &nThumb);
    qint64 total = 0;
    QDirIterator it(d, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) { it.next(); total += it.fileInfo().size(); }
    const QLocale loc;
    m_stored.clear();
    m_stored << tr("Recent files: %1").arg(m_set->get("recent").toArray().size());
    m_stored << tr("Resume points: %1").arg(keysIn(d + QStringLiteral("/resume.json")));
    m_stored << tr("Watched and keep/reject marks: %1 files").arg(keysIn(d + QStringLiteral("/marks.json")));
    m_stored << tr("Thumbnails: %1 (%2 pictures)").arg(loc.formattedDataSize(thumbs, 1), QString::number(nThumb));
    m_stored << tr("Music index: %1").arg(loc.formattedDataSize(QFileInfo(d + QStringLiteral("/music.json")).size(), 1));
    m_stored << tr("Logs: %1").arg(loc.formattedDataSize(sizeOf(Paths::logsDir(), {QStringLiteral("*")}, nullptr), 1));
    m_stored << tr("Everything in the data folder: %1").arg(loc.formattedDataSize(total, 1));
    update();
}

static QHash<QString, QString> readKeymapFile(const QString& file) {
    QHash<QString, QString> out;
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly)) return out;
    const QJsonObject keys = QJsonDocument::fromJson(f.readAll()).object().value("keys").toObject();
    for (auto it = keys.begin(); it != keys.end(); ++it) out.insert(QKeySequence(it.key().trimmed()).toString(QKeySequence::PortableText), it.value().toString());
    return out;
}

static QString keyStringOf(QKeyEvent* e) {           // the same spelling the main window looks keys up by
    const auto mods = e->modifiers();
    const QString t = e->text();
    QString ks;
    if (!(mods & (Qt::ControlModifier | Qt::AltModifier)) && t.size() == 1 && t[0].isPrint() && !t[0].isLetterOrNumber() && t[0] != ' ') ks = t;
    else ks = QKeySequence(e->keyCombination()).toString(QKeySequence::PortableText);
    return QKeySequence(ks).toString(QKeySequence::PortableText);
}

QJsonObject SettingsPage::keymapOverrides(const QHash<QString, QString>& cur, const QHash<QString, QString>& def) {
    QJsonObject o;
    for (auto it = cur.begin(); it != cur.end(); ++it) {
        if (it.value().isEmpty() && !def.contains(it.key())) continue;
        if (def.value(it.key()) != it.value()) o[it.key()] = it.value();
    }
    for (auto it = def.begin(); it != def.end(); ++it) if (!cur.contains(it.key()) || cur.value(it.key()).isEmpty()) o[it.key()] = QString();   // a default you removed
    return o;
}

static QString actionLabel(const QString& id) {
    for (const auto& a : kActions) if (id == QLatin1String(a.id)) return QObject::tr(a.label);
    return id;
}

void SettingsPage::rebuildKeyRows() {
    QHash<QString, QStringList> byAction;
    for (auto it = m_keys.begin(); it != m_keys.end(); ++it) if (!it.value().isEmpty()) byAction[it.value()] << it.key();
    QVector<RowList::Row> rows;
    QString lastGroup;
    int capRow = -1;
    for (const auto& a : kActions) {
        const QString group = tr(a.group);
        const QString id = QString::fromLatin1(a.id);
        if (group != lastGroup) { rows.push_back({Icon::File, group, {}, {}, {}, true}); lastGroup = group; }
        QStringList k = byAction.value(id); k.sort();
        QString hint;
        if (m_capture == id) { hint = tr("Press the new key…"); capRow = rows.size(); }
        else if (id == QLatin1String("seekpct1")) hint = QStringLiteral("1 … 9");               // one row for all nine keys
        else hint = k.isEmpty() ? tr("not set") : k.join("   ·   ").replace("Return", "Enter");
        rows.push_back({Icon::Check, tr(a.label), {}, hint, id.startsWith("seekpct") ? QString() : QStringLiteral("act:") + id, false});
    }
    rows.push_back({Icon::File, tr("Mouse"), {}, {}, {}, true});
    const std::pair<const char*, const char*> mouse[] = {
        {"Click the picture", "Play / pause"}, {"Double-click the picture", "Fullscreen"}, {"Mouse wheel", "Volume  (zoom for images)"},
        {"Drag a zoomed image", "Pan"}, {"Right-click the picture", "Menu"}, {"Hover the seek bar", "Frame preview"}, {"Drag the seek bar", "Scrub"}, {"Double-click a slider", "Reset it"}};
    for (const auto& m : mouse) rows.push_back({Icon::Check, tr(m.first), {}, tr(m.second), {}, false});
    m_keysList->setRows(rows, true);
    m_keysList->setCurrent(capRow);
    update();
}

void SettingsPage::startCapture(const QString& action) {
    m_capture = action; m_keyNote.clear();
    rebuildKeyRows();
    setFocus();
}

void SettingsPage::saveKeymap() {
    const QString f = Paths::dataDir() + QStringLiteral("/keymap.json");
    const QJsonObject o = keymapOverrides(m_keys, readKeymapFile(Paths::appDir() + QStringLiteral("/keymaps/default.json")));
    if (o.isEmpty()) { QFile::remove(f); return; }
    QJsonObject doc;
    doc["_info"] = QStringLiteral("Your changed shortcuts. Change them in kinoz under Settings > Shortcuts; this file only stores what differs from the defaults.");
    doc["keys"] = o;
    QFile out(f);
    if (out.open(QIODevice::WriteOnly | QIODevice::Truncate)) out.write(QJsonDocument(doc).toJson(QJsonDocument::Indented));
}

void SettingsPage::keyPressEvent(QKeyEvent* e) {
    if (m_capture.isEmpty()) { QWidget::keyPressEvent(e); return; }
    const int k = e->key();
    e->accept();
    if (k == Qt::Key_Escape) { m_capture.clear(); rebuildKeyRows(); return; }
    if (k == Qt::Key_unknown || k == Qt::Key_Control || k == Qt::Key_Shift || k == Qt::Key_Alt || k == Qt::Key_Meta || k == Qt::Key_AltGr || k == Qt::Key_CapsLock) return;   // wait for the real key
    const QString id = m_capture;
    QStringList own;
    for (auto it = m_keys.begin(); it != m_keys.end(); ++it) if (it.value() == id) own << it.key();
    if (k == Qt::Key_Delete && e->modifiers() == Qt::NoModifier) {                  // Delete clears this action's keys
        for (const QString& o : own) m_keys.remove(o);
        m_keyNote = tr("%1 has no key now").arg(actionLabel(id));
    } else {
        const QString ks = keyStringOf(e);
        if (ks.isEmpty()) return;
        QString shown = ks; shown.replace("Return", "Enter");
        const QString prev = m_keys.value(ks);
        for (const QString& o : own) m_keys.remove(o);                               // one key per action: the new one replaces the old
        m_keys.insert(ks, id);
        m_keyNote = (!prev.isEmpty() && prev != id) ? tr("%1 moved here from %2").arg(shown, actionLabel(prev)) : tr("%1 is now %2").arg(actionLabel(id), shown);
    }
    m_capture.clear();
    saveKeymap();
    emit keymapChanged(m_keys);
    rebuildKeyRows();
}

void SettingsPage::buildShortcuts() {
    m_keysList = new RowList(this);
    m_keysList->setRowHeight(36);
    m_keysList->setShowIcons(false);
    rebuildKeyRows();
    m_keysList->setCurrent(-1);
    connect(m_keysList, &RowList::activated, this, [this](int row) {
        const auto& rows = m_keysList->rows();
        if (row < 0 || row >= rows.size() || !rows[row].data.startsWith(QLatin1String("act:"))) return;
        const QString id = rows[row].data.mid(4);
        if (m_capture == id) { m_capture.clear(); rebuildKeyRows(); return; }          // a second click cancels
        startCapture(id);
    });
    m_resetKeys = new TextButton(tr("Reset all"), this);
    connect(m_resetKeys, &QAbstractButton::clicked, this, [this] {
        if (!m_resetArmed) {
            m_resetArmed = true; m_resetKeys->setText(tr("Click again to reset"));
            QTimer::singleShot(3500, this, [this] { m_resetArmed = false; m_resetKeys->setText(tr("Reset all")); });
            return;
        }
        m_resetArmed = false; m_resetKeys->setText(tr("Reset all"));
        m_keys = readKeymapFile(Paths::appDir() + QStringLiteral("/keymaps/default.json"));
        QFile::remove(Paths::dataDir() + QStringLiteral("/keymap.json"));
        m_capture.clear(); m_keyNote = tr("Back to the default shortcuts");
        emit keymapChanged(m_keys);
        rebuildKeyRows();
    });
    m_openKeymap = new TextButton(tr("Edit file (advanced)"), this);
    connect(m_openKeymap, &QAbstractButton::clicked, this, [this] {
        const QString f = Paths::dataDir() + QStringLiteral("/keymap.json");
        if (!QFile::exists(f)) saveKeymap();
        if (!QFile::exists(f)) { QFile out(f); if (out.open(QIODevice::WriteOnly)) out.write("{\n  \"keys\": {}\n}\n"); }
        QDesktopServices::openUrl(QUrl::fromLocalFile(f));
    });
    m_pageWidgets[Shortcuts] << m_keysList << m_resetKeys << m_openKeymap;
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
    // Download it yourself: always possible, whatever the update mode is.
    auto* page = new TextButton(tr("Download page"), this);
    connect(page, &QAbstractButton::clicked, this, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/p4inz-code/kinoz/releases/latest"))); });
    m_dlDirect = new TextButton(tr("Direct download"), this);
    connect(m_dlDirect, &QAbstractButton::clicked, this, [this] { if (m_up->directDownload().isValid()) QDesktopServices::openUrl(m_up->directDownload()); });
    m_updMode = new Segmented({tr("Automatic"), tr("Notify me"), tr("Off")}, this);
    m_updMode->setCurrent(m_up->mode());
    connect(m_updMode, &Segmented::currentChanged, this, [this](int i) { m_up->setMode(i); update(); });
    m_pageWidgets[About] << m_updBtn << src << lic << rel << page << m_dlDirect << m_updMode;

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
            "You can turn it off under About → Updates → Off. Details: <b>PRIVACY.md</b>.</p>"
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
    case Updater::Ready: m_updBtn->setText(tr("Restart now")); m_updStatus = m_up->info(); break;
    case Updater::Failed: m_updBtn->setText(tr("Try again")); m_updStatus = m_up->error(); break;
    case Updater::UpToDate: m_updBtn->setText(tr("Check again")); m_updStatus = m_up->info(); break;
    default: m_updBtn->setText(tr("Check for updates")); m_updStatus.clear(); break;
    }
    m_updBtn->resize(m_updBtn->sizeHint());
    if (m_dlDirect) m_dlDirect->setEnabled(m_up->directDownload().isValid());       // known once a check has found a newer version
    update();
}

void SettingsPage::showPage(int p) {
    m_page = p;
    for (int i = 0; i < 3; ++i) for (const Row& r : m_rows[i]) if (r.w) r.w->setVisible(p == i);
    for (int i = 0; i < PageCount; ++i) for (auto* w : m_pageWidgets[i]) w->setVisible(p == i);
    m_tabs->setCurrent(p);
    if (m_skipStepper) m_skipStepper->setVisible(p == Playback && m_skipSeg->current() == 3);
    if (p != Shortcuts && !m_capture.isEmpty()) { m_capture.clear(); rebuildKeyRows(); }
    if (p == Privacy) refreshPrivacy();
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
    for (int pg = 0; pg < 3; ++pg) {
        const int rh = pg == General ? 56 : pg == Playback ? 40 : 48;
        int y = 118;
        for (const Row& r : m_rows[pg]) {
            if (r.w) { const QSize s = r.w->sizeHint(); r.w->setGeometry(right - s.width(), y + (rh - s.height()) / 2, s.width(), s.height());
                if (r.w == m_skipSeg && m_skipStepper) { const QSize ss = m_skipStepper->sizeHint(); m_skipStepper->setGeometry(right - s.width() - 12 - ss.width(), y + (rh - ss.height()) / 2, ss.width(), ss.height()); } }
            y += rh;
        }
        if (pg == General) {
            int x = 24; y += 8;
            for (auto* w : m_pageWidgets[General]) { const QSize s = w->sizeHint(); w->setGeometry(x, y, s.width(), s.height()); x += s.width() + 10; }
        }
        if (pg == Privacy) {                                                           // three rows of buttons under the two switches
            const auto& pb = m_pageWidgets[Privacy];
            const int rowsY[3] = {252, 296, 340};
            for (int i = 0; i < pb.size(); ++i) {
                const int rowIdx = i / 3;
                int x = 24; for (int j = rowIdx * 3; j < i; ++j) x += pb[j]->sizeHint().width() + 10;
                const QSize s = pb[i]->sizeHint(); pb[i]->setGeometry(x, rowsY[qMin(rowIdx, 2)], s.width(), 38);
            }
        }
    }
    const QRect content(24, 116, width() - 48, height() - 116 - 68);
    if (m_keysList) m_keysList->setGeometry(content.left() - 12, content.top(), content.width() + 24, content.height());
    if (m_resetKeys) m_resetKeys->setGeometry(24, height() - 56, m_resetKeys->sizeHint().width(), 36);
    if (m_openKeymap && m_resetKeys) m_openKeymap->setGeometry(24 + m_resetKeys->sizeHint().width() + 10, height() - 56, m_openKeymap->sizeHint().width(), 36);
    if (m_infoView) m_infoView->setGeometry(content);
    if (m_copyDiag) m_copyDiag->setGeometry(24, height() - 56, m_copyDiag->sizeHint().width(), 36);
    if (m_legal) m_legal->setGeometry(24, 116, width() - 48, height() - 132);
    if (m_pageWidgets[About].size() == 7) {
        int x = 24; const int y = 500;
        m_updBtn->resize(m_updBtn->sizeHint()); m_updBtn->move(x, y); x += m_updBtn->width() + 10;
        for (int i = 1; i <= 2; ++i) { auto* w = m_pageWidgets[About][i]; const QSize s = w->sizeHint(); w->setGeometry(x, y, s.width(), s.height()); x += s.width() + 10; }
        x = 24;
        for (int i = 3; i <= 5; ++i) { auto* w = m_pageWidgets[About][i]; const QSize s = w->sizeHint(); w->setGeometry(x, 544, s.width(), s.height()); x += s.width() + 10; }
        m_updMode->setGeometry(width() - 28 - m_updMode->sizeHint().width(), 590, m_updMode->sizeHint().width(), 30);
    }
}

void SettingsPage::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const auto& c = pal();
    p.setPen(c.text); p.setFont(Theme::i().font(22, QFont::DemiBold, true));
    p.drawText(QRectF(24, 14, 300, 40), Qt::AlignVCenter, tr("Settings"));
    p.setPen(c.border); p.drawLine(24, 104, width() - 24, 104);

    if (m_page == General || m_page == Playback || m_page == Privacy) {
        const int rh = m_page == General ? 56 : m_page == Playback ? 40 : 48;
        int y = 118;
        for (const Row& r : m_rows[m_page]) {
            p.setPen(c.text); p.setFont(Theme::i().font(15, QFont::Medium)); p.drawText(QRectF(24, y + (rh == 56 ? 6 : rh == 40 ? 1 : 4), width() * 0.52, 22), Qt::AlignVCenter, r.title);
            p.setPen(c.muted); p.setFont(Theme::i().font(12)); p.drawText(QRectF(24, y + (rh == 56 ? 28 : rh == 40 ? 21 : 25), width() * 0.5, 18), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(r.desc, Qt::ElideRight, int(width() * 0.5)));
            y += rh;
        }
        if (m_page == Privacy) {
            p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::DemiBold)); p.drawText(QRectF(24, 222, width() - 48, 24), Qt::AlignVCenter, tr("Clean up now"));
            p.setPen(c.muted); p.setFont(Theme::i().font(12));
            p.drawText(QRectF(24, 386, width() - 48, 34), Qt::AlignTop | Qt::TextWordWrap, tr("Clean identity clears all of the above and forgets your last folder. Folders, settings, playlists and saved frames stay."));
            p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::DemiBold)); p.drawText(QRectF(24, 428, width() - 48, 24), Qt::AlignVCenter, tr("What kinoz stores on this computer"));
            int ty = 456; p.setFont(Theme::i().font(13));
            for (const QString& line : m_stored) { p.setPen(c.muted); p.drawText(QRectF(24, ty, width() - 48, 22), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(line, Qt::ElideRight, width() - 48)); ty += 22; }
        }
        if (m_page == General) {
            const auto sw = swatchRects();
            const int cur = int(m_set->num("accent", 0));
            for (int i = 0; i < sw.size(); ++i) {
                p.setPen(Qt::NoPen); p.setBrush(swatchColor(i)); p.drawEllipse(sw[i]);
                if (i == cur) { p.setBrush(Qt::NoBrush); p.setPen(QPen(c.text, 2)); p.drawEllipse(sw[i].adjusted(-4, -4, 4, 4)); }
            }
            p.setPen(c.muted); p.setFont(Theme::i().font(12));
            p.drawText(QRectF(24, y + 54, width() - 48, 22), Qt::AlignTop | Qt::AlignLeft, QFontMetrics(p.font()).elidedText(tr("Everything kinoz saves lives in its data folder next to the program, nothing on C:."), Qt::ElideRight, width() - 48));
        }
    } else if (m_page == Shortcuts) {
        p.setPen(c.muted); p.setFont(Theme::i().font(12));
        p.drawText(QRectF(width() - 24 - 300, height() - 56, 300, 36), Qt::AlignVCenter | Qt::AlignRight | Qt::TextWordWrap, !m_capture.isEmpty() ? tr("Press the new key. Esc cancels, Delete clears it.") : !m_keyNote.isEmpty() ? m_keyNote : tr("Click an action, then press the new key."));
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
        // download progress: a real bar while the update downloads in the background, full once it is verified
        const auto st = m_up->state();
        if (st == Updater::Downloading || st == Updater::Ready) {
            const qreal frac = st == Updater::Ready ? 1.0 : (m_up->total() > 0 ? qBound(0.0, double(m_up->received()) / double(m_up->total()), 1.0) : 0.0);
            const QRectF track(24, 458, width() - 48, 8);
            p.setPen(Qt::NoPen); p.setBrush(c.sunken); p.drawRoundedRect(track, 4, 4);
            p.setBrush(c.accent); p.drawRoundedRect(QRectF(track.left(), track.top(), qMax(8.0, track.width() * frac), track.height()), 4, 4);
        }
        if (!m_updStatus.isEmpty()) { p.setPen(st == Updater::Failed ? c.danger : c.muted); p.setFont(Theme::i().font(13)); p.drawText(QRectF(24, 472, width() - 48, 22), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(m_updStatus, Qt::ElideRight, width() - 48)); }
        p.setPen(c.text); p.setFont(Theme::i().font(14, QFont::DemiBold)); p.drawText(QRectF(24, 590, 120, 30), Qt::AlignVCenter, tr("Updates"));
        const QString desc = m_up->mode() == Updater::Automatic ? tr("Downloads quietly, then asks to restart. If you pick Later, it installs on the next launch.")
                           : m_up->mode() == Updater::Notify ? tr("Tells you when a new version is out. You choose when to download and install.")
                                                              : tr("Never checks by itself. “Check for updates” above still works.");
        p.setPen(c.muted); p.setFont(Theme::i().font(12)); p.drawText(QRectF(24, 622, width() - 48, 18), Qt::AlignVCenter, QFontMetrics(p.font()).elidedText(desc, Qt::ElideRight, width() - 48));
    }
}
