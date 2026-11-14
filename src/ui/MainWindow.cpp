// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#include "ui/MainWindow.h"
#ifdef Q_OS_WIN
#include <windows.h>
#include <psapi.h>
#endif
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QRandomGenerator>
#include "core/DefaultApp.h"
#include "core/Smtc.h"
#include "core/JsonStore.h"
#include "core/Paths.h"
#include "core/player/MpvPlayer.h"
#include "core/player/PlayerController.h"
#include "core/Updater.h"
#include "library/LibraryIndex.h"
#include "library/MusicIndex.h"
#include "library/Playlists.h"
#include "thumbs/PreviewService.h"
#include "thumbs/ThumbService.h"
#include "ui/Annotator.h"
#include "ui/ControlBar.h"
#include "ui/Overlays.h"
#include "ui/SettingsPage.h"
#include "core/Log.h"
#include "core/Doctor.h"
#include <QClipboard>
#include <QLocale>
#include <QSysInfo>
#include <QTextBrowser>
#include <QThread>
#include "ui/Sidebar.h"
#include "ui/Stages.h"
#include "ui/Theme.h"
#include "ui/HoverTip.h"
#include "ui/TopBar.h"
#include "ui/VideoWidget.h"
#include "ui/Widgets.h"

#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QPainter>
#include <QProcess>
#include <QScreen>
#include <QTextStream>
#include <QToolTip>
#include <QUrl>
#include <QWindow>
#include <functional>
#include <memory>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>

#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#endif  // Q_OS_WIN

namespace {
QString keyString(QKeyEvent* e) {
    const auto mods = e->modifiers();
    const QString t = e->text();
    if (!(mods & (Qt::ControlModifier | Qt::AltModifier)) && t.size() == 1 && t[0].isPrint() && !t[0].isLetterOrNumber() && t[0] != ' ')
        return t;
    return QKeySequence(e->keyCombination()).toString(QKeySequence::PortableText);
}
}

MainWindow::MainWindow(JsonStore* settings, JsonStore* resume) : QWidget(nullptr, Qt::Window | Qt::FramelessWindowHint), m_set(settings), m_resume(resume) {
    setWindowTitle(QStringLiteral(APP_NAME));
    setAcceptDrops(true);
    setMinimumSize(420, 260);
    setAttribute(Qt::WA_OpaquePaintEvent);
    Theme::i().setMode(Theme::Mode(int(m_set->num("theme", 0))));
    Theme::i().setCompact(m_set->num("compact", 0) > 0);
    Theme::i().setMotionMode(int(m_set->num("motion", 0)));
    Theme::i().setAccentIndex(int(m_set->num("accent", 0)));
    Theme::i().setGlass(m_set->flag("glass", true));
    applyAppPalette();
    connect(&Theme::i(), &Theme::changed, this, [this] { applyAppPalette(); update(); });

    m_mpv = new MpvPlayer(this);
    m_ctl = new PlayerController(m_mpv, m_set, m_resume, this);
    m_thumbs = new ThumbService(this);
    m_preview = new PreviewService(this);
    m_index = new LibraryIndex(this);
    m_updater = new Updater(m_set, this);
    connect(m_updater, &Updater::updateFound, this, [this](const QString& v) { notify(tr("kinoz %1 is available. Settings → About to update.").arg(v)); });
    // A verified update is ready: ask to restart now or later. "Later" is safe: the update installs by itself the next time kinoz starts.
    connect(m_updater, &Updater::readyToInstall, this, [this](const QString& v, bool automatic) {
        const bool watching = automatic && m_ctl->kind() == MediaKind::Video && !m_mpv->paused();      // never interrupt a movie for a background download
        if (watching) { notify(tr("kinoz %1 is ready. It installs the next time you open kinoz, or restart now from Settings → About.").arg(v)); return; }
        confirm(tr("kinoz %1 is ready").arg(v), tr("Restart now to finish updating. If you choose Later, kinoz installs the update by itself the next time you open it. Your settings and library are kept."),
                tr("Restart now"), [this] { m_updater->installAndQuit(); }, false, tr("Later"));
    });
    if (m_set->has("updateFailedVersion")) {                                                          // an update could not be applied twice in a row
        const QString v = m_set->str("updateFailedVersion");
        m_set->remove("updateFailedVersion");
        QTimer::singleShot(3000, this, [this, v] { notify(tr("kinoz couldn't install version %1 automatically. Open Settings → About and press Update to try again.").arg(v), true); });
    }
    if (!QCoreApplication::arguments().contains(QStringLiteral("--new")) && !qEnvironmentVariableIsSet("KINOZ_OFFSCREEN"))
        QTimer::singleShot(8000, this, [this] { m_updater->startBackground(); });
    {   // first start after an update: say so once
        const QString last = m_set->str("lastRunVersion");
        if (!last.isEmpty() && Updater::isNewer(Updater::current(), last)) {
            Log::info("app", QStringLiteral("updated from %1 to %2").arg(last, Updater::current()));
            QTimer::singleShot(3500, this, [this] { notify(tr("kinoz was updated to version %1. What's new is under Settings → About → Release notes.").arg(Updater::current())); });
        }
        m_set->set("lastRunVersion", Updater::current());
    }
    m_thumbs->trimDiskCache();
    buildUi();
    loadKeymap();
    wireController();

    m_sidebarOpen = false;                                  // the library sidebar is closed every time kinoz starts (Tab or the top-left button opens it)
    m_sidebarW = qBound(260, int(m_set->num("sidebarW", 320)), 480);
    m_sideT = m_sidebarOpen ? 1.0 : 0.0;
    m_sideAnim.setEasingCurve(Theme::easeMac());
    connect(&m_sideAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant& v) { m_sideT = v.toReal(); layoutAll(); });

    // geometry: restore, clamp onto a real screen
    const QJsonObject g = m_set->get("win").toObject();
    QScreen* scr = QGuiApplication::primaryScreen();
    QRect geo(0, 0, 1180, 720);
    geo.moveCenter(scr->availableGeometry().center());
    if (g.contains("w")) {
        const QRect saved(g["x"].toInt(), g["y"].toInt(), g["w"].toInt(), g["h"].toInt());
        for (QScreen* s : QGuiApplication::screens()) if (s->availableGeometry().contains(saved.center())) { geo = saved; scr = s; break; }
    }
    const QRect av = scr->availableGeometry();
    geo.setWidth(qMin(geo.width(), av.width())); geo.setHeight(qMin(geo.height(), av.height()));
    geo.moveLeft(qBound(av.left(), geo.left(), av.right() - geo.width())); geo.moveTop(qBound(av.top(), geo.top(), av.bottom() - geo.height()));
    if (QCoreApplication::arguments().contains(QStringLiteral("--new"))) geo.translate(56, 56);   // cascade extra windows
    if (qEnvironmentVariableIsSet("KINOZ_OFFSCREEN")) { geo = QRect(-4000, -4000, 1180, 720); setAttribute(Qt::WA_ShowWithoutActivating); }   // automated tests never touch the real screen
    setGeometry(geo);
    m_normalGeo = geo;
    m_wasMax = qEnvironmentVariableIsSet("KINOZ_OFFSCREEN") ? false : g["max"].toBool();

    m_idle.setSingleShot(true);
    applyHideDelay();
    m_thumbs->setPrivate(m_set->flag("privateMode", false));
    m_top->setPrivate(m_set->flag("privateMode", false));
    connect(&m_idle, &QTimer::timeout, this, &MainWindow::idleTick);
    m_slide.setSingleShot(true);
    connect(&m_slide, &QTimer::timeout, this, [this] { if (m_ctl->kind() == MediaKind::Image) m_ctl->next(); });
    connect(m_ctl, &PlayerController::pathChanged, this, &MainWindow::restartSlideshow);
    m_sleep.setSingleShot(true);
    connect(&m_sleep, &QTimer::timeout, this, [this] { m_ctl->pause(); notify(tr("Sleep timer: paused")); });
    m_stateTimer.setInterval(100);
    connect(&m_stateTimer, &QTimer::timeout, this, [this] { m_bar->setPosition(m_mpv->position(), m_mpv->duration()); });
    m_stateTimer.start();
    m_geoTimer.setSingleShot(true);
    m_geoTimer.setInterval(500);
    connect(&m_geoTimer, &QTimer::timeout, this, &MainWindow::saveGeometryNow);
    qApp->installEventFilter(this);

    m_mpv->initAsync(m_ctl->decodeMode() != 2);
    connect(m_mpv, &MpvPlayer::ready, this, [this] {
        m_mpv->setVolume(m_set->num("volume", 80));
        Log::info("engine", QStringLiteral("libmpv %1 ready").arg(m_mpv->getProp("mpv-version").toString()));
        if (m_set->flag("rememberMute", true) && m_set->flag("muted", false)) m_mpv->setMute(true);   // the last session ended muted
        m_muteRestored = true;
        refreshChrome();
        if (!m_pendingOpen.isEmpty()) { const auto p = m_pendingOpen; m_pendingOpen.clear(); m_ctl->open(p); }
    });
    connect(m_mpv, &MpvPlayer::initFailed, this, [this] { notify(tr("The playback engine failed to start. Check that libmpv-2.dll sits next to the app."), true); });
    connect(m_mpv, &MpvPlayer::logLine, this, [](const QString& s) {
        if (s.contains(QLatin1String("UDTA parsing failed"))) return;      // a harmless FFmpeg note about an odd MP4 metadata box, repeated for every such file
        qWarning().noquote() << "[mpv]" << s;
    });

    // library index for the command palette
    connect(m_side, &Sidebar::placesChanged, this, [this] { m_index->rebuild(m_side->places()); });
    QTimer::singleShot(1500, this, [this] { m_index->rebuild(m_side->places()); });
}

MainWindow::~MainWindow() {
    // libmpv aborts if its core is destroyed while a render context is alive, and Qt deletes children in creation
    // order (the player before the video widget). Free the video widget (and its render context) first.
    delete m_video;
    m_video = nullptr;
}

void MainWindow::notify(const QString& text, bool error) {
    if (error) qWarning().noquote() << "[error]" << text;
    m_toast->show(text, error ? 4200 : 2200, error);
}

void MainWindow::buildUi() {
    m_side = new Sidebar(m_ctl, m_thumbs, m_set, this);
    m_content = new QWidget(this);
    m_video = new VideoWidget(m_mpv, m_content);
    m_audio = new AudioStage(m_content);
    m_audio->hide();
    m_empty = new EmptyState(m_content);
    m_top = new TopBar(m_content);
    m_bar = new ControlBar(m_content);
    m_tip = new PreviewTip(m_content);
    m_panel = new SidePanel(m_ctl, m_set, m_content);
    m_annot = new Annotator(m_ctl, m_content);
    m_toast = new Toast(m_content);
    m_palette = new CommandPalette(m_ctl, m_index, this);
    m_cover = new FadeCover(this);          // veil for the open/close animation
#ifdef Q_OS_WIN
    for (auto sig : {&QGuiApplication::screenAdded, &QGuiApplication::screenRemoved})       // unplugging or adding a monitor while fullscreen
        connect(qApp, sig, this, [this] { QTimer::singleShot(400, this, [this] { fitFullscreenToMonitor(); }); });
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, [this] { QTimer::singleShot(400, this, [this] { fitFullscreenToMonitor(); }); });
#endif
    m_cover->setGeometry(rect());
    m_bar->hide();

    connect(m_video, &VideoWidget::glassReady, this, [this](bool ok) { m_glassOn = ok; updateGlass(); });
    connect(m_bar, &ControlBar::opacityChanged, this, &MainWindow::updateGlass);
    connect(m_top, &TopBar::opacityChanged, this, &MainWindow::updateGlass);

    connect(m_top, &TopBar::action, this, [this](const QString& id) {
        if (id == "min") showMinimized();
#ifdef Q_OS_MACOS
        else if (id == "max") doAction("fullscreen");                  // the green button means full screen on a Mac
#else
        else if (id == "max") { if (!m_full) isMaximized() ? showNormal() : showMaximized(); }
#endif
        else if (id == "close") close();
        else if (id == "pin") togglePin();
        else if (id == "sidebar") doAction("sidebar");
        else if (id == "settings") doAction("settings");
        else if (id == "more") { const QRect r = m_top->globalButtonRect("more"); showMoreMenu(QPoint(r.left(), r.bottom() + 6)); }
    });
    connect(m_bar, &ControlBar::action, this, [this](const QString& id) { doAction(id); });
    connect(m_bar, &ControlBar::scrubbed, this, [this](double f) { m_ctl->seekFrac(f, false); });
    connect(m_bar, &ControlBar::scrubFinished, this, [this](double f) { m_ctl->seekFrac(f, true); });
    connect(m_bar, &ControlBar::volumeDragged, this, [this](double v) { m_ctl->setVolume(v); });
    connect(m_bar, &ControlBar::hoverSeek, this, [this](double f, const QPoint& g) {
        const double dur = m_mpv->duration();
        if (dur <= 0 || m_ctl->kind() == MediaKind::Image) return;
        const double sec = f * dur;
        const int t = int(sec);
        const QString txt = t >= 3600 ? QStringLiteral("%1:%2:%3").arg(t / 3600).arg(t / 60 % 60, 2, 10, QLatin1Char('0')).arg(t % 60, 2, 10, QLatin1Char('0'))
                                      : QStringLiteral("%1:%2").arg(t / 60).arg(t % 60, 2, 10, QLatin1Char('0'));
        const QPoint local = m_content->mapFromGlobal(g);
        const QPoint anchor(local.x(), m_bar->y() + int(m_bar->panelRect().top()) - 8);
        m_previewPath = m_ctl->path();
        if (m_ctl->kind() == MediaKind::Video) { m_tip->showTimeOnly(txt, anchor); m_preview->request(m_previewPath, sec, 240, dur); }
        else m_tip->showTimeOnly(txt, anchor);
        m_tip->setProperty("anchor", anchor); m_tip->setProperty("txt", txt);
        updateGlass();
    });
    connect(m_bar, &ControlBar::hoverLeft, this, [this] { m_tip->hide(); m_preview->cancel(); updateGlass(); });
    connect(m_preview, &PreviewService::frame, this, [this](const QImage& img, double, const QString& path) {
        if (path != m_previewPath || !m_bar->underMouse()) return;
        m_tip->showFrame(img, m_tip->property("txt").toString(), m_tip->property("anchor").toPoint());
        updateGlass();
    });

    connect(m_video, &VideoWidget::clicked, this, [this](const QPoint&) {
        poke();
        if (m_annot->active()) return;
        if (Media::isPicture(m_ctl->kind()) && m_ctl->kind() != MediaKind::Gif) return;
        doAction("playpause");
    });
    connect(m_video, &VideoWidget::doubleClicked, this, [this] { toggleFullscreen(); });
    connect(m_video, &VideoWidget::moved, this, [this] { poke(); });
    connect(m_video, &VideoWidget::rightClicked, this, [this](const QPoint&) { showMoreMenu(); });
    connect(m_video, &VideoWidget::wheel, this, [this](int d, const QPointF&, Qt::KeyboardModifiers) {
        if (Media::isPicture(m_ctl->kind()) && m_ctl->kind() != MediaKind::Gif) m_ctl->zoomBy(d > 0 ? 1 : -1);
        else m_ctl->volumeStep(d > 0 ? 1 : -1);
    });
    connect(m_video, &VideoWidget::dragged, this, [this](const QPoint& d) { m_ctl->panByPixels(d, m_video->size(), m_video->devicePixelRatioF()); });
    connect(m_empty, &EmptyState::recentClicked, this, [this](const QString& p) { m_ctl->play(p); });
    connect(m_empty, &EmptyState::openFolderRequested, this, [this] { openPicker(true); });
    connect(m_empty, &EmptyState::defaultPlayerRequested, this, [this] { doAction("defaultplayer"); });

    connect(m_side, &Sidebar::addFolderRequested, this, [this] { openPicker(true); });
    connect(m_side, &Sidebar::toast, this, [this](const QString& t) { notify(t); });
    connect(m_side, &Sidebar::savePlaylistRequested, this, [this] { savePlaylist(); });
    connect(m_side, &Sidebar::renamePlaylistRequested, this, [this](const QString& n) { renamePlaylist(n); });
    connect(m_side, &Sidebar::deletePlaylistRequested, this, [this](const QString& n) { deletePlaylist(n); });
    connect(m_side, &Sidebar::openInNewWindow, this, [this](const QString& p) {
        if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--new"), p})) notify(tr("Couldn't open a new window"), true);
    });
    connect(m_panel, &SidePanel::toast, this, [this](const QString& t) { notify(t); });
    connect(m_panel, &SidePanel::closedByUser, this, [this] { poke(); });

    connect(m_palette, &CommandPalette::command, this, [this](const QString& id) { doAction(id); });
    connect(m_palette, &CommandPalette::playFile, this, [this](const QString& p) { m_ctl->play(p); });

    connect(m_annot, &Annotator::message, this, [this](const QString& t, bool e) { notify(t, e); });
    connect(m_annot, &Annotator::finished, this, [this] { poke(); m_video->setFocus(); });
    connect(m_ctl, &PlayerController::error, this, [this](const QString& t) { notify(t, true); });

    m_top->raise(); m_bar->raise();
}

void MainWindow::wireController() {
    connect(m_ctl, &PlayerController::pathChanged, this, &MainWindow::refreshChrome);
    connect(m_ctl, &PlayerController::modeChanged, this, &MainWindow::refreshChrome);
    connect(m_ctl, &PlayerController::viewChanged, this, &MainWindow::refreshChrome);
    connect(m_ctl, &PlayerController::message, this, [this](const QString& t) { notify(t); });
    connect(m_mpv, &MpvPlayer::pauseChanged, this, [this](bool) { refreshChrome(); poke(); });
    connect(m_mpv, &MpvPlayer::volumeChanged, this, [this](double) { refreshChrome(); });
    connect(m_mpv, &MpvPlayer::muteChanged, this, [this](bool on) {
        // Remember the mute state for the next start. Not before the saved state has been applied (the engine
        // reports "not muted" once at startup), and Clean identity clears it again.
        if (m_muteRestored && m_set->flag("rememberMute", true)) { if (on) m_set->set("muted", true); else m_set->remove("muted"); }
        refreshChrome();
    });
    connect(m_mpv, &MpvPlayer::speedChanged, this, [this](double) { refreshChrome(); });
    connect(m_mpv, &MpvPlayer::metadataChanged, this, &MainWindow::refreshChrome);
    connect(m_mpv, &MpvPlayer::tracksChanged, this, &MainWindow::refreshChrome);
    connect(m_mpv, &MpvPlayer::durationChanged, this, [this](double) { refreshChrome(); });
    connect(m_mpv, &MpvPlayer::videoSizeChanged, this, [this] { refreshChrome(); });
}

void MainWindow::loadKeymap() {
    auto load = [this](const QString& file) {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly)) return;
        QJsonParseError pe;
        const auto doc = QJsonDocument::fromJson(f.readAll(), &pe);
        if (pe.error != QJsonParseError::NoError) { qWarning() << "keymap parse error in" << file << pe.errorString(); return; }
        const QJsonObject keys = doc.object().value("keys").toObject();
        for (auto it = keys.begin(); it != keys.end(); ++it) m_keys.insert(QKeySequence(it.key().trimmed()).toString(QKeySequence::PortableText), it.value().toString());
    };
    load(Paths::appDir() + "/keymaps/default.json");
    load(Paths::dataDir() + "/keymap.json");   // user overrides
}

// ---------------- chrome state ----------------
void MainWindow::refreshChrome() {
    const MediaKind k = m_ctl->kind();
    const bool has = m_ctl->hasMedia();
    if (m_sideAuto && (!(k == MediaKind::Video || k == MediaKind::Gif) || m_mpv->paused())) { m_sideAuto = false; setSidebarOpen(true, true, true); }   // paused, stopped or switched to music/pictures: the library returns
    m_empty->setVisible(!has);
    if (!has) m_empty->setRecent(m_ctl->recent());
    const bool audioCard = has && k == MediaKind::Audio;
    m_audio->setVisible(audioCard);
    if (audioCard) m_audio->setInfo(m_ctl->title(), m_ctl->artist(), m_ctl->album(), m_mpv->hasCover());
    m_top->setOverVideo(has && !(k == MediaKind::Audio && !m_mpv->hasCover()));
    m_top->setTitle(has ? m_ctl->title() : QString());
    QString sub = m_ctl->artist();
    if (sub.isEmpty() && has) sub = QFileInfo(m_ctl->path()).dir().dirName();
    m_top->setSubtitle(sub);
    m_top->setMaximized(isMaximized());
    m_top->setPinned(m_pinned);
    ControlBar::State s;
    s.kind = k;
    s.pos = m_mpv->position(); s.dur = m_mpv->duration();
    s.volume = m_mpv->volume(); s.muted = m_mpv->muted(); s.speed = m_mpv->speed(); s.skipStep = qBound(1, int(m_set->num("skipStep", 10)), 600);
    s.playing = !m_mpv->paused(); s.shuffle = m_ctl->shuffle(); s.repeat = m_ctl->repeat();
    m_bar->setCompact(m_mini);
    m_bar->setState(s);
    if (!has) { m_bar->reveal(false); m_top->reveal(true); }
    else if (!m_bar->isRevealed() && !m_annot->active()) poke();
    setWindowTitle(has ? m_ctl->title() + QStringLiteral(" — ") + QStringLiteral(APP_NAME) : QStringLiteral(APP_NAME));
    updateGlass();
}

void MainWindow::updateGlass() {
    if (!m_video || !m_glassOn) return;
    const MediaKind k = m_ctl->kind();
    const bool over = Theme::i().glass() && m_ctl->hasMedia() && !(k == MediaKind::Audio && !m_mpv->hasCover()) && !m_annot->active();
    m_bar->setGlass(over); m_top->setGlass(over && m_top->overVideo());
    QVector<VideoWidget::Glass> g;
    if (over) {
        if (m_bar->isVisible() && m_bar->opacity() > 0.01) { VideoWidget::Glass x; x.rect = m_bar->panelRect().translated(m_bar->pos()); x.radius = 16; x.opacity = m_bar->opacity(); g << x; }
        if (m_top->isVisible() && m_top->opacity() > 0.01 && m_top->overVideo()) { VideoWidget::Glass x; x.rect = QRectF(m_top->geometry()); x.radius = 0; x.opacity = m_top->opacity() * 0.9; x.tintStrength = 0.22; x.metal = 0.6; g << x; }
        if (m_tip->isVisible()) { VideoWidget::Glass x; x.rect = QRectF(m_tip->geometry()); x.radius = 10; x.opacity = 1; x.tintStrength = 0.38; g << x; }
    }
    m_video->setGlass(g);
}

void MainWindow::poke() {
    if (!m_ctl || m_annot->active()) return;
    m_top->reveal(true);
    if (m_ctl->hasMedia()) m_bar->reveal(true);
    m_video->unsetCursor();
    m_idle.start();
}

bool MainWindow::pointerOverChrome() const {
    const QPoint g = QCursor::pos();
    return (m_bar->isVisible() && m_bar->rect().contains(m_bar->mapFromGlobal(g))) || (m_panel->isOpen() && m_panel->rect().contains(m_panel->mapFromGlobal(g)));
}

void MainWindow::idleTick() {
    if (!m_ctl->hasMedia() || m_annot->active()) return;
    if (int(m_set->num("hideDelay", 2)) == 0) { m_idle.start(); return; }       // "Never hide"
    const MediaKind k = m_ctl->kind();
    const bool keep = k == MediaKind::Audio || m_mpv->paused() || m_bar->busy() || m_top->busy() || pointerOverChrome() || m_palette->isOpen() || m_panel->isOpen();
    if (keep) { m_idle.start(); return; }
    if (k == MediaKind::Video || k == MediaKind::Gif || k == MediaKind::Image) {
        m_bar->reveal(false);
        m_top->reveal(false);
        m_tip->hide();
        HoverTip::cancel();
        m_video->setCursor(Qt::BlankCursor);
    }
    // Like the Windows 11 player: after the pause, a playing video gets the whole window. The library slides away too,
    // unless the pointer is on it, and returns when the pointer reaches the left edge.
    if (!m_selfTest && !m_shots && (k == MediaKind::Video || k == MediaKind::Gif) && m_set->flag("autoHideLibrary", true) && m_sidebarOpen && !m_mini && !m_full
        && !(m_side->isVisible() && m_side->rect().contains(m_side->mapFromGlobal(QCursor::pos())))
        && !m_side->isAncestorOf(focusWidget()) && !QApplication::activePopupWidget()) {      // not while typing in the library or with a menu open
        m_sideAuto = true;
        setSidebarOpen(false, true, true);
    }
}

bool MainWindow::eventFilter(QObject* o, QEvent* e) {
#ifndef Q_OS_WIN
    // Frameless window on macOS/Linux: let the window system do the moving and resizing.
    if (e->type() == QEvent::MouseButtonPress && isActiveWindow() && windowHandle()) {
        auto* me = static_cast<QMouseEvent*>(e);
        auto* w = qobject_cast<QWidget*>(o);
        if (me->button() == Qt::LeftButton && w && (w == this || isAncestorOf(w)) && !m_full) {
            const QPoint lp = mapFromGlobal(me->globalPosition().toPoint());
            Qt::Edges ed;
            if (!isMaximized() && !m_mini) { if (lp.x() < 6) ed |= Qt::LeftEdge; if (lp.x() >= width() - 6) ed |= Qt::RightEdge; if (lp.y() < 6) ed |= Qt::TopEdge; if (lp.y() >= height() - 6) ed |= Qt::BottomEdge; }
            if (ed) { windowHandle()->startSystemResize(ed); return true; }
            const QPoint inContent = lp - m_content->pos();
            if ((m_top->isVisible() && QRect(0, 0, m_top->width(), m_top->height()).contains(inContent) && m_top->isCaption(inContent)) || captionInSidebar(lp)) { windowHandle()->startSystemMove(); return true; }
        }
    }
#endif
    if (e->type() == QEvent::MouseMove && isActiveWindow()) {
        auto* w = qobject_cast<QWidget*>(o);
        if (w && (w == this || isAncestorOf(w))) {
            const QPoint g = static_cast<QMouseEvent*>(e)->globalPosition().toPoint();
            if ((g - m_lastMouse).manhattanLength() > 2) { m_lastMouse = g; poke(); }
            if (m_sideAuto && !m_sidebarOpen) {                                   // pointer at the left edge: bring the library back
                const QPoint lp = mapFromGlobal(g);
                if (lp.x() >= 0 && lp.x() <= 6) { m_sideAuto = false; setSidebarOpen(true, true, true); }
            }
        }
    }
    return QWidget::eventFilter(o, e);
}

// ---------------- layout ----------------
void MainWindow::layoutAll() {
    const bool noSide = m_full || m_mini;
    const int sw = noSide ? 0 : int(m_sidebarW * m_sideT);
    m_side->setVisible(sw > 0);
    m_side->setGeometry(sw - m_sidebarW, 0, m_sidebarW, height());
    m_content->setGeometry(sw, 0, width() - sw, height());
    const QRect r = m_content->rect();
    m_video->setGeometry(r);
    m_audio->setGeometry(r);
    m_empty->setGeometry(r);
    m_top->setGeometry(0, 0, r.width(), m_top->height());
    const int bw = qMin(r.width() - 32, m_mini ? 420 : 920);
    const int bh = m_bar->preferredHeight();
    m_bar->setGeometry((r.width() - bw) / 2, r.height() - bh - 12, bw, bh);
    if (m_panel->isOpen()) { m_panel->setFixedHeight(r.height()); m_panel->move(r.width() - SidePanel::kWidth, 0); }
    if (m_annot->active()) m_annot->setGeometry(r);
    m_top->raise(); m_bar->raise(); m_tip->raise(); m_toast->raise();
    if (m_panel->isOpen()) m_panel->raise();
    if (m_annot->active()) m_annot->raise();
    updateGlass();
}

void MainWindow::resizeEvent(QResizeEvent*) {
    layoutAll();
    if (!isMaximized() && !m_full && !m_mini) m_geoTimer.start();
    m_top->setMaximized(isMaximized());
    if (m_palette->isOpen()) m_palette->setGeometry(rect());
}

void MainWindow::paintEvent(QPaintEvent*) { QPainter(this).fillRect(rect(), pal().bg); }

void MainWindow::changeEvent(QEvent* e) {
    if (e->type() == QEvent::WindowStateChange) {
        m_top->setMaximized(isMaximized()); layoutAll();
        // Multitasking: a minimized video keeps playing its audio but stops decoding/rendering the picture.
        if (m_mpv && m_mpv->isReady() && m_ctl->kind() == MediaKind::Video) {
            if (isMinimized() && !m_vidOff) { m_vidOff = true; m_mpv->setProp("vid", QStringLiteral("no")); }
            else if (!isMinimized() && m_vidOff) { m_vidOff = false; m_mpv->setProp("vid", QStringLiteral("auto")); }
        }
    }
    QWidget::changeEvent(e);
}

void MainWindow::saveGeometryNow() {
    if (m_full || m_mini) return;
    QJsonObject o;
    if (!isMaximized()) m_normalGeo = geometry();
    const QRect g = m_normalGeo;
    o["x"] = g.x(); o["y"] = g.y(); o["w"] = g.width(); o["h"] = g.height(); o["max"] = isMaximized();
    m_set->set("win", o);
}

void MainWindow::closeEvent(QCloseEvent* e) {
    if (!m_closing && Theme::i().animMs(1) > 0 && isVisible() && !isMinimized()) {
        m_closing = true;                    // soft fade-out first, then really close
        e->ignore();
        m_cover->run(0.0, 1.0, 140, [this] { close(); });
        return;
    }
    if (m_full) exitFullscreen();
#ifdef Q_OS_WIN
#ifdef Q_OS_WIN
    { HWND h = reinterpret_cast<HWND>(winId()); for (int id = 101; id <= 104; ++id) UnregisterHotKey(h, id); }
#endif
#endif
    saveGeometryNow();
    m_ctl->saveResumeNow();
    if (m_set->flag("forgetOnExit", false)) cleanIdentity(false);               // "clean identity when kinoz closes"
    m_set->flush();
    m_resume->flush();
    hide();                      // vanish instantly; teardown happens behind the scenes
    e->accept();
    qApp->quit();
}

// ---------------- frameless window ----------------
void MainWindow::applyDwmFrame(bool on) {
#ifdef Q_OS_WIN
    HWND h = reinterpret_cast<HWND>(winId());
    const MARGINS m = on ? MARGINS{1, 1, 1, 1} : MARGINS{0, 0, 0, 0};
    DwmExtendFrameIntoClientArea(h, &m);
#else
    Q_UNUSED(on)
#endif
}

void MainWindow::setTopmost(bool on) {
#ifdef Q_OS_WIN
    SetWindowPos(reinterpret_cast<HWND>(winId()), on ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
#else
    if (windowHandle()) windowHandle()->setFlag(Qt::WindowStaysOnTopHint, on);
#endif
}

void MainWindow::showEvent(QShowEvent* e) {
    QWidget::showEvent(e);
    static bool done = false;
    if (done) return;
    done = true;
#ifdef Q_OS_WIN
    HWND h = reinterpret_cast<HWND>(winId());
    LONG_PTR st = GetWindowLongPtr(h, GWL_STYLE);
    SetWindowLongPtr(h, GWL_STYLE, st | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CAPTION);
    applyDwmFrame(true);
    // keyboard media keys (work even when another app has focus)
    RegisterHotKey(h, 101, 0, VK_MEDIA_PLAY_PAUSE); RegisterHotKey(h, 102, 0, VK_MEDIA_NEXT_TRACK);
    RegisterHotKey(h, 103, 0, VK_MEDIA_PREV_TRACK); RegisterHotKey(h, 104, 0, VK_MEDIA_STOP);
    const int round = 2;
    DwmSetWindowAttribute(h, DWMWA_WINDOW_CORNER_PREFERENCE, &round, sizeof(round));
    SetWindowPos(h, nullptr, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
#endif
    setGeometry(m_normalGeo);                                  // frame change can shift the window: re-apply
    if (m_wasMax) QTimer::singleShot(0, this, [this] { showMaximized(); });
    QTimer::singleShot(0, this, [this] { m_cover->run(1.0, 0.0, 300); });   // gentle fade-in on launch
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(KINOZ_HAS_DBUS)
    if (!qEnvironmentVariableIsSet("KINOZ_SILENT")) {                       // tests stay invisible to the system media overlay
        m_smtc = new Smtc(this, this);
        if (m_smtc->ok()) {
            connect(m_smtc, &Smtc::playRequested, this, [this] { if (m_mpv->paused()) m_ctl->togglePause(); });
            connect(m_smtc, &Smtc::pauseRequested, this, [this] { if (!m_mpv->paused()) m_ctl->togglePause(); });
            connect(m_smtc, &Smtc::nextRequested, this, [this] { doAction("next"); });
            connect(m_smtc, &Smtc::previousRequested, this, [this] { doAction("prev"); });
            connect(m_smtc, &Smtc::stopRequested, this, [this] { doAction("stop"); });
            connect(m_smtc, &Smtc::seekRequested, this, [this](double s) { m_mpv->seek(s, true); });
            connect(m_ctl, &PlayerController::pathChanged, this, &MainWindow::updateSmtc);
            connect(m_mpv, &MpvPlayer::pauseChanged, this, [this](bool) { updateSmtc(); });
            m_smtcTimer.setInterval(1500);                                 // keeps the overlay's progress bar in step
            connect(&m_smtcTimer, &QTimer::timeout, this, &MainWindow::updateSmtc);
            m_smtcTimer.start();
        }
    }
#endif
}

void MainWindow::updateSmtc() {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS) || defined(KINOZ_HAS_DBUS)
    if (!m_smtc || !m_smtc->ok()) return;
    Smtc::State s;
    s.hasMedia = m_ctl->hasMedia() && Media::family(m_ctl->kind()) == 1;      // pictures don't belong in the media overlay
    s.video = m_ctl->kind() == MediaKind::Video;
    s.title = m_ctl->title(); s.artist = m_ctl->artist(); s.album = m_ctl->album();
    s.playing = !m_mpv->paused();
    s.pos = m_mpv->position(); s.dur = m_mpv->duration();
    m_smtc->update(s);
#endif
}

#ifndef Q_OS_WIN
bool MainWindow::nativeEvent(const QByteArray&, void*, qintptr*) { return false; }
#else
bool MainWindow::nativeEvent(const QByteArray&, void* msg, qintptr* result) {
    MSG* m = static_cast<MSG*>(msg);
    HWND h = m->hwnd;
    switch (m->message) {
    case WM_HOTKEY: {
        const int id = int(m->wParam);
        doAction(id == 101 ? "playpause" : id == 102 ? "next" : id == 103 ? "prev" : "stop");
        *result = 0; return true; }
    case WM_NCCALCSIZE:
        if (m->wParam) {
            auto* p = reinterpret_cast<NCCALCSIZE_PARAMS*>(m->lParam);
            if (IsZoomed(h) && !m_full) {
                MONITORINFO mi{sizeof(mi)};
                if (GetMonitorInfo(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi)) p->rgrc[0] = mi.rcWork;
            }
            *result = 0;
            return true;
        }
        return false;
    case WM_NCHITTEST: {
        POINT pt{GET_X_LPARAM(m->lParam), GET_Y_LPARAM(m->lParam)};
        ScreenToClient(h, &pt);
        const qreal dpr = devicePixelRatioF();
        RECT rc; GetClientRect(h, &rc);
        const int b = int(6 * dpr);
        if (!IsZoomed(h) && !m_full && !m_mini) {
            const bool L = pt.x < b, R = pt.x >= rc.right - b, T = pt.y < b, B = pt.y >= rc.bottom - b;
            if (T && L) { *result = HTTOPLEFT; return true; }
            if (T && R) { *result = HTTOPRIGHT; return true; }
            if (B && L) { *result = HTBOTTOMLEFT; return true; }
            if (B && R) { *result = HTBOTTOMRIGHT; return true; }
            if (L) { *result = HTLEFT; return true; }
            if (R) { *result = HTRIGHT; return true; }
            if (T) { *result = HTTOP; return true; }
            if (B) { *result = HTBOTTOM; return true; }
        }
        const QPoint lp(int(pt.x / dpr), int(pt.y / dpr));
        if (!m_full) {
            const QPoint inContent = lp - m_content->pos();
            if (m_top->isVisible() && QRect(0, 0, m_top->width(), m_top->height()).contains(inContent) && m_top->isCaption(inContent)) { *result = HTCAPTION; return true; }
            if (captionInSidebar(lp)) { *result = HTCAPTION; return true; }
            if (m_mini && m_content->rect().contains(inContent) && !m_bar->geometry().contains(inContent)) { /* video area: let clicks through */ }
        }
        *result = HTCLIENT;
        return true;
    }
    }
    return false;
}
#endif  // Q_OS_WIN

bool MainWindow::captionInSidebar(const QPoint& p) const { return m_side->isVisible() && m_side->geometry().contains(p) && m_side->isCaptionAt(p - m_side->pos()); }

// ---------------- input ----------------
void MainWindow::keyPressEvent(QKeyEvent* e) {
    switch (e->key()) {                                  // media keys while focused (Windows also registers them globally)
    case Qt::Key_MediaPlay: case Qt::Key_MediaTogglePlayPause: doAction("playpause"); e->accept(); return;
    case Qt::Key_MediaNext: doAction("next"); e->accept(); return;
    case Qt::Key_MediaPrevious: doAction("prev"); e->accept(); return;
    case Qt::Key_MediaStop: doAction("stop"); e->accept(); return;
    default: break;
    }
    const QString ks = keyString(e);
    const QString act = m_keys.value(ks);
    if (act.isEmpty()) { QWidget::keyPressEvent(e); return; }
    static const QSet<QString> repeatable{"seekback", "seekfwd", "seekback30", "seekfwd30", "seekback10", "seekfwd10", "skipback", "skipfwd", "volup", "voldown", "framestep", "framestepback", "zoomin", "zoomout", "next", "prev", "nextfile", "prevfile"};
    if (e->isAutoRepeat() && !repeatable.contains(act)) return;
    doAction(act);
    e->accept();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* e) { if (e->mimeData()->hasUrls()) e->acceptProposedAction(); }
void MainWindow::dropEvent(QDropEvent* e) {
    QStringList paths;
    for (const QUrl& u : e->mimeData()->urls()) if (u.isLocalFile()) paths << u.toLocalFile();
    if (paths.isEmpty()) { notify(tr("Only local files and folders can be dropped here"), true); return; }
    openPaths(paths);
}

void MainWindow::openPaths(const QStringList& paths) {
    QStringList real;
    for (const QString& p : paths) {
        if (p.startsWith(QStringLiteral("--action="))) { doAction(p.mid(9)); continue; }
        real << p;
    }
    if (!real.isEmpty()) {
        if (m_mpv->isReady()) m_ctl->open(real); else m_pendingOpen = real;
    }
    if (isMinimized()) showNormal();
    raise(); activateWindow();
}

// ---------------- actions ----------------
void MainWindow::doAction(const QString& id) {
    auto seek = [&](double s) { m_ctl->seekBy(s); poke(); };
    if (id == "playpause" || id == "play") m_ctl->togglePause();
    else if (id == "stop") m_ctl->stop();
    else if (id == "next") m_ctl->next();
    else if (id == "prev") m_ctl->prev();
    else if (id == "nextfile") m_ctl->next();
    else if (id == "prevfile") m_ctl->prevFile();
    else if (id == "seekfwd") seek(m_set->num("seekSmall", 5));
    else if (id == "seekback") seek(-m_set->num("seekSmall", 5));
    else if (id == "seekfwd30") seek(m_set->num("seekLarge", 30));
    else if (id == "seekback30") seek(-m_set->num("seekLarge", 30));
    else if (id == "abloop") {
        m_mpv->command({QStringLiteral("ab-loop")});
        QTimer::singleShot(150, this, [this] {
            const bool a = m_mpv->getProp("ab-loop-a").toString() != "no", b = m_mpv->getProp("ab-loop-b").toString() != "no";
            notify(!a ? tr("A-B loop cleared") : !b ? tr("Loop start (A) set — press again for the end (B)") : tr("Looping between A and B — press again to clear"));
        });
    }
    else if (id == "sleep") showSleepMenu();
    else if (id == "aspect") showAspectMenu();
    else if (id == "reveal") { if (m_ctl->hasMedia()) {
#ifdef Q_OS_WIN
        QProcess::startDetached(QStringLiteral("explorer.exe"), {QStringLiteral("/select,"), QDir::toNativeSeparators(m_ctl->path())});
#else
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_ctl->path()).absolutePath()));
#endif
    } }
    else if (id == "copypath") { if (m_ctl->hasMedia()) { QGuiApplication::clipboard()->setText(QDir::toNativeSeparators(m_ctl->path())); notify(tr("Path copied")); } }
    else if (id == "seekfwd10") seek(10);
    else if (id == "seekback10") seek(-10);
    else if (id == "skipfwd") seek(qBound(1, int(m_set->num("skipStep", 10)), 600));
    else if (id == "skipback") seek(-qBound(1, int(m_set->num("skipStep", 10)), 600));
    else if (id == "volup" || id == "voldown") { if (!m_ctl->hasMedia() || (m_ctl->kind() != MediaKind::Image && m_ctl->kind() != MediaKind::Gif)) m_ctl->volumeStep(id == "volup" ? 1 : -1); }   // pictures have no sound
    else if (id == "healthcheck") {
        const QString rep = Doctor::textReport(Doctor::runCore(Paths::dataDir(), true, false, true));
        QGuiApplication::clipboard()->setText(QStringLiteral("kinoz ") + QStringLiteral(APP_VERSION) + QStringLiteral(" health check\n\n") + rep);
        int bad = 0; for (const QString& l : rep.split('\n')) bad += l.startsWith(QLatin1String("WARN")) || l.startsWith(QLatin1String("FAIL"));
        notify(bad ? tr("Health check copied. %1 thing(s) need a look; the report says which.").arg(bad) : tr("Health check copied: everything looks fine"));
    }
    else if (id == "mute" ) { if (!m_ctl->hasMedia() || m_ctl->kind() == MediaKind::Video || m_ctl->kind() == MediaKind::Audio) m_ctl->toggleMute(); }   // pictures have no sound
    else if (id == "fullscreen") toggleFullscreen();
    else if (id == "escape") {
        if (m_palette->isOpen()) m_palette->closePalette();
        else if (m_annot->active()) m_annot->end();
        else if (m_panel->isOpen()) closePanel();
        else if (m_full) exitFullscreen();
        else if (m_mini) toggleMini();
    }
    else if (id == "shuffle") m_ctl->setShuffle(!m_ctl->shuffle());
    else if (id == "repeat") m_ctl->cycleRepeat();
    else if (id == "speedup") m_ctl->speedStep(1);
    else if (id == "speeddown") m_ctl->speedStep(-1);
    else if (id == "speedreset") m_ctl->setSpeed(1.0);
    else if (id == "framestep") { m_ctl->pause(); m_mpv->frameStep(false); }
    else if (id == "framestepback") { m_ctl->pause(); m_mpv->frameStep(true); }
    else if (id == "mini") toggleMini();
    else if (id == "pin") togglePin();
    else if (id == "theme") cycleTheme();
    else if (id == "rotate") m_ctl->rotate(90);
    else if (id == "zoomin") m_ctl->zoomBy(1);
    else if (id == "zoomout") m_ctl->zoomBy(-1);
    else if (id == "zoom" || id == "zoomreset") {
        const MediaKind k = m_ctl->kind();
        if (id == "zoomreset" && (k == MediaKind::Video || k == MediaKind::Audio)) { m_ctl->seekFrac(0.0, true); poke(); }     // 0 = back to the start
        else m_ctl->resetView();
    }
    else if (id == "screenshot") screenshotFrame();
    else if (id == "more") showMoreMenu();
    else if (id == "speed") showSpeedMenu();
    else if (id == "tracks") showTracksMenu();
    else if (id == "subdelayplus") m_ctl->setSubDelay(m_mpv->getProp("sub-delay").toDouble() + 0.1);
    else if (id == "subdelayminus") m_ctl->setSubDelay(m_mpv->getProp("sub-delay").toDouble() - 0.1);
    else if (id == "subcycle") m_mpv->command({"cycle", "sub"});
    else if (id == "quit") close();
    else if (id == "sidebar") setSidebarOpen(!m_sidebarOpen);
    else if (id == "adjust" || id == "eq") { const MediaKind k = m_ctl->kind(); if (!m_ctl->hasMedia() || (id == "eq" ? (k == MediaKind::Video || k == MediaKind::Audio) : (k == MediaKind::Video || k == MediaKind::Gif))) togglePanel(id); }
    else if (id == "palette") { if (m_palette->isOpen()) m_palette->closePalette(); else m_palette->openPalette(); }
    else if (id == "openfolder") openPicker(true);
    else if (id == "openfile") openPicker(false);
    else if (id == "settings") openSettings();
    else if (id == "saveplaylist") savePlaylist();
    else if (id == "playlists") { setSidebarOpen(true); m_side->showPlaylists(); }
    else if (id == "music") { setSidebarOpen(true); m_side->showTab(2); }
    else if (id == "keep") { if (m_ctl->hasMedia()) m_ctl->toggleMark(m_ctl->path(), PlayerController::MarkKeep); }
    else if (id == "reject") { if (m_ctl->hasMedia()) m_ctl->toggleMark(m_ctl->path(), PlayerController::MarkReject); }
    else if (id == "trash") trashCurrent();
    else if (id == "trashrejected") trashRejected();
    else if (id == "nextunwatched") { const int i = m_ctl->nextUnwatched(); if (i >= 0) m_ctl->playIndex(i); else notify(tr("You've seen everything in this list")); }
    else if (id.startsWith(QStringLiteral("seekpct"))) {                       // 1..9 jump to 10%..90% of a video or track
        const MediaKind k = m_ctl->kind();
        if (k == MediaKind::Video || k == MediaKind::Audio) { m_ctl->seekFrac(id.mid(7).toInt() / 10.0, true); poke(); }
    }
    else if (id == "defaultplayer") { const auto r = DefaultApp::apply(); notify(r.message, !r.ok); }
    else if (id == "uninstall") { if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--uninstall")})) notify(tr("Couldn't open the uninstaller"), true); }
    else if (id == "doctor") { if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--doctor")})) notify(tr("Couldn't open the Doctor"), true); }
    else if (id == "annotate") startAnnotate();
    else if (id == "newwindow") { if (!QProcess::startDetached(QCoreApplication::applicationFilePath(), {QStringLiteral("--new")})) notify(tr("Couldn't open a new window"), true); }
    else if (id == "queue") { setSidebarOpen(true); m_side->showQueue(true); }
    else if (id == "search") { setSidebarOpen(true); m_side->focusSearch(); }
    else qWarning() << "unknown action" << id;
    if (id != "escape") poke();
}

void MainWindow::startAnnotate() {
    const MediaKind k = m_ctl->kind();
    if (!m_ctl->hasMedia() || k == MediaKind::Audio) { notify(tr("Open a video, image or GIF to annotate"), true); return; }
    if (m_annot->active()) { m_annot->end(); return; }
    closePanel();
    m_bar->reveal(false); m_top->reveal(true); m_tip->hide();
    m_annot->begin();
    updateGlass();
    notify(tr("Draw on the frame. Save exports it at full resolution."));
}

void MainWindow::toggleFullscreen() {
    // A double-click or a fast F-F must not thrash the window style: ignore toggles that arrive within 350 ms.
    if (m_fsGuard.isValid() && m_fsGuard.elapsed() < 350) return;
    m_fsGuard.start();
    m_full ? exitFullscreen() : enterFullscreen();
}

#ifndef Q_OS_WIN
void MainWindow::fitFullscreenToMonitor() {}
#else
// Re-fit the borderless window to whichever monitor it is on (also after the display setup changes).
void MainWindow::fitFullscreenToMonitor() {
    if (!m_full) return;
    HWND h = reinterpret_cast<HWND>(winId());
    MONITORINFO mi{sizeof(mi)};
    if (!GetMonitorInfo(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi)) return;
    const bool safe = m_set->flag("fsSafe", false);
    RECT r = safe ? mi.rcWork : mi.rcMonitor;
    if (!safe) r.bottom -= 1;
    SetWindowPos(h, safe ? HWND_TOP : HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE);
    layoutAll();
}
#endif

void MainWindow::enterFullscreen() {
    if (m_full) return;
    if (m_mini) toggleMini();
#ifndef Q_OS_WIN
    m_wasMax = isMaximized(); m_restoreSidebar = m_sidebarOpen; m_full = true; showFullScreen();
#else
    HWND h = reinterpret_cast<HWND>(winId());
    // Remember where the window lives WITHOUT un-maximising it first: SW_RESTORE plays Windows' restore animation,
    // which shows up as a blink right before fullscreen. GetWindowPlacement gives the normal rectangle directly.
    WINDOWPLACEMENT wp{sizeof(wp)};
    GetWindowPlacement(h, &wp);
    m_wasMax = wp.showCmd == SW_SHOWMAXIMIZED;
    const RECT wr = wp.rcNormalPosition;
    m_fsRestoreX = wr.left; m_fsRestoreY = wr.top; m_fsRestoreW = wr.right - wr.left; m_fsRestoreH = wr.bottom - wr.top;
    m_fsStyle = quint64(GetWindowLongPtr(h, GWL_STYLE)) & ~quint64(WS_MAXIMIZE);
    m_restoreSidebar = m_sidebarOpen;
    applyDwmFrame(false);                                       // an extended DWM frame can blank GL windows covering the monitor
    SetWindowLongPtr(h, GWL_STYLE, WS_POPUP | WS_VISIBLE);
    MONITORINFO mi{sizeof(mi)};
    GetMonitorInfo(MonitorFromWindow(h, MONITOR_DEFAULTTONEAREST), &mi);
    m_full = true;
    // A window that covers the monitor EXACTLY makes Windows/GPU drivers switch into a special flip mode, which is
    // what blanks the screen on/off. Stay 1px short of the bottom edge (invisible) so that never triggers,
    // and sit above the taskbar. "Safe" mode instead fills the work area and leaves the taskbar visible.
    RECT r = m_set->flag("fsSafe", false) ? mi.rcWork : mi.rcMonitor;
    if (!m_set->flag("fsSafe", false)) r.bottom -= 1;
    SetWindowPos(h, m_set->flag("fsSafe", false) ? HWND_TOP : HWND_TOPMOST, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW | SWP_NOACTIVATE);
#endif
    layoutAll();
    QTimer::singleShot(80, this, [this] { refreshChrome(); poke(); m_video->update(); });
}

void MainWindow::exitFullscreen() {
    if (!m_full) return;
#ifndef Q_OS_WIN
    m_full = false; if (m_wasMax) showMaximized(); else showNormal();
#else
    HWND h = reinterpret_cast<HWND>(winId());
    m_full = false;
    SetWindowLongPtr(h, GWL_STYLE, LONG_PTR(m_fsStyle));
    applyDwmFrame(true);
    // If the monitor the window came from is gone (or the layout changed), bring the window back onto a visible screen.
    RECT rr{m_fsRestoreX, m_fsRestoreY, m_fsRestoreX + m_fsRestoreW, m_fsRestoreY + m_fsRestoreH};
    HMONITOR mon = MonitorFromRect(&rr, MONITOR_DEFAULTTONEAREST);
    MONITORINFO mi{sizeof(mi)};
    if (GetMonitorInfo(mon, &mi)) {
        const int w = qMin<int>(m_fsRestoreW, mi.rcWork.right - mi.rcWork.left), hh = qMin<int>(m_fsRestoreH, mi.rcWork.bottom - mi.rcWork.top);
        rr.left = std::clamp<LONG>(rr.left, mi.rcWork.left, LONG(mi.rcWork.right - w)); rr.top = std::clamp<LONG>(rr.top, mi.rcWork.top, LONG(mi.rcWork.bottom - hh));
        rr.right = rr.left + w; rr.bottom = rr.top + hh;
    }
    SetWindowPos(h, m_pinned ? HWND_TOPMOST : HWND_NOTOPMOST, rr.left, rr.top, rr.right - rr.left, rr.bottom - rr.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    if (m_wasMax) {                                             // straight back to maximised, no un-maximise/maximise flicker
        WINDOWPLACEMENT wp{sizeof(wp)};
        wp.showCmd = SW_SHOWMAXIMIZED;
        wp.rcNormalPosition = rr;
        SetWindowPlacement(h, &wp);
    }
#endif
    layoutAll();
    QTimer::singleShot(80, this, [this] { refreshChrome(); poke(); m_video->update(); });
}

void MainWindow::toggleMini() {
    if (!m_mini) {
        if (m_full) exitFullscreen();
        m_wasMax = isMaximized();
        if (m_wasMax) showNormal();
        m_restoreGeo = geometry();
        closePanel();
        m_mini = true;
        const QRect a = screen()->availableGeometry();
        const int w = 440, hh = 248;
        setMinimumSize(280, 160);
        setGeometry(a.right() - w - 20, a.bottom() - hh - 20, w, hh);
        setTopmost(true);
    } else {
        m_mini = false;
        setMinimumSize(420, 260);
        setGeometry(m_restoreGeo);
        if (m_wasMax) showMaximized();
        setTopmost(m_pinned);
    }
    layoutAll();
    refreshChrome();
}

void MainWindow::togglePin() {
    m_pinned = !m_pinned;
    setTopmost(m_pinned);
    m_top->setPinned(m_pinned);
    notify(m_pinned ? tr("Always on top") : tr("Not on top"));
}

void MainWindow::setSidebarOpen(bool open, bool animate, bool automatic) {
    m_sidebarOpen = open;
    if (!automatic) { m_sideAuto = false; m_set->set("sidebarOpen", open); }       // only the user's own choice is remembered, never the automatic hide
    m_sideAnim.stop();
    m_sideAnim.setDuration(Theme::i().animMs(320));
    if (animate && m_sideAnim.duration() > 0 && !m_full && !m_mini) {
        m_sideAnim.setStartValue(m_sideT); m_sideAnim.setEndValue(open ? 1.0 : 0.0); m_sideAnim.start();
    } else { m_sideT = open ? 1.0 : 0.0; layoutAll(); }
}

void MainWindow::togglePanel(const QString& which) {
    if (m_panel->isOpen() && m_panel->page() == which) { closePanel(); return; }
    if (m_annot->active()) m_annot->end();
    m_panel->showPage(which);
    layoutAll();
}
void MainWindow::closePanel() { if (m_panel->isOpen()) m_panel->slideOut(); }

void MainWindow::cycleTheme() {
    int m = (int(Theme::i().mode()) + 1) % 3;
    Theme::i().setMode(Theme::Mode(m));
    m_set->set("theme", m);
    static const char* names[] = {"System", "Light", "Dark"};
    notify(tr("Theme: %1").arg(QString::fromLatin1(names[m])));
}

void MainWindow::screenshotFrame() {
    if (!m_ctl->hasMedia()) { notify(tr("Nothing to capture yet"), true); return; }
    const QString f = Paths::exportsDir() + QStringLiteral("/frame-") + QDateTime::currentDateTime().toString("yyyyMMdd-HHmmss-zzz") + QStringLiteral(".png");
    if (m_mpv->commandSync({"screenshot-to-file", f, "video"})) notify(tr("Frame saved to the exports folder"));
    else notify(tr("Couldn't capture this frame"), true);
}

QString MainWindow::diagnosticsText() const {
    auto prop = [this](const char* n) { return m_mpv->getProp(n); };
    QStringList L;
    L << "## kinoz";
    L << QStringLiteral("Version: ") + QStringLiteral(APP_VERSION);
    L << QStringLiteral("Built: ") + QStringLiteral(__DATE__ " " __TIME__);
    L << QStringLiteral("Data folder: ") + QDir::toNativeSeparators(Paths::dataDir());
    L << QStringLiteral("Log file: ") + QDir::toNativeSeparators(Paths::logsDir() + "/app.log");
    L << "## System";
    L << QStringLiteral("Operating system: ") + QSysInfo::prettyProductName() + " (" + QSysInfo::kernelVersion() + ")";
    L << QStringLiteral("CPU: ") + QSysInfo::currentCpuArchitecture() + ", " + QString::number(QThread::idealThreadCount()) + " threads";
    if (QScreen* s = screen()) L << QStringLiteral("Display: %1×%2 at %3 Hz, scale %4×").arg(s->size().width()).arg(s->size().height()).arg(s->refreshRate(), 0, 'f', 0).arg(devicePixelRatioF());
    L << QStringLiteral("Graphics: ") + m_video->rendererInfo();
    L << QStringLiteral("Glass effect: ") + (m_video->glassOk() ? "available" : "not available");
    L << "## Playback engine";
    L << QStringLiteral("mpv: ") + prop("mpv-version").toString();
    L << QStringLiteral("FFmpeg: ") + prop("ffmpeg-version").toString();
    L << QStringLiteral("Interface: Qt ") + QString::fromLatin1(qVersion());
    L << QStringLiteral("Hardware decoding: ") + (m_set->flag("hwdec", true) ? QStringLiteral("on (%1)").arg(prop("hwdec-current").toString().isEmpty() ? QStringLiteral("not in use for this file") : prop("hwdec-current").toString()) : QStringLiteral("off"));
    if (m_ctl->hasMedia()) {
        L << "## Current media";
        L << QStringLiteral("File type: .") + QFileInfo(m_ctl->path()).suffix().toLower();      // never the name or folder: this text is meant to be pasted into a bug report
        const QVariantMap vp = prop("video-params").toMap();
        if (m_mpv->videoW() > 0) L << QStringLiteral("Picture: %1×%2, %3").arg(m_mpv->videoW()).arg(m_mpv->videoH()).arg(vp.value("pixelformat").toString());
        const QString vc = prop("video-codec").toString(), ac = prop("audio-codec-name").toString();
        if (!vc.isEmpty()) L << QStringLiteral("Video codec: ") + vc;
        if (!ac.isEmpty()) L << QStringLiteral("Audio codec: ") + ac;
        const double fps = prop("container-fps").toDouble();
        if (fps > 0) L << QStringLiteral("Frame rate: %1 fps").arg(fps, 0, 'f', 2);
        const qint64 sz = prop("file-size").toLongLong();
        if (sz > 0) L << QStringLiteral("Size: ") + QLocale().formattedDataSize(sz);
        if (m_mpv->duration() > 0) L << QStringLiteral("Duration: %1 s").arg(m_mpv->duration(), 0, 'f', 1);
    }
    L << "## Supported formats";
    auto join = [](const QSet<QString>& s) { QStringList l = s.values(); l.sort(); return l.join(", "); };
    L << QStringLiteral("Video: ") + join(Media::videoExt());
    L << QStringLiteral("Audio: ") + join(Media::audioExt());
    L << QStringLiteral("Images: ") + join(Media::imageExt());
    L << QStringLiteral("Animated: ") + join(Media::gifExt());
    L << QStringLiteral("Subtitles: ") + join(Media::subtitleExt());
    L << "## Storage";
    qint64 bytes = 0; int n = 0;
    for (const QFileInfo& f : QDir(Paths::thumbsDir()).entryInfoList({"*.jpg"}, QDir::Files)) { bytes += f.size(); ++n; }
    L << QStringLiteral("Thumbnails: %1 files, %2").arg(n).arg(QLocale().formattedDataSize(bytes));
    L << QStringLiteral("Exports: ") + QDir::toNativeSeparators(Paths::exportsDir());
    L << "## Updates";
    L << QStringLiteral("Mode: ") + (m_updater->mode() == Updater::Automatic ? QStringLiteral("automatic") : m_updater->mode() == Updater::Notify ? QStringLiteral("notify only") : QStringLiteral("off"));
    L << QStringLiteral("Last result: ") + (m_set->str("updateLast").isEmpty() ? QStringLiteral("no check yet") : m_set->str("updateLast"));
    L << QStringLiteral("Can this copy update itself: ") + (Updater::unsupportedReason().isEmpty() ? QStringLiteral("yes") : Updater::unsupportedReason());
    L << QStringLiteral("Install type: ") + Log::installType();
    L << "## Health";
    for (const Doctor::Check& c : Doctor::runCore(Paths::dataDir(), false, false, true)) L << c.name + QStringLiteral(": ") + (c.state == 0 ? QStringLiteral("OK") : c.state == 1 ? QStringLiteral("warning") : QStringLiteral("PROBLEM")) + QStringLiteral(", ") + c.detail;
    return L.join('\n');
}

void MainWindow::applySetting(const QString& key) {
    if (key == "theme") Theme::i().setMode(Theme::Mode(int(m_set->num("theme", 0))));
    else if (key == "accent") Theme::i().setAccentIndex(int(m_set->num("accent", 0)));
    else if (key == "motion") Theme::i().setMotionMode(int(m_set->num("motion", 0)));
    else if (key == "compact") Theme::i().setCompact(m_set->num("compact", 0) > 0);
    else if (key == "glass") { Theme::i().setGlass(m_set->flag("glass", true)); updateGlass(); }
    else if (key == "hideDelay") { applyHideDelay(); poke(); }
    else if (key == "privateMode") {
        const bool on = m_set->flag("privateMode", false);
        m_thumbs->setPrivate(on); m_top->setPrivate(on);
        notify(on ? tr("Private mode is on: nothing you play is remembered") : tr("Private mode is off: history is saved again"));
    }
    else if (key == "rememberMute") { if (m_set->flag("rememberMute", true) && m_mpv->muted()) m_set->set("muted", true); else m_set->remove("muted"); }
    else if (key == "skipStep") refreshChrome();
    else if (key == "slideshow") restartSlideshow();
    else if (key == "normalize") m_ctl->applyEq();
    else if (key == "decode") { m_ctl->applyDecode(); notify(tr("Decoding changes apply to the next file")); }
}

void MainWindow::applyHideDelay() {
    const int s = int(m_set->num("hideDelay", 2));
    m_idle.setInterval(qMax(1, s) * 1000);
}

void MainWindow::restartSlideshow() {
    m_slide.stop();
    const int s = int(m_set->num("slideshow", 0));
    if (s > 0 && m_ctl->kind() == MediaKind::Image) m_slide.start(s * 1000);
}

void MainWindow::setSleepTimer(int minutes) {
    m_sleep.stop();
    m_ctl->setStopAfterCurrent(minutes < 0);
    if (minutes > 0) m_sleep.start(minutes * 60 * 1000);
    notify(minutes == 0 ? tr("Sleep timer off") : minutes < 0 ? tr("Will stop at the end of this file") : tr("Sleep timer: %1 min").arg(minutes));
}

void MainWindow::showSleepMenu() {
    auto* m = new PopupMenu(this);
    m->addHeader(tr("Sleep timer"));
    const bool on = m_sleep.isActive() || m_ctl->stopAfterCurrent();
    m->addItem("0", tr("Off"), !on);
    for (int mins : {15, 30, 45, 60, 90}) m->addItem(QString::number(mins), tr("%1 minutes").arg(mins), m_sleep.isActive() && m_sleep.interval() == mins * 60000);
    m->addItem("-1", tr("At the end of this file"), m_ctl->stopAfterCurrent());
    connect(m, &PopupMenu::triggered, this, [this](const QString& id) { setSleepTimer(id.toInt()); });
    m->popupAbove(m_bar->isVisible() ? m_bar->buttonRect("more") : QRect(QCursor::pos(), QSize(1, 1)));
}

void MainWindow::showAspectMenu() {
    auto* m = new PopupMenu(this);
    m->addHeader(tr("Aspect ratio"));
    const QString cur = m_mpv->getProp("video-aspect-override").toString();
    const bool fill = m_mpv->getProp("panscan").toDouble() > 0.5;
    m->addItem("-1", tr("Original"), cur == "-1" && !fill);
    for (const char* a : {"16:9", "4:3", "21:9", "1:1"}) m->addItem(QString::fromLatin1(a), QString::fromLatin1(a), cur == a && !fill);
    m->addItem("fill", tr("Fill the window (crop)"), fill);
    connect(m, &PopupMenu::triggered, this, [this](const QString& id) {
        if (id == "fill") { m_mpv->setProp("video-aspect-override", QStringLiteral("-1")); m_mpv->setProp("panscan", 1.0); }
        else { m_mpv->setProp("panscan", 0.0); m_mpv->setProp("video-aspect-override", id); }
    });
    m->popupAbove(m_bar->isVisible() ? m_bar->buttonRect("more") : QRect(QCursor::pos(), QSize(1, 1)));
}

// ---------------- playlists ----------------
void MainWindow::promptText(const QString& title, const QString& label, const QString& text, const QString& okText, std::function<void(const QString&)> onOk) {
    auto* sh = new Sheet(this);
    auto* box = new InputBox(title, label, text, okText);
    sh->setContent(box, QSize(460, 220));
    connect(box, &InputBox::cancelled, sh, &Sheet::closeSheet);
    connect(box, &InputBox::accepted, this, [sh, onOk](const QString& t) { sh->closeSheet(); onOk(t); });
    sh->openSheet();
    QTimer::singleShot(0, box, [box] { box->focusEdit(); });
}

void MainWindow::savePlaylist() {
    const QStringList files = m_ctl->queue()->stringList();
    if (files.isEmpty()) { notify(tr("The queue is empty. Play something first, then save it."), true); return; }
    promptText(tr("Save as playlist"), tr("%n song(s) from the queue", nullptr, files.size()), Playlists::uniqueName(tr("Playlist")), tr("Save"), [this, files](const QString& name) {
        const QString n = Playlists::sanitize(name);
        auto write = [this, files, n] {
            QString err;
            if (Playlists::save(n, files, &err)) { notify(tr("Saved “%1”").arg(n)); m_side->showPlaylists(); }
            else notify(tr("Couldn't save the playlist: %1").arg(err), true);
        };
        if (Playlists::exists(n)) confirm(tr("Replace “%1”?").arg(n), tr("A playlist with this name already exists."), tr("Replace"), write, false);
        else write();
    });
}

void MainWindow::renamePlaylist(const QString& name) {
    promptText(tr("Rename playlist"), tr("New name"), name, tr("Rename"), [this, name](const QString& to) {
        QString err;
        if (Playlists::rename(name, Playlists::sanitize(to), &err)) { notify(tr("Renamed")); m_side->showPlaylists(); }
        else notify(err, true);
    });
}

void MainWindow::deletePlaylist(const QString& name) {
    confirm(tr("Delete “%1”?").arg(name), tr("The playlist file is removed. Your songs are not touched."), tr("Delete playlist"), [this, name] {
        if (Playlists::remove(name)) { notify(tr("Playlist deleted")); m_side->showPlaylists(); }
        else notify(tr("Couldn't delete the playlist"), true);
    });
}

// ---------------- deleting (always to the Recycle Bin, always asks first) ----------------
void MainWindow::confirm(const QString& title, const QString& body, const QString& okText, std::function<void()> onOk, bool danger, const QString& cancelText) {
    auto* sh = new Sheet(this);
    auto* box = new ConfirmBox(title, body, okText, danger, nullptr, cancelText);
    sh->setContent(box, QSize(460, 220));
    connect(box, &ConfirmBox::cancelled, sh, &Sheet::closeSheet);
    connect(box, &ConfirmBox::accepted, this, [sh, onOk] { sh->closeSheet(); onOk(); });
    sh->openSheet();
    QTimer::singleShot(0, box, qOverload<>(&QWidget::setFocus));
}

void MainWindow::trashCurrent() {
    if (!m_ctl->hasMedia()) return;
    const QString path = m_ctl->path();
    confirm(tr("Move to the Recycle Bin?"), tr("“%1” will be moved to the Recycle Bin. You can restore it from there.").arg(QFileInfo(path).fileName()),
            tr("Move to Recycle Bin"), [this, path] { trashFiles({path}); });
}

void MainWindow::trashRejected() {
    const QStringList files = m_ctl->rejected();
    if (files.isEmpty()) { notify(tr("Nothing is marked to reject. Press N on a file to mark it.")); return; }
    qint64 bytes = 0;
    for (const QString& f : files) bytes += QFileInfo(f).size();
    confirm(tr("Move %n rejected file(s) to the Recycle Bin?", nullptr, files.size()),
            tr("%1 in total. You can restore them from the Recycle Bin.").arg(QLocale().formattedDataSize(bytes, 1)),
            tr("Move to Recycle Bin"), [this, files] { trashFiles(files); });
}

void MainWindow::trashFiles(const QStringList& files) {
    if (files.isEmpty()) return;
    m_ctl->dropFiles(files);                     // playback moves off a file that is about to go; marks and resume points are forgotten
    // mpv needs a moment to let go of the file; try again once if Windows still has it open.
    auto attempt = [](const QStringList& l) { QStringList failed; for (const QString& f : l) if (QFileInfo::exists(f) && !QFile::moveToTrash(f)) failed << f; return failed; };
    QTimer::singleShot(450, this, [this, files, attempt] {
        const QStringList failed = attempt(files);
        if (failed.isEmpty()) { notify(tr("Moved %n file(s) to the Recycle Bin", nullptr, files.size())); m_side->refreshFolder(); return; }
        QTimer::singleShot(900, this, [this, failed, attempt, total = int(files.size())] {
            const QStringList still = attempt(failed);
            if (still.isEmpty()) notify(tr("Moved %n file(s) to the Recycle Bin", nullptr, total));
            else notify(tr("Couldn't move %n file(s): in use, or this drive has no Recycle Bin", nullptr, int(still.size())), true);
            m_side->refreshFolder();
        });
    });
}

// ---------------- privacy: wipe history, private mode, clean identity ----------------
void MainWindow::wipeLogs() {
    // Everything in the logs folder except the log that is open right now (it is replaced at the next start anyway).
    for (const QFileInfo& f : QDir(Paths::logsDir()).entryInfoList(QDir::Files)) if (f.fileName() != QLatin1String("app.log")) QFile::remove(f.absoluteFilePath());
}

// Like Tor's "new identity": forgets what you played and where you looked. Your folders, settings, playlists and saved frames stay.
void MainWindow::cleanIdentity(bool announce) {
    m_ctl->clearRecent();
    m_ctl->clearResume();
    m_ctl->clearMarks();
    m_thumbs->clearAll();
    m_side->clearMusicIndex();
    wipeLogs();
    m_set->remove("lastFolder");
    m_set->remove("muted");                                                       // a remembered mute is part of "what you did last time"
    m_empty->setRecent({});
    m_side->update();
    if (announce) notify(tr("Clean identity done: history, marks, thumbnails, the music index and logs are gone"));
}

void MainWindow::privacyAction(const QString& id) {
    if (id == "clearRecent") { m_ctl->clearRecent(); m_empty->setRecent({}); notify(tr("Recent files cleared")); }
    else if (id == "clearResume") { m_ctl->clearResume(); notify(tr("Resume points cleared")); }
    else if (id == "clearMarks") { m_ctl->clearMarks(); notify(tr("Watched and keep/reject marks cleared")); }
    else if (id == "clearThumbs") { m_thumbs->clearAll(); m_side->update(); notify(tr("Thumbnails cleared")); }
    else if (id == "clearMusic") { m_side->clearMusicIndex(); notify(tr("Music index cleared")); }
    else if (id == "clearLogs") { wipeLogs(); notify(tr("Logs cleared")); }
    else if (id == "openData") QDesktopServices::openUrl(QUrl::fromLocalFile(Paths::dataDir()));
    else if (id == "forgetFolders")
        confirm(tr("Forget your folders?"), tr("The library goes back to empty. Your files are not touched."), tr("Forget folders"), [this] { m_side->forgetPlaces(); notify(tr("Library folders forgotten")); });
    else if (id == "cleanIdentity")
        confirm(tr("Clean identity?"), tr("Clears your recent files, resume points, watched and keep/reject marks, thumbnails, the music index, logs and your last folder. Your folders, settings, playlists and saved frames stay. This can't be undone."),
                tr("Clean identity"), [this] { cleanIdentity(true); });
}

void MainWindow::openSettings() {
    auto* sh = new Sheet(this);
    auto* pg = new SettingsPage(m_set, m_keys, m_updater, diagnosticsText());
    sh->setContent(pg, QSize(660, 680));
    connect(pg, &SettingsPage::closeRequested, sh, &Sheet::closeSheet);
    connect(pg, &SettingsPage::makeDefault, this, [this] { doAction("defaultplayer"); });
    connect(pg, &SettingsPage::changed, this, [this](const QString& key) { applySetting(key); });
    connect(pg, &SettingsPage::keymapChanged, this, [this](const QHash<QString, QString>& k) { m_keys = k; });   // shortcut edits apply at once
    connect(pg, &SettingsPage::privacyAction, this, [this](const QString& id) { privacyAction(id); });
    sh->openSheet();
}

void MainWindow::openPicker(bool folder) {
    auto* sh = new Sheet(this);
    QString start = m_set->str("lastFolder");
    if (start.isEmpty() || !QDir(start).exists()) start = QDir::homePath();
    auto* fp = new FolderPicker(folder ? FolderPicker::PickFolder : FolderPicker::PickFile, start);
    sh->setContent(fp, QSize(580, 540));
    connect(fp, &FolderPicker::cancelled, sh, &Sheet::closeSheet);
    connect(fp, &FolderPicker::chosen, this, [this, sh, folder](const QString& path) {
        sh->closeSheet();
        if (folder) { m_side->addPlace(path); m_side->navigate(path); setSidebarOpen(true); notify(tr("Added “%1” to your places").arg(QFileInfo(path).fileName().isEmpty() ? path : QFileInfo(path).fileName())); }
        else m_ctl->play(path);
    });
    sh->openSheet();
}

void MainWindow::showMoreMenu(const QPoint& anchorGlobal) {
    auto* m = new PopupMenu(this);
    const MediaKind k = m_ctl->kind();
    const bool has = m_ctl->hasMedia();
    m->addItem("openfile", tr("Open file…"));
    m->addItem("openfolder", tr("Add folder…"), false, "Ctrl+O");
    m->addItem("palette", tr("Search & commands…"), false, "Ctrl+K");
    m->addItem("newwindow", tr("New window"), false, "Ctrl+N");
    m->addSeparator();
    if (has && k != MediaKind::Image) m->addItem("speed", tr("Playback speed…"));
    if (has && (k == MediaKind::Video || k == MediaKind::Audio)) m->addItem("abloop", tr("A-B loop"), false, "B");
    if (has && (k == MediaKind::Video || k == MediaKind::Audio)) m->addItem("sleep", tr("Sleep timer…"));
    if (has && (k == MediaKind::Video || k == MediaKind::Gif)) m->addItem("aspect", tr("Aspect ratio…"));
    if (k == MediaKind::Video) m->addItem("tracks", tr("Audio & subtitles…"));
    if (k == MediaKind::Video || k == MediaKind::Gif) m->addItem("adjust", tr("Adjust picture…"), m_panel->isOpen() && m_panel->page() == "adjust", "Ctrl+J");
    if (k == MediaKind::Video || k == MediaKind::Audio) m->addItem("eq", tr("Equalizer…"), m_panel->isOpen() && m_panel->page() == "eq", "Ctrl+E");
    if (has && k != MediaKind::Audio) { m->addItem("annotate", tr("Annotate frame"), false, "A"); m->addItem("screenshot", tr("Save frame as image"), false, "Ctrl+Shift+S"); }
    m->addSeparator();
    if (has) { m->addItem("reveal", tr("Show in folder")); m->addItem("copypath", tr("Copy file path")); m->addSeparator(); }
    m->addItem("sidebar", tr("Library sidebar"), m_sidebarOpen, "Tab");
    m->addItem("pin", tr("Always on top"), m_pinned);
    m->addItem("theme", tr("Switch theme"), false, "Ctrl+D");
    m->addItem("settings", tr("Settings…"), false, "Ctrl+,");
    m->addItem("defaultplayer", tr("Make kinoz the default player…"));
    connect(m, &PopupMenu::triggered, this, [this](const QString& id) { QTimer::singleShot(0, this, [this, id] { doAction(id); }); });
    if (anchorGlobal.x() >= 0) m->popup(anchorGlobal);                       // opened from the top bar button
    else if (m_bar->isVisible()) m->popupAbove(m_bar->buttonRect("more"));
    else m->popup(QCursor::pos());
}

void MainWindow::showSpeedMenu() {
    auto* m = new PopupMenu(this);
    m->addHeader(tr("Playback speed"));
    for (double s : {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0})
        m->addItem(QString::number(s), s == 1.0 ? tr("Normal") : QStringLiteral("%1×").arg(s, 0, 'g', 3), qFuzzyCompare(m_mpv->speed(), s));
    connect(m, &PopupMenu::triggered, this, [this](const QString& id) { m_ctl->setSpeed(id.toDouble()); });
    m->popupAbove(m_bar->buttonRect("speed"));
}

void MainWindow::showTracksMenu() {
    auto* m = new PopupMenu(this);
    QVariantList audio, subs;
    for (const auto& t : m_mpv->tracks()) {
        const QVariantMap tm = t.toMap();
        if (tm["type"] == "audio") audio << tm; else if (tm["type"] == "sub") subs << tm;
    }
    auto label = [](const QVariantMap& tm) {
        QStringList p;
        if (!tm["title"].toString().isEmpty()) p << tm["title"].toString();
        if (!tm["lang"].toString().isEmpty()) p << tm["lang"].toString().toUpper();
        if (!tm["codec"].toString().isEmpty()) p << tm["codec"].toString();
        return p.isEmpty() ? QStringLiteral("Track %1").arg(tm["id"].toInt()) : p.join(" · ");
    };
    if (!audio.isEmpty()) { m->addHeader(tr("Audio")); for (const auto& t : audio) { const auto tm = t.toMap(); m->addItem(QStringLiteral("a%1").arg(tm["id"].toInt()), label(tm), tm["selected"].toBool()); } }
    m->addHeader(tr("Subtitles"));
    bool anySelected = false;
    for (const auto& t : subs) if (t.toMap()["selected"].toBool()) anySelected = true;
    m->addItem("s0", tr("Off"), !anySelected);
    for (const auto& t : subs) { const auto tm = t.toMap(); m->addItem(QStringLiteral("s%1").arg(tm["id"].toInt()), label(tm), tm["selected"].toBool()); }
    if (subs.isEmpty()) m->addItem("none", tr("No subtitle tracks in this file"), false, {}, false);
    m->addSeparator();
    m->addItem("audioearlier", tr("Audio earlier (sync)"));
    m->addItem("audiolater", tr("Audio later (sync)"));
    m->addItem("subminus", tr("Subtitles earlier"), false, "Z");
    m->addItem("subplus", tr("Subtitles later"), false, "X");
    m->addItem("subsmaller", tr("Smaller text"));
    m->addItem("subbigger", tr("Bigger text"));
    connect(m, &PopupMenu::triggered, this, [this](const QString& id) {
        if (id.startsWith('a')) m_ctl->selectTrack("audio", id.mid(1).toInt());
        else if (id.startsWith('s') && id.size() > 1 && id[1].isDigit()) m_ctl->selectTrack("sub", id.mid(1).toInt());
        else if (id == "audioearlier") { const double d = m_mpv->getProp("audio-delay").toDouble() - 0.1; m_mpv->setProp("audio-delay", d); notify(tr("Audio delay %1 s").arg(d, 0, 'f', 1)); }
        else if (id == "audiolater") { const double d = m_mpv->getProp("audio-delay").toDouble() + 0.1; m_mpv->setProp("audio-delay", d); notify(tr("Audio delay %1 s").arg(d, 0, 'f', 1)); }
        else if (id == "subminus") doAction("subdelayminus");
        else if (id == "subplus") doAction("subdelayplus");
        else if (id == "subsmaller") m_ctl->setSubScale(m_mpv->getProp("sub-scale").toDouble() - 0.1);
        else if (id == "subbigger") m_ctl->setSubScale(m_mpv->getProp("sub-scale").toDouble() + 0.1);
    });
    m->popupAbove(m_bar->buttonRect("tracks"));
}

void MainWindow::applyAppPalette() {
    const auto& c = pal();
    QApplication::setStyle(QStringLiteral("Fusion"));
    QPalette p;
    p.setColor(QPalette::Window, c.bg); p.setColor(QPalette::WindowText, c.text);
    p.setColor(QPalette::Base, c.raised); p.setColor(QPalette::AlternateBase, c.surface);
    p.setColor(QPalette::Text, c.text); p.setColor(QPalette::Button, c.surface); p.setColor(QPalette::ButtonText, c.text);
    p.setColor(QPalette::ToolTipBase, c.raised); p.setColor(QPalette::ToolTipText, c.text);
    p.setColor(QPalette::Highlight, c.accent); p.setColor(QPalette::HighlightedText, c.onAccent);
    p.setColor(QPalette::PlaceholderText, c.muted);
    QApplication::setPalette(p);
    QToolTip::setPalette(p);
}

// ---------------- diagnostics / self test ----------------
QString MainWindow::debugState() const {
    return QStringLiteral("path=%1 kind=%2 playing=%3 pos=%4 dur=%5 vol=%6 speed=%7 muted=%8 full=%9 mini=%10 side=%11 panel=%12 queue=%13")
        .arg(QFileInfo(m_ctl->path()).fileName()).arg(int(m_ctl->kind())).arg(!m_mpv->paused()).arg(m_mpv->position(), 0, 'f', 1).arg(m_mpv->duration(), 0, 'f', 1)
        .arg(m_mpv->volume(), 0, 'f', 0).arg(m_mpv->speed()).arg(m_mpv->muted()).arg(m_full).arg(m_mini).arg(m_sidebarOpen).arg(m_panel->page() + (m_panel->isOpen() ? "(open)" : "")).arg(m_ctl->queue()->rowCount());
}

// --bench <video>: the way you really use it (click through files, spam next, drag the seek bar), measured.
// Writes data/logs/bench.txt with PASS/FAIL against budgets. Silent and off-screen with KINOZ_SILENT / KINOZ_OFFSCREEN.
void MainWindow::runBench() {
    m_selfTest = true;
    auto* out = new QFile(Paths::logsDir() + "/bench.txt");
    out->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    auto log = [out](const QString& s) { QTextStream(out) << s << '\n'; out->flush(); };
    auto fails = std::make_shared<int>(0);
    auto verdict = [log, fails](const QString& name, double v, double budget, const QString& unit) {
        const bool ok = v <= budget;
        if (!ok) ++*fails;
        log(QStringLiteral("%1  %2: %3 %4   (budget %5 %4)").arg(ok ? QStringLiteral("PASS") : QStringLiteral("FAIL"), name).arg(v, 0, 'f', 0).arg(unit).arg(budget, 0, 'f', 0));
    };
    // The UI thread must never stall: a 4 ms heartbeat records the longest gap between two beats.
    struct Mon { QElapsedTimer t; qint64 max = 0; QTimer tm; };
    auto mon = std::make_shared<Mon>();
    mon->t.start(); mon->tm.setInterval(4);
    connect(&mon->tm, &QTimer::timeout, this, [mon] { const qint64 g = mon->t.restart(); if (g > mon->max) mon->max = g; });
    mon->tm.start();

    auto loaded = [this] { return m_ctl->settled() && m_mpv->videoW() > 0; };      // cached state only: a blocking mpv query would itself freeze the UI
    auto waitUntil = [this](std::function<bool()> cond, int timeoutMs, std::function<void(bool, qint64)> done) {
        auto t = std::make_shared<QElapsedTimer>(); t->start();
        auto poll = std::make_shared<std::function<void()>>();
        *poll = [this, cond, done, t, timeoutMs, poll] {
            if (cond()) { done(true, t->elapsed()); return; }
            if (t->elapsed() > timeoutMs) { done(false, t->elapsed()); return; }
            QTimer::singleShot(4, this, *poll);
        };
        (*poll)();
    };
    using Next = std::function<void()>;
    auto phases = std::make_shared<QVector<std::function<void(Next)>>>();
    auto P = [phases](std::function<void(Next)> f) { phases->push_back(std::move(f)); };
    auto started = std::make_shared<QElapsedTimer>(); started->start();

    P([=](Next next) { waitUntil([this] { return m_mpv->videoW() > 0; }, 10000, [=](bool ok, qint64 ms) { verdict(QStringLiteral("first frame after launch"), ok ? double(started->elapsed()) : 99999, 3000, QStringLiteral("ms")); (void)ms; next(); }); });
    P([=](Next next) {
        const QString dir = Paths::dataDir() + QStringLiteral("/bench-media");
        QDir().mkpath(dir);
        const QString src = m_ctl->path();
        for (int i = 1; i <= 8; ++i) QFile::copy(src, QStringLiteral("%1/clip-%2.%3").arg(dir).arg(i).arg(QFileInfo(src).suffix()));
        m_ctl->open({dir});
        waitUntil([=] { return m_ctl->queue()->rowCount() == 8 && loaded(); }, 8000, [=](bool ok, qint64 ms) { verdict(QStringLiteral("open a folder of 8 files"), ok ? double(ms) : 99999, 1500, QStringLiteral("ms")); next(); });
    });
    P([=](Next next) {                                              // calm clicking: one file after another
        auto times = std::make_shared<QVector<qint64>>();
        auto step = std::make_shared<std::function<void()>>();
        mon->max = 0;
        *step = [=] {
            if (times->size() >= 6) {
                verdict(QStringLiteral("click-through: longest UI freeze"), double(mon->max), 100, QStringLiteral("ms"));
                qint64 sum = 0, mx = 0; for (qint64 t : *times) { sum += t; mx = qMax(mx, t); }
                verdict(QStringLiteral("click to picture, average"), double(sum) / times->size(), 700, QStringLiteral("ms"));
                verdict(QStringLiteral("click to picture, slowest"), double(mx), 1500, QStringLiteral("ms"));
                next(); return;
            }
            m_ctl->next();
            waitUntil(loaded, 6000, [=](bool ok, qint64 ms) { times->push_back(ok ? ms : 9999); QTimer::singleShot(120, this, *step); });
        };
        (*step)();
    });
    P([=](Next next) {                                              // skimming: 150 clicks, 3 ms apart
        mon->max = 0;
        if (m_ctl->repeat() == PlayerController::RepeatOff) m_ctl->cycleRepeat();      // wrap around, so every click really loads a file
        const int before = m_ctl->loadCount();
        auto n = std::make_shared<int>(0);
        auto slow = std::make_shared<QPair<qint64, int>>(0, 0);
        auto t = new QTimer(this); t->setInterval(3);
        connect(t, &QTimer::timeout, this, [=] {
            if (++*n <= 150) {
                QElapsedTimer one; one.start();
                m_ctl->next();
                if (one.elapsed() > slow->first) { slow->first = one.elapsed(); slow->second = *n; }
                return;
            }
            log(QStringLiteral("info  slowest single next(): %1 ms at click %2").arg(slow->first).arg(slow->second));
            t->stop(); t->deleteLater();
            const qint64 stall = mon->max;
            waitUntil(loaded, 6000, [=](bool ok, qint64 ms) {
                verdict(QStringLiteral("skip spam: longest UI freeze"), double(stall), 150, QStringLiteral("ms"));
                verdict(QStringLiteral("skip spam: time to show the last file"), ok ? double(ms) : 99999, 1000, QStringLiteral("ms"));
                verdict(QStringLiteral("skip spam: files really opened"), m_ctl->loadCount() - before, 12, QStringLiteral("files"));
                next();
            });
        });
        t->start();
    });
    P([=](Next next) {                                              // dragging the seek bar like mad
        mon->max = 0;
        auto n = std::make_shared<int>(0);
        auto t = new QTimer(this); t->setInterval(4);
        connect(t, &QTimer::timeout, this, [=] {
            if (++*n <= 300) { m_ctl->seekFrac(QRandomGenerator::global()->generateDouble(), false); return; }
            t->stop(); t->deleteLater();
            verdict(QStringLiteral("seek-bar drag: longest UI freeze"), double(mon->max), 150, QStringLiteral("ms"));
            next();
        });
        t->start();
    });
    P([=](Next next) {
#ifdef Q_OS_WIN
        PROCESS_MEMORY_COUNTERS pmc{};
        if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) verdict(QStringLiteral("memory (working set) after all that"), pmc.WorkingSetSize / 1048576.0, 700, QStringLiteral("MB"));
#endif
        next();
    });
    auto idx = std::make_shared<int>(0);
    auto runNext = std::make_shared<std::function<void()>>();
    *runNext = [=] {
        if (*idx >= phases->size()) {
            log(QStringLiteral("DONE  %1 checks, %2 over budget").arg(phases->size()).arg(*fails));
            out->close();
            QTimer::singleShot(200, qApp, &QApplication::quit);
            return;
        }
        auto ph = (*phases)[(*idx)++];
        ph([=] { QTimer::singleShot(150, this, *runNext); });
    };
    QTimer::singleShot(600, this, *runNext);
}

void MainWindow::runSelfTest() {
    m_selfTest = true;
    auto* out = new QFile(Paths::logsDir() + "/selftest.txt");
    out->open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text);
    auto log = [out](const QString& s) { QTextStream(out) << s << '\n'; out->flush(); };
    struct Step { QString name; std::function<void()> run; std::function<bool()> check; int waitMs; };
    auto steps = new QVector<Step>;
    auto S = [steps](const QString& n, std::function<void()> r, std::function<bool()> c, int w = 350) { steps->push_back({n, r, c, w}); };
    S("playing after open", [] {}, [this] { return m_mpv->isReady() && m_ctl->hasMedia() && !m_mpv->paused(); }, 1500);
    S("queue built from siblings", [] {}, [this] { return m_ctl->queue()->rowCount() >= 1; }, 600);
    S("pause", [this] { doAction("playpause"); }, [this] { return m_mpv->paused(); });
    S("resume", [this] { doAction("playpause"); }, [this] { return !m_mpv->paused(); });
    S("seek forward", [this] { m_mpv->seek(1, true); }, [this] { return m_mpv->position() >= 0.5; }, 700);
    S("seek +5", [this] { doAction("seekfwd"); }, [this] { return m_mpv->position() >= 4.0 || m_mpv->duration() < 6; }, 700);
    S("volume up", [this] { m_ctl->setVolume(50); doAction("volup"); }, [this] { return m_mpv->volume() >= 54.9; });
    S("volume down", [this] { doAction("voldown"); }, [this] { return m_mpv->volume() <= 50.1; });
    S("volume rapid x10 is exact", [this] { m_ctl->setVolume(20); for (int i = 0; i < 10; ++i) doAction("volup"); }, [this] { return qAbs(m_mpv->volume() - 70) < 0.5; }, 500);
    S("mute", [this] { doAction("mute"); }, [this] { return m_mpv->muted(); });
    S("mute is remembered for the next start", [] {}, [this] { return m_mpv->muted() && m_set->flag("muted", false); });
    S("unmute", [this] { doAction("mute"); }, [this] { return !m_mpv->muted() && !m_set->flag("muted", false); });
    S("speed up", [this] { doAction("speedup"); }, [this] { return m_mpv->speed() > 1.0; });
    S("speed reset", [this] { doAction("speedreset"); }, [this] { return qFuzzyCompare(m_mpv->speed(), 1.0); });
    S("shuffle on", [this] { doAction("shuffle"); }, [this] { return m_ctl->shuffle(); });
    S("shuffle off", [this] { doAction("shuffle"); }, [this] { return !m_ctl->shuffle(); });
    S("repeat cycles", [this] { doAction("repeat"); }, [this] { return m_ctl->repeat() == PlayerController::RepeatAll; });
    S("repeat off", [this] { doAction("repeat"); doAction("repeat"); }, [this] { return m_ctl->repeat() == PlayerController::RepeatOff; });
    S("next", [this] { doAction("next"); }, [this] { return m_ctl->hasMedia(); }, 800);
    S("prev", [this] { m_mpv->seek(0, true); doAction("prev"); }, [this] { return m_ctl->hasMedia(); }, 800);
    S("skip forward uses your step", [this] { m_set->set("skipStep", 3); m_mpv->seek(0.2, true); m_selfPos = m_mpv->position(); doAction("skipfwd"); },
      [this] { return m_mpv->position() >= m_selfPos + 2.0 || m_mpv->duration() < m_selfPos + 4.0; }, 700);
    S("skip back uses your step", [this] { m_mpv->seek(2.5, true); m_selfPos = m_mpv->position(); doAction("skipback"); },
      [this] { m_set->set("skipStep", 10); return m_mpv->position() <= m_selfPos - 2.0 || m_selfPos < 3.0; }, 700);
    S("shortcut editor: pick an action, press a key, saved and live", [this] {
        SettingsPage pg(m_set, m_keys, m_updater, QString());
        pg.resize(660, 650);
        QHash<QString, QString> got;
        connect(&pg, &SettingsPage::keymapChanged, &pg, [&got](const QHash<QString, QString>& k) { got = k; });
        auto* list = pg.findChild<RowList*>();
        int row = -1;
        if (list) for (int i = 0; i < list->rows().size(); ++i) if (list->rows()[i].data == QLatin1String("act:nextfile")) row = i;
        if (row >= 0) {
            emit list->activated(row);
            QKeyEvent ev(QEvent::KeyPress, Qt::Key_F5, Qt::NoModifier);
            QCoreApplication::sendEvent(&pg, &ev);
        }
        m_selfOk = row >= 0 && got.value("F5") == QLatin1String("nextfile") && !got.contains("Right") && QFile::exists(Paths::dataDir() + QStringLiteral("/keymap.json"));
        QFile::remove(Paths::dataDir() + QStringLiteral("/keymap.json"));
    }, [this] { return m_selfOk; }, 100);
    S("updater: version and tag parsing", [] {},
      [] {
          return Updater::isNewer("0.10.0", "0.9.9") && !Updater::isNewer("1.0", "1.0.0") && Updater::isNewer("v0.3.0", "0.2.9") && !Updater::isNewer("0.2.3", "0.2.3")
              && Updater::tagFromLocation(QUrl("https://github.com/p4inz-code/kinoz/releases/tag/v0.3.1")) == QLatin1String("v0.3.1")
              && Updater::tagFromLocation(QUrl("https://github.com/evil/kinoz/releases/tag/v0.3.1")).isEmpty()
              && Updater::tagFromLocation(QUrl("http://github.com/p4inz-code/kinoz/releases/tag/v0.3.1")).isEmpty()
              && Updater::tagFromLocation(QUrl("https://github.com/p4inz-code/kinoz/releases/tag/v0.3.1/../../x")).isEmpty()
              && Updater::assetNameFor("0.3.1").contains("0.3.1");
      }, 20);
    S("updater: only dropped connections are retried, skipped versions stay quiet", [this] { m_set->set("skippedVersion", QStringLiteral("9.9.9")); },
      [this] {
          const bool ok = Updater::shouldRetryDownload(0, 4, 0) && Updater::shouldRetryDownload(503, 0, 1) && !Updater::shouldRetryDownload(404, 0, 0)
              && !Updater::shouldRetryDownload(0, 5, 0) && !Updater::shouldRetryDownload(0, 4, 3) && Updater::isSkipped(m_set, QStringLiteral("9.9.9")) && !Updater::isSkipped(m_set, QStringLiteral("9.9.10"));
          m_set->remove("skippedVersion");
          return ok;
      }, 20);
    S("a damaged settings file is restored from its backup", [this] {
        const QString f = Paths::tmpDir() + QStringLiteral("/jstest.json");
        QFile::remove(f); QFile::remove(f + ".bak"); QFile::remove(f + ".corrupt");
        { JsonStore a(f); a.set("keep", 42); a.flush(); }
        { JsonStore b(f); }                                           // opening it makes the backup copy
        { QFile w(f); w.open(QIODevice::WriteOnly | QIODevice::Truncate); w.write("{ this is not json"); }
        JsonStore c(f);
        m_selfOk = c.num("keep") == 42 && c.recoveredFromBackup() && QFile::exists(f + ".corrupt");
        QFile::remove(f); QFile::remove(f + ".bak"); QFile::remove(f + ".corrupt");
    }, [this] { return m_selfOk; }, 50);
    S("a damaged file with no backup starts clean and is kept aside", [this] {
        const QString f = Paths::tmpDir() + QStringLiteral("/jstest2.json");
        QFile::remove(f); QFile::remove(f + ".bak"); QFile::remove(f + ".corrupt");
        { QFile w(f); w.open(QIODevice::WriteOnly | QIODevice::Truncate); w.write("garbage"); }
        JsonStore c(f);
        m_selfOk = c.object().isEmpty() && !c.recoveredFromBackup() && QFile::exists(f + ".corrupt");
        QFile::remove(f); QFile::remove(f + ".corrupt");
    }, [this] { return m_selfOk; }, 50);
    S("doctor finds a damaged file and restores it", [this] {
        const QString d = Paths::tmpDir() + QStringLiteral("/doctor-test");
        QDir(d).removeRecursively(); QDir().mkpath(d + "/logs");
        { QFile w(d + "/settings.json"); w.open(QIODevice::WriteOnly); w.write("{ broken"); }
        { QFile w(d + "/settings.json.bak"); w.open(QIODevice::WriteOnly); w.write("{\"theme\":2}"); }
        bool found = false;
        for (const Doctor::Check& c : Doctor::runCore(d, false, false, false)) if (c.name == QObject::tr("Settings files") && c.state == 1 && c.fixable) { found = true; Doctor::applyCoreFix(c.fix, d); }
        QFile r(d + "/settings.json");
        m_selfOk = found && r.open(QIODevice::ReadOnly) && QJsonDocument::fromJson(r.readAll()).object().value("theme").toInt() == 2;
        r.close(); QDir(d).removeRecursively();
    }, [this] { return m_selfOk; }, 50);
    S("doctor engine check starts the engine headless", [this] {
        m_selfOk = false;
        for (const Doctor::Check& c : Doctor::runCore(Paths::tmpDir(), false, true, false)) if (c.name == QObject::tr("Playback engine")) m_selfOk = c.state == 0;
    }, [this] { return m_selfOk; }, 400);
    S("a playlist file with junk loads only the real local files", [this] {
        const QString f = Paths::tmpDir() + QStringLiteral("/junk.m3u8");
        const QString real = m_ctl->path();
        { QFile w(f); w.open(QIODevice::WriteOnly | QIODevice::Truncate);
          const QByteArray r = real.toUtf8();
          w.write(QByteArray("#EXTM3U\n") + r + "\n" + r + "\n");                                                   // the same file twice
          w.write(QByteArray("\\\\attacker\\share\\evil.mp4\n//attacker/share/evil.mp4\nhttp://example.com/a.mp4\nfile:///nonexistent/nothing.mp3\n"));
          w.write(QByteArray(200000, 'x')); }                                                                      // one absurdly long line
        const QStringList got = Playlists::load(f);
        m_selfOk = got.size() == 1 && got.first().compare(QDir::cleanPath(real), Qt::CaseInsensitive) == 0;
        QFile::remove(f);
    }, [this] { return m_selfOk; }, 50);
    S("a song with no embedded cover shows the folder image beside it", [this] {
        const QString d = Paths::tmpDir() + QStringLiteral("/arttest");
        QDir(d).removeRecursively(); QDir().mkpath(d);
        QImage img(64, 64, QImage::Format_RGB32); img.fill(QColor("#c2410c")); img.save(d + QStringLiteral("/folder.jpg"), "JPG");
        for (const char* n : {"song.mp3", "song2.mp3", "song3.mp3"}) { QFile w(d + QLatin1Char('/') + QLatin1String(n)); w.open(QIODevice::WriteOnly); w.write("not really audio"); m_thumbs->get(d + QLatin1Char('/') + QLatin1String(n), MediaKind::Audio); }
    }, [this] {
        bool ok = true;                                                        // every song of the folder gets the picture, not just the first
        for (const char* n : {"song.mp3", "song2.mp3", "song3.mp3"}) ok = ok && !m_thumbs->get(Paths::tmpDir() + QStringLiteral("/arttest/") + QLatin1String(n), MediaKind::Audio).isNull();
        if (ok) QDir(Paths::tmpDir() + QStringLiteral("/arttest")).removeRecursively();
        return ok;
    }, 900);
    S("updater: a redirect may only lead to https on GitHub", [] {},
      [] {
          return Updater::redirectAllowed(QUrl("https://objects.githubusercontent.com/x/y")) && Updater::redirectAllowed(QUrl("https://github.com/p4inz-code/kinoz/releases/download/v1/a"))
              && Updater::redirectAllowed(QUrl("https://release-assets.githubusercontent.com/z"))
              && !Updater::redirectAllowed(QUrl("http://github.com/x")) && !Updater::redirectAllowed(QUrl("https://evil.example.com/github.com"))
              && !Updater::redirectAllowed(QUrl("https://github.com.evil.example.com/x")) && !Updater::redirectAllowed(QUrl("https://notgithub.com/x"))
              && !Updater::redirectAllowed(QUrl("https://user:pw@github.com/x")) && !Updater::redirectAllowed(QUrl("https://github.com:8443/x"));
      }, 20);
    S("keymap file keeps only what changed", [] {},
      [] {
          const QHash<QString, QString> def{{"J", "skipback"}, {"L", "skipfwd"}, {"M", "mute"}};
          const QHash<QString, QString> cur{{"J", "skipback"}, {"N", "mute"}, {"L", "skipfwd"}};
          const QJsonObject o = SettingsPage::keymapOverrides(cur, def);
          return o.size() == 2 && o.value("N").toString() == "mute" && o.contains("M") && o.value("M").toString().isEmpty();
      }, 50);
    S("right arrow goes to the next file", [this] { m_selfPath = m_ctl->path(); doAction("nextfile"); }, [this] { return m_ctl->hasMedia() && m_ctl->path() != m_selfPath; }, 800);
    S("left arrow goes back, even deep into a file", [this] { m_selfPath = m_ctl->path(); m_mpv->seek(2.0, true); doAction("prevfile"); }, [this] { return m_ctl->hasMedia() && m_ctl->path() != m_selfPath; }, 800);
    S("frame step", [this] { doAction("framestep"); }, [this] { return m_mpv->paused(); });
    S("play again", [this] { doAction("playpause"); }, [this] { return !m_mpv->paused(); });
    S("sidebar forced open", [this] { setSidebarOpen(true, false); }, [this] { return m_sidebarOpen && m_sideT > 0.99 && m_side->isVisible(); }, 400);
    S("sidebar toggle off", [this] { doAction("sidebar"); }, [this] { return !m_sidebarOpen && m_sideT < 0.01; }, 600);
    S("sidebar toggle on", [this] { doAction("sidebar"); }, [this] { return m_sidebarOpen && m_sideT > 0.99; }, 600);
    S("library opens a folder", [this] { m_side->navigate(QFileInfo(m_ctl->path()).absolutePath()); }, [this] { return m_side->libraryRows() > 3; }, 1500);
    S("thumbnails are generated", [] {}, [] { return QDir(Paths::thumbsDir()).entryList({"*.jpg"}, QDir::Files).size() >= 2; }, 6000);
    S("shot 1", [this] { poke(); }, [this] { return grab().save(Paths::logsDir() + "/shot-1.png"); }, 700);
    S("queue tab", [this] { doAction("queue"); }, [this] { return m_ctl->queue()->rowCount() > 0; }, 400);
    S("library search box", [this] { doAction("search"); }, [this] { return m_side->isVisible(); }, 300);
    S("index builds for palette", [this] { m_index->rebuild({QFileInfo(m_ctl->path()).absolutePath()}); }, [this] { return m_index->count() > 3 && !m_index->search("brush", 5).isEmpty(); }, 2500);
    S("palette opens", [this] { doAction("palette"); }, [this] { return m_palette->isOpen(); });
    S("shot 3", [this] { poke(); }, [this] { return grab().save(Paths::logsDir() + "/shot-3.png"); }, 700);
    S("palette closes (esc)", [this] { doAction("escape"); }, [this] { return !m_palette->isOpen(); });
    S("adjust panel opens", [this] { doAction("adjust"); }, [this] { return m_panel->isOpen() && m_panel->page() == "adjust"; }, 500);
    S("shot 2", [this] { poke(); }, [this] { return grab().save(Paths::logsDir() + "/shot-2.png"); }, 700);
    S("color brightness applies", [this] { m_ctl->setColor("brightness", 20); }, [this] { return m_mpv->getProp("brightness").toInt() == 20; });
    S("color reset", [this] { m_ctl->resetColor(); }, [this] { return m_mpv->getProp("brightness").toInt() == 0; });
    S("eq panel opens", [this] { doAction("eq"); }, [this] { return m_panel->isOpen() && m_panel->page() == "eq"; }, 500);
    S("eq enables filter chain", [this] { m_set->set("eq.enabled", true); m_set->set("eq.bands", QJsonArray{3, 2, 1, 0, 0, 0, 0, 1, 2, 3}); m_ctl->applyEq(); }, [this] { return !m_mpv->getProp("af").toList().isEmpty(); }, 600);
    S("eq off", [this] { m_set->set("eq.enabled", false); m_ctl->applyEq(); doAction("eq"); }, [this] { return m_mpv->getProp("af").toList().isEmpty(); }, 600);
    auto visibleSheets = [this] { int n = 0; for (auto* s : findChildren<Sheet*>()) if (s->isVisible()) ++n; return n; };
    S("settings opens", [this] { doAction("settings"); }, [visibleSheets] { return visibleSheets() >= 1; }, 600);
    S("shot 4", [this] { poke(); }, [this] { return grab().save(Paths::logsDir() + "/shot-4.png"); }, 700);
    S("settings closes", [this] { for (auto* s : findChildren<Sheet*>()) s->closeSheet(); }, [visibleSheets] { return visibleSheets() == 0; }, 700);
    S("folder picker opens", [this] { doAction("openfolder"); }, [visibleSheets] { return visibleSheets() >= 1; }, 600);
    S("folder picker closes", [this] { for (auto* s : findChildren<Sheet*>()) s->closeSheet(); }, [visibleSheets] { return visibleSheets() == 0; }, 700);
    if (qEnvironmentVariableIsSet("KINOZ_TEST_FULLSCREEN")) {     // only ever on explicit request: it flashes the real screen
        S("fullscreen on", [this] { doAction("fullscreen"); }, [this] { QRect r = screen()->geometry(); return m_full && qAbs(frameGeometry().width() - r.width()) <= 2; }, 900);
        S("fullscreen keeps playing", [] {}, [this] { return !m_mpv->paused(); }, 500);
        S("fullscreen off", [this] { doAction("fullscreen"); }, [this] { return !m_full; }, 900);
    }
    S("mini on", [this] { doAction("mini"); }, [this] { return m_mini && width() < 600; }, 700);
    S("mini off", [this] { doAction("mini"); }, [this] { return !m_mini && width() > 600; }, 700);
    S("light theme", [this] { Theme::i().setMode(Theme::Mode::Light); poke(); }, [this] { return grab().save(Paths::logsDir() + "/shot-5.png"); }, 700);
    if (qEnvironmentVariableIsSet("KINOZ_UPDATE_E2E")) {            // with KINOZ_FAKE_VERSION=0.0.1: the real release is found, downloaded, verified and staged (never applied)
        S("update e2e: the release is found with a build for this system", [this] { m_updater->checkNow(true); }, [this] { return m_updater->state() == Updater::Available && m_updater->canInstall(); }, 6000);
        S("update e2e: it downloads, verifies and stages", [this] { m_updater->download(false); }, [this] { return m_updater->state() == Updater::Ready; }, 60000);
        S("update e2e: the staged file would be applied at the next start", [] {}, [this] { return Updater::applyPendingOnStartup(m_set, true); }, 100);
    }
    S("update check works over HTTPS", [this] { m_updater->checkNow(true); }, [this] { return m_updater->state() == Updater::UpToDate || m_updater->state() == Updater::Available; }, 4500);
    S("pin on/off", [this] { doAction("pin"); doAction("pin"); }, [this] { return !m_pinned; });
    S("theme cycle", [this] { doAction("theme"); doAction("theme"); doAction("theme"); }, [this] { return true; });
    S("screenshot saves file", [this] { doAction("screenshot"); }, [] { return !QDir(Paths::exportsDir()).entryList({"frame-*.png"}, QDir::Files).isEmpty(); }, 900);
    S("annotate begins", [this] { doAction("annotate"); }, [this] { return m_ctl->kind() == MediaKind::Audio || m_annot->active(); }, 500);
    S("annotate ends", [this] { doAction("escape"); }, [this] { return !m_annot->active(); });
    S("error: missing file reports", [this] { m_ctl->play("Z:/definitely/not/here.mp4"); }, [] { return true; }, 500);
    S("still alive after error", [] {}, [this] { return m_mpv->isReady(); }, 300);
    S("hardening: no external tools or scripts", [] {}, [this] {
        auto off = [this](const char* o) { const QVariant v = m_mpv->getProp(o); return v.toString() == "no" || v.toString() == "false" || (v.typeId() == QMetaType::Bool && !v.toBool()); };
        return off("options/ytdl") && off("options/load-scripts"); }, 200);
    S("settings: shortcuts, info, playback pages", [this] { doAction("settings"); }, [this] {
        auto* pg = findChild<SettingsPage*>(); if (!pg) return false;
        auto* list = pg->findChild<RowList*>(); const auto views = pg->findChildren<QTextBrowser*>();
        return bool(list) && list->rows().size() > 40 && views.size() >= 2; }, 700);
    S("settings: close", [this] { for (auto* s : findChildren<Sheet*>()) s->closeSheet(); }, [this] { return findChildren<Sheet*>().isEmpty() || !findChildren<Sheet*>().first()->isVisible(); }, 700);
    S("animations off really removes motion", [this] { m_set->set("motion", 2); applySetting("motion"); }, [] { return Theme::i().reduceMotion() && Theme::i().animMs(200) == 0; }, 300);
    S("animations back on", [this] { m_set->set("motion", 1); applySetting("motion"); }, [] { return !Theme::i().reduceMotion() && Theme::i().animMs(200) == 200; }, 300);
    S("accent colour changes live", [this] { m_set->set("accent", 3); applySetting("accent"); }, [] { return Theme::i().accentIndex() == 3; }, 300);
    S("accent reset", [this] { m_set->set("accent", 0); applySetting("accent"); }, [] { return Theme::i().accentIndex() == 0; }, 300);
    S("seek step setting is honoured", [this] { m_set->set("seekSmall", 10); m_mpv->seek(1, true); }, [this] { return true; }, 600);
    S("seek +10 from setting", [this] { const double before = m_mpv->position(); m_seekRef = before; doAction("seekfwd"); }, [this] { return m_mpv->position() >= m_seekRef + 8.0 || m_mpv->duration() < m_seekRef + 12; }, 800);
    S("seek step reset", [this] { m_set->set("seekSmall", 5); }, [] { return true; }, 100);
    S("sleep timer set and cleared", [this] { setSleepTimer(15); }, [this] { return m_sleep.isActive(); }, 200);
    S("sleep timer off", [this] { setSleepTimer(0); }, [this] { return !m_sleep.isActive(); }, 200);
    if (qEnvironmentVariableIsSet("KINOZ_TEST_HIRES")) {      // heavy GPU/CPU stress: only when explicitly asked for
        S("4K frame decodes and renders", [this] { m_mpv->setPause(false); m_mpv->loadFile("av://lavfi:testsrc2=size=3840x2160:rate=30"); }, [this] { return m_mpv->videoW() == 3840 && m_mpv->videoH() == 2160 && !m_mpv->paused(); }, 2500);
        S("8K frame decodes and renders", [this] { m_mpv->loadFile("av://lavfi:testsrc2=size=7680x4320:rate=24"); }, [this] { return m_mpv->videoW() == 7680 && m_mpv->videoH() == 4320; }, 3000);
        S("recovers after 8K (loads a normal file)", [this] { m_ctl->playIndex(0); }, [this] { return m_ctl->hasMedia() && m_mpv->videoW() > 0 && m_mpv->videoW() < 3000; }, 1500);
    }
    // ---- skimming features: marks, number keys, skip-spam ----
    auto before = std::make_shared<int>(0);
    S("test folder of 6 clips opens", [this] {
        const QString dir = Paths::dataDir() + QStringLiteral("/selftest-media");
        QDir().mkpath(dir);
        const QString src = m_ctl->path();
        for (int i = 1; i <= 6; ++i) QFile::copy(src, QStringLiteral("%1/clip-%2.%3").arg(dir).arg(i).arg(QFileInfo(src).suffix()));
        m_ctl->open({dir});
    }, [this] { return m_ctl->queue()->rowCount() == 6 && m_ctl->hasMedia(); }, 1600);
    S("keep mark", [this] { doAction("keep"); }, [this] { return m_ctl->reviewState(m_ctl->path()) & PlayerController::MarkKeep; }, 200);
    S("reject replaces keep", [this] { doAction("reject"); }, [this] { const int s = m_ctl->reviewState(m_ctl->path()); return (s & PlayerController::MarkReject) && !(s & PlayerController::MarkKeep); }, 200);
    S("same key clears the mark", [this] { doAction("reject"); }, [this] { return !(m_ctl->reviewState(m_ctl->path()) & (PlayerController::MarkKeep | PlayerController::MarkReject)); }, 200);
    S("number key 5 jumps to the middle", [this] { m_mpv->setPause(true); doAction("seekpct5"); }, [this] { return m_mpv->duration() < 1 || qAbs(m_mpv->position() / m_mpv->duration() - 0.5) < 0.25; }, 900);
    S("0 returns to the start", [this] { doAction("zoomreset"); }, [this] { return m_mpv->position() < 1.0; }, 900);
    S("progress marks the file as seen", [this] { m_mpv->seek(1.5, true); }, [this] { m_ctl->saveResumeNow(); return m_ctl->reviewState(m_ctl->path()) & PlayerController::MarkSeen; }, 700);
    S("next unwatched skips seen files", [this] { doAction("nextunwatched"); }, [this] { return m_ctl->hasMedia() && !(m_ctl->reviewState(m_ctl->path()) & PlayerController::MarkReject); }, 900);
    S("skip-spam burst", [this, before] { if (m_ctl->repeat() == PlayerController::RepeatOff) m_ctl->cycleRepeat(); *before = m_ctl->loadCount(); for (int i = 0; i < 25; ++i) m_ctl->next(); }, [] { return true; }, 50);
    S("skip-spam opened only the last file", [] {}, [this, before] {
        const QString want = QFileInfo(m_ctl->path()).fileName();
        const QString have = QFileInfo(m_mpv->getProp("path").toString()).fileName();
        return have == want && m_ctl->loadCount() - *before <= 2;               // 25 clicks, at most the first and the last were opened
    }, 900);
    S("reject then delete list excludes unmarked", [this] { doAction("reject"); }, [this] { return m_ctl->rejected().size() == 1; }, 200);
    S("drop forgets files and moves playback on", [this] { m_ctl->dropFiles(m_ctl->rejected()); }, [this] { return m_ctl->queue()->rowCount() == 5 && m_ctl->rejected().isEmpty(); }, 500);
    // ---- privacy: history is saved by default, and every wipe really wipes ----
    S("history is saved by default", [this] { m_ctl->forgetFile(m_ctl->path()); m_set->set("privateMode", false); m_ctl->saveResumeNow(); m_ctl->markSeen(m_ctl->path(), 0.5); },
      [this] { return m_ctl->watched(m_ctl->path()) > 0.4 && !m_ctl->recent().isEmpty(); }, 200);
    S("private mode saves no progress or recent file", [this] {
        m_ctl->clearRecent(); m_ctl->forgetFile(m_ctl->path()); m_set->set("privateMode", true); applySetting("privateMode");
        m_ctl->markSeen(m_ctl->path(), 0.9); m_ctl->saveResumeNow(); m_ctl->next(); },
      [this] { return m_ctl->recent().isEmpty() && m_ctl->watched(m_ctl->path()) < 0.01; }, 900);
    S("private mode off again", [this] { m_set->set("privateMode", false); applySetting("privateMode"); }, [this] { return !m_ctl->privateMode(); }, 100);
    S("forget this file removes its traces", [this] { m_ctl->markSeen(m_ctl->path(), 0.8); m_ctl->toggleMark(m_ctl->path(), PlayerController::MarkKeep); m_ctl->forgetFile(m_ctl->path()); },
      [this] { return m_ctl->reviewState(m_ctl->path()) == 0 && m_ctl->watched(m_ctl->path()) == 0.0; }, 100);
    S("clear recent, resume and marks", [this] {
        m_ctl->markSeen(m_ctl->path(), 0.7); m_ctl->toggleMark(m_ctl->path(), PlayerController::MarkReject);
        privacyAction("clearRecent"); privacyAction("clearResume"); privacyAction("clearMarks"); },
      [this] { return m_ctl->recent().isEmpty() && m_ctl->reviewState(m_ctl->path()) == 0 && !m_resume->has(m_ctl->path().toLower()); }, 200);
    S("clean identity wipes everything it promises", [this] {
        m_ctl->markSeen(m_ctl->path(), 0.6); m_set->set("lastFolder", QStringLiteral("X:/somewhere")); m_set->set("muted", true);
        QFile f(Paths::logsDir() + QStringLiteral("/old-crash.dmp")); f.open(QIODevice::WriteOnly); f.write("x"); f.close();
        cleanIdentity(false); },
      [this] { return m_ctl->recent().isEmpty() && m_ctl->watched(m_ctl->path()) == 0.0 && !m_set->has("lastFolder") && !m_set->has("muted") && !QFile::exists(Paths::logsDir() + QStringLiteral("/old-crash.dmp")) && !QFile::exists(Paths::dataDir() + QStringLiteral("/music.json")); }, 300);
    S("--wipe style cleanup leaves settings and playlists alone", [this] { Playlists::save(QStringLiteral("keepme"), {m_ctl->path()}); cleanIdentity(false); },
      [this] { const bool ok = Playlists::exists(QStringLiteral("keepme")) && m_set->has("decode"); Playlists::remove(QStringLiteral("keepme")); return ok; }, 200);

    // ---- playlists and the music library ----
    auto plCount = std::make_shared<int>(0);
    S("playlist saves", [this, plCount] { *plCount = m_ctl->queue()->rowCount(); Playlists::save(QStringLiteral("selftest list"), m_ctl->queue()->stringList()); },
      [plCount] { return Playlists::exists(QStringLiteral("selftest list")) && Playlists::tracks(QStringLiteral("selftest list")).size() == *plCount && *plCount > 0; }, 300);
    S("playlist opens as a queue", [this] { m_ctl->open({Playlists::fileFor(QStringLiteral("selftest list"))}); }, [this, plCount] { return m_ctl->queue()->rowCount() == *plCount && m_ctl->hasMedia(); }, 800);
    S("playlist names are made safe", [] {}, [] { return Playlists::sanitize(QStringLiteral("a/b:c*")) == QStringLiteral("a_b_c_") && Playlists::sanitize(QStringLiteral("CON")) == QStringLiteral("Playlist"); }, 50);
    S("playlist is deleted", [] { Playlists::remove(QStringLiteral("selftest list")); }, [] { return !Playlists::exists(QStringLiteral("selftest list")); }, 100);
    S("playlist refuses network paths (no login leak)", [this] {
        const QString clip = Paths::dataDir() + QStringLiteral("/selftest-media/clip-1.") + QFileInfo(m_ctl->path()).suffix();
        QFile f(Paths::dataDir() + QStringLiteral("/selftest-evil.m3u8")); f.open(QIODevice::WriteOnly | QIODevice::Truncate);
        f.write(("\\\\127.0.0.1\\share\\a.mp4\n//attacker/share/b.mp4\nfile://attacker/share/c.mp4\nhttp://example.com/d.mp4\n" + QDir::toNativeSeparators(clip).toUtf8() + "\n"));
    }, [this] {
        const QStringList l = Playlists::load(Paths::dataDir() + QStringLiteral("/selftest-evil.m3u8"));
        return l.size() == 1 && l.first().contains(QStringLiteral("clip-1."));
    }, 100);
    S("live list refresh keeps its scroll position", [] {}, [this] {
        RowList list; list.resize(300, 200);
        QVector<RowList::Row> rows; for (int i = 0; i < 100; ++i) { RowList::Row r; r.title = QStringLiteral("row %1").arg(i); rows << r; }
        list.setRows(rows);
        QWheelEvent ev(QPointF(50, 50), QPointF(50, 50), QPoint(), QPoint(0, -240), Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
        QApplication::sendEvent(&list, &ev);
        const int moved = list.scrollPos();
        list.setRows(rows, true);  const int kept = list.scrollPos();
        list.setRows(rows, false); const int reset = list.scrollPos();
        return moved > 0 && kept == moved && reset == 0;
    }, 100);
    auto idxp = std::make_shared<MusicIndex*>(nullptr);
    S("music index reads tags and lengths", [this, idxp] {
        const QString dir = Paths::dataDir() + QStringLiteral("/selftest-music");
        QDir().mkpath(dir);
        for (int i = 1; i <= 3; ++i) {                                               // three 1-second WAV files with known content
            QFile f(QStringLiteral("%1/0%2 - Song %2.wav").arg(dir).arg(i));
            if (!f.open(QIODevice::WriteOnly)) continue;
            const quint32 n = 8000 * 2, riff = 36 + n, rate = 8000, byteRate = 16000, fmtLen = 16;
            const quint16 pcm = 1, ch = 1, align = 2, bits = 16;
            QDataStream s(&f); s.setByteOrder(QDataStream::LittleEndian);
            f.write("RIFF"); s << riff; f.write("WAVEfmt "); s << fmtLen << pcm << ch << rate << byteRate << align << bits; f.write("data"); s << n;
            QByteArray pcmData(int(n), '\0');
            for (int k = 0; k < 8000; ++k) { const qint16 v = qint16(8000 * std::sin(k * 0.2 * i)); memcpy(pcmData.data() + k * 2, &v, 2); }
            f.write(pcmData);
        }
        auto* idx = new MusicIndex(this);
        *idxp = idx;
        idx->setRoots({dir});
        idx->ensureScanned();
    }, [idxp] {
        MusicIndex* idx = *idxp;
        if (!idx || idx->scanning()) return false;
        const auto t = idx->tracks();
        if (t.size() != 3) return false;
        for (const auto& x : t) if (x.dur < 0.9 || x.dur > 1.1 || x.title.startsWith(QLatin1String("0"))) return false;       // length read, "01 - " prefix stripped from the fallback title
        return true;
    }, 5000);
#ifdef Q_OS_WIN
    // ---- updates: what happens to a downloaded update the user left for "later" (decided without launching anything) ----
    auto stageFake = [this](const QString& version, bool tamper) {
        QDir().mkpath(Updater::stagingDir());
        const QString file = Updater::stagingDir() + QStringLiteral("/kinoz-Setup-fake.exe");
        QFile f(file); f.open(QIODevice::WriteOnly | QIODevice::Truncate); f.write("not a real installer, never run"); f.close();
        QFile fr(file); fr.open(QIODevice::ReadOnly);
        const QString sha = QString::fromLatin1(QCryptographicHash::hash(fr.readAll(), QCryptographicHash::Sha256).toHex());
        fr.close();
        if (tamper) { QFile t(file); t.open(QIODevice::Append); t.write("x"); }
        QJsonObject o; o["version"] = version; o["file"] = file; o["sha"] = sha; o["name"] = QStringLiteral("kinoz-Setup-fake.exe");
        QFile pf(Updater::stagingDir() + QStringLiteral("/pending.json")); pf.open(QIODevice::WriteOnly | QIODevice::Truncate); pf.write(QJsonDocument(o).toJson(QJsonDocument::Compact));
    };
    auto staged = [] { return QFile::exists(Updater::stagingDir() + QStringLiteral("/pending.json")); };
    S("update left for later is applied on next start", [this, stageFake] { m_set->set("updateMode", 0); m_set->set("updateAttempts", 0); stageFake(QStringLiteral("99.0.0"), false); },
      [this] { return Updater::applyPendingOnStartup(m_set, true) && int(m_set->num("updateAttempts")) == 1; }, 150);
    S("a second start tries again", [] {}, [this] { return Updater::applyPendingOnStartup(m_set, true) && int(m_set->num("updateAttempts")) == 2; }, 100);
    S("it gives up after two tries (no loop)", [] {}, [this, staged] { const bool a = Updater::applyPendingOnStartup(m_set, true); const bool ok = !a && !staged() && m_set->str("updateFailedVersion") == QStringLiteral("99.0.0"); m_set->remove("updateFailedVersion"); return ok; }, 100);
    S("a changed installer file is never run", [this, stageFake] { m_set->set("updateAttempts", 0); stageFake(QStringLiteral("99.0.0"), true); }, [this, staged] { return !Updater::applyPendingOnStartup(m_set, true) && !staged(); }, 150);
    S("an older staged version is just cleaned up", [this, stageFake] { stageFake(QStringLiteral("0.0.1"), false); }, [this, staged] { return !Updater::applyPendingOnStartup(m_set, true) && !staged(); }, 150);
    S("turning updates off discards a staged one", [this, stageFake] { stageFake(QStringLiteral("99.0.0"), false); m_updater->setMode(Updater::Off); }, [this, staged] { const bool ok = !staged() && m_updater->mode() == Updater::Off; m_updater->setMode(Updater::Automatic); return ok && m_updater->mode() == Updater::Automatic; }, 150);
#endif
    S("rapid next x30", [this] { for (int i = 0; i < 30; ++i) m_ctl->next(); }, [this] { return m_ctl->hasMedia(); }, 1200);
    S("garbage input never crashes the parsers (seeded random files and URLs)", [this] {
        QRandomGenerator rng(20261114);                                       // fixed seed: the same inputs every run
        const QString d = Paths::tmpDir() + QStringLiteral("/fuzz");
        QDir(d).removeRecursively(); QDir().mkpath(d);
        const QByteArray alphabet = QByteArray("{}[]\":,\n\r\t #=/.:-_%0aA\xff\x00\xc3\xa9", 26);
        for (int i = 0; i < 400; ++i) {
            QByteArray junk;
            const int n = int(rng.bounded(1, 2000));
            for (int k = 0; k < n; ++k) junk.append(rng.bounded(3) ? alphabet.at(int(rng.bounded(alphabet.size()))) : char(rng.bounded(256)));
            const QString f = d + QLatin1String("/f.json");
            { QFile w(f); w.open(QIODevice::WriteOnly | QIODevice::Truncate); w.write(junk); }
            { JsonStore js(f); js.object(); }                                 // settings, resume points, marks: any bytes in a store file
            { QFile w(d + QLatin1String("/p.m3u8")); w.open(QIODevice::WriteOnly | QIODevice::Truncate); w.write(junk); }
            (void)Playlists::load(d + QLatin1String("/p.m3u8"));              // playlists
            (void)Updater::tagFromLocation(QUrl(QString::fromUtf8(junk.left(120))));
            (void)Updater::redirectAllowed(QUrl(QString::fromUtf8(junk.left(120))));
            (void)Updater::isNewer(QString::fromUtf8(junk.left(30)), QStringLiteral("0.3.1"));
            QHash<QString, QString> cur{{QString::fromUtf8(junk.left(10)), QString::fromUtf8(junk.mid(10, 10))}};
            (void)SettingsPage::keymapOverrides(cur, cur);
        }
        QDir(d).removeRecursively();
        m_selfOk = true;                                                      // reaching this line without a crash is the test
    }, [this] { return m_selfOk; }, 50);
    S("gapless: two songs are queued, the first is playing", [this] {
        const QString d = Paths::tmpDir() + QStringLiteral("/gaptest");
        QDir(d).removeRecursively(); QDir().mkpath(d);
        for (const char* n : {"a.wav", "b.wav"}) {                                            // 3 s of silence each: 8 kHz, mono, 16 bit
            QByteArray pcm(8000 * 2 * 3, '\0'), h;
            auto le32 = [&h](quint32 v) { for (int i = 0; i < 4; ++i) h.append(char((v >> (8 * i)) & 0xff)); };
            auto le16 = [&h](quint16 v) { h.append(char(v & 0xff)); h.append(char(v >> 8)); };
            h.append("RIFF"); le32(36 + pcm.size()); h.append("WAVEfmt "); le32(16); le16(1); le16(1); le32(8000); le32(16000); le16(2); le16(16); h.append("data"); le32(pcm.size());
            QFile w(d + QLatin1Char('/') + QLatin1String(n)); w.open(QIODevice::WriteOnly); w.write(h); w.write(pcm);
        }
        m_ctl->setQueue({d + QStringLiteral("/a.wav"), d + QStringLiteral("/b.wav")}, d + QStringLiteral("/a.wav"));
        m_selfPos = m_ctl->gaplessSwitches(); m_selfLoads = m_ctl->loadCount();
    }, [this] { return m_ctl->path().endsWith(QLatin1String("a.wav")) && m_ctl->settled(); }, 1200);
    S("gapless: the second song takes over by itself, with no new load", [this] { m_selfLoads = m_ctl->loadCount(); m_mpv->seek(1.5, true); },
      [this] { const bool ok = m_ctl->path().endsWith(QLatin1String("b.wav")) && m_ctl->gaplessSwitches() > int(m_selfPos) && m_ctl->loadCount() == m_selfLoads; if (ok) QDir(Paths::tmpDir() + QStringLiteral("/gaptest")).removeRecursively(); return ok; }, 3500);
    auto idx = std::make_shared<int>(0);
    auto fails = std::make_shared<int>(0);
    auto tick = std::make_shared<std::function<void()>>();
    *tick = [=, this]() {
        if (*idx > 0) {
            const Step& prev = (*steps)[*idx - 1];
            const bool ok = prev.check();
            if (!ok) ++*fails;
            log(QStringLiteral("%1  %2   [%3]").arg(ok ? "PASS" : "FAIL", prev.name, debugState()));
        }
        if (*idx >= steps->size()) { log(QStringLiteral("DONE  %1 steps, %2 failed").arg(steps->size()).arg(*fails)); out->close(); QTimer::singleShot(200, qApp, &QApplication::quit); return; }
        const Step& s = (*steps)[*idx];
        ++*idx;
        s.run();
        QTimer::singleShot(s.waitMs, this, [tick] { (*tick)(); });
    };
    QTimer::singleShot(1800, this, [tick] { (*tick)(); });
}



// Renders the app's own screens to PNGs for the README (off-screen; uses no third-party media).
void MainWindow::runShots(const QString& dir) {
    m_shots = true;
    QDir().mkpath(dir);
    auto tabsOf = [this]() { auto* pg = findChild<SettingsPage*>(); return pg ? pg->findChild<Segmented*>() : nullptr; };
    auto snap = [this, dir](const QString& n) { grab().save(dir + "/" + n + ".png"); };
    auto page = [tabsOf](int i) { if (auto* t = tabsOf()) { t->setCurrent(i); emit t->currentChanged(i); } };
    struct S { int wait; std::function<void()> fn; };
    auto steps = std::make_shared<QVector<S>>();
    steps->push_back({1200, [this] {                                     // a populated library for the home pictures (when demo media is present)
        const QString videoDir = QFileInfo(m_ctl->path()).absolutePath();
        const QString musicDir = QDir(videoDir + "/../music").absolutePath();
        if (!QDir(musicDir).exists()) return;
        m_side->addPlace(videoDir); m_side->addPlace(musicDir); m_side->navigate(videoDir); setSidebarOpen(true, false);
    }});
    steps->push_back({800, [this] { Theme::i().setMode(Theme::Mode::Dark); }});
    steps->push_back({500, [snap] { snap("home-dark"); }});
    steps->push_back({100, [this] { Theme::i().setMode(Theme::Mode::Light); }});
    steps->push_back({500, [snap] { snap("home-light"); }});
    steps->push_back({100, [this] { doAction("settings"); }});
    steps->push_back({800, [snap] { snap("settings-light"); }});
    steps->push_back({100, [page] { page(1); }});
    steps->push_back({400, [snap] { snap("settings-playback-light"); }});
    steps->push_back({100, [page] { page(2); }});
    steps->push_back({600, [snap] { snap("settings-privacy-light"); }});
    steps->push_back({100, [page] { page(3); }});
    steps->push_back({400, [snap] { snap("settings-shortcuts-light"); }});
    steps->push_back({100, [page] { page(4); }});
    steps->push_back({400, [snap] { snap("settings-info-light"); }});
    steps->push_back({100, [page] { page(5); }});
    steps->push_back({400, [snap] { snap("about-light"); }});
    steps->push_back({100, [page] { page(6); }});
    steps->push_back({400, [snap] { snap("legal-light"); }});
    steps->push_back({100, [this, page] { Theme::i().setMode(Theme::Mode::Dark); page(5); }});
    steps->push_back({500, [snap] { snap("about-dark"); }});
    steps->push_back({100, [this] { for (auto* s : findChildren<Sheet*>()) s->closeSheet(); }});
    steps->push_back({700, [this] { doAction("palette"); }});
    steps->push_back({500, [snap] { snap("palette-dark"); }});
    // ---- skimming marks, music, playlists, queue (needs demo media: <demo>/video/*.avi and <demo>/music/...) ----
    {
        auto vids = std::make_shared<QStringList>();
        auto musicDirP = std::make_shared<QString>();
        steps->push_back({100, [this, vids, musicDirP] {
            doAction("escape"); Theme::i().setMode(Theme::Mode::Light); setSidebarOpen(true, false);
            const QString videoDir = QFileInfo(m_ctl->path()).absolutePath();
            *musicDirP = QDir(videoDir + "/../music").absolutePath();
            if (!QDir(*musicDirP).exists()) { musicDirP->clear(); return; }           // no demo media: only the basic shots
            for (const QFileInfo& fi : QDir(videoDir).entryInfoList({"*.avi"}, QDir::Files, QDir::Name)) *vids << fi.absoluteFilePath();
            m_side->addPlace(videoDir); m_side->addPlace(*musicDirP); m_side->navigate(videoDir);
            if (vids->size() >= 4) {
                m_ctl->toggleMark(vids->at(1), PlayerController::MarkKeep);
                m_ctl->toggleMark(vids->at(2), PlayerController::MarkReject);
                m_ctl->markSeen(vids->at(0), 0.45);
                m_ctl->markSeen(vids->at(3), 1.0);
            }
        }});
        steps->push_back({1500, [snap, musicDirP] { if (!musicDirP->isEmpty()) snap("library-marks-light"); }});
        steps->push_back({100, [this, musicDirP] { if (!musicDirP->isEmpty()) Theme::i().setMode(Theme::Mode::Dark); }});
        steps->push_back({500, [snap, musicDirP] { if (!musicDirP->isEmpty()) snap("library-marks-dark"); }});
        steps->push_back({150, [this] { m_toast->hide(); poke(); }});                       // the banner picture: no toast, controls shown
        steps->push_back({450, [snap] { snap("hero-dark"); }});
        steps->push_back({100, [this, musicDirP] { if (!musicDirP->isEmpty()) { Theme::i().setMode(Theme::Mode::Light); m_side->showTab(2); } }});
        steps->push_back({2500, [this, musicDirP] { if (!musicDirP->isEmpty()) (void)m_side->grab(); }});          // an off-screen window only paints when asked: this asks the covers to load
        steps->push_back({1500, [snap, musicDirP] { if (!musicDirP->isEmpty()) snap("music-artists-light"); }});
        steps->push_back({100, [this, musicDirP] { if (!musicDirP->isEmpty()) m_side->musicDrill(QStringLiteral("Mira Lowe"), QStringLiteral("Night Drive")); }});
        steps->push_back({300, [this, musicDirP] { if (!musicDirP->isEmpty()) (void)m_side->grab(); }});
        steps->push_back({1200, [snap, musicDirP] { if (!musicDirP->isEmpty()) snap("music-album-light"); }});
        steps->push_back({100, [this, musicDirP] {
            if (musicDirP->isEmpty()) return;
            QStringList l;
            for (const QString& a : {"Mira Lowe/Night Drive/01 - Headlights.wav", "The Quiet Hours/Long Way Home/02 - Porch Lights.wav", "Mira Lowe/Soft Light/03 - Linen.wav", "The Quiet Hours/Small Rooms/01 - Radiator.wav"}) l << *musicDirP + "/" + a;
            Playlists::save(QStringLiteral("Evening mix"), l);
            Playlists::save(QStringLiteral("Road trip"), l.mid(0, 2));
            m_side->showPlaylists();
        }});
        steps->push_back({500, [snap, musicDirP] { if (!musicDirP->isEmpty()) snap("playlists-light"); }});
        steps->push_back({100, [this, vids, musicDirP] { if (musicDirP->isEmpty()) return; if (!vids->isEmpty()) m_ctl->setQueue(*vids, vids->first()); m_side->showQueue(true); }});
        steps->push_back({900, [snap, musicDirP] { if (!musicDirP->isEmpty()) snap("queue-light"); }});
    }
    steps->push_back({100, [] { QApplication::quit(); }});
    auto idx = std::make_shared<int>(0);
    auto tick = std::make_shared<std::function<void()>>();
    *tick = [=, this]() {
        if (*idx >= steps->size()) return;
        const S s = (*steps)[*idx];
        ++*idx;
        QTimer::singleShot(s.wait, this, [=] { s.fn(); (*tick)(); });
    };
    (*tick)();
}