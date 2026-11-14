// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include "core/SessionHandoff.h"
#include "media/MediaKind.h"
#include <QHash>
#include <QTimer>
#include <QElapsedTimer>
#include <QVariantAnimation>
#include <functional>
#include <QWidget>

class MpvPlayer;
class PlayerController;
class JsonStore;
class VideoWidget;
class ControlBar;
class TopBar;
class Toast;
class EmptyState;
class AudioStage;
class Sidebar;
class SidePanel;
class CommandPalette;
class Annotator;
class PreviewTip;
class Smtc;
class PreviewService;
class ThumbService;
class LibraryIndex;
class Updater;
class FadeCover;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(JsonStore* settings, JsonStore* resume);
    ~MainWindow() override;
    void openPaths(const QStringList& paths);
    void doAction(const QString& id);
    bool isFull() const { return m_full; }
    // test hooks
    QString debugState() const;
    void runSelfTest();
    void runBench();
    void notifySafeMode() { notify(tr("kinoz ended unexpectedly the last two times, so it started in safe mode (software decoding, no glass). Change this in Settings."), true); }
    void runShots(const QString& dir);

protected:
    bool nativeEvent(const QByteArray&, void* msg, qintptr* result) override;
    void showEvent(QShowEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void closeEvent(QCloseEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void dragEnterEvent(QDragEnterEvent*) override;
    void dropEvent(QDropEvent*) override;
    bool eventFilter(QObject*, QEvent*) override;
    void changeEvent(QEvent*) override;
    void paintEvent(QPaintEvent*) override;

private:
    void buildUi();
    void layoutAll();
    void wireController();
    void loadKeymap();
    void refreshChrome();
    void showMoreMenu(const QPoint& anchorGlobal = QPoint(-1, -1));
    void showTracksMenu();
    void showSpeedMenu();
    void toggleFullscreen();
    void enterFullscreen();
    void exitFullscreen();
    void fitFullscreenToMonitor();           // Windows: re-fit after the display setup changes
    void updateSmtc();                       // Windows media overlay (volume flyout / lock screen)
    Smtc* m_smtc = nullptr;
    QTimer m_smtcTimer;
    void toggleMini();
    void togglePin();
    void setSidebarOpen(bool open, bool animate = true, bool automatic = false);
    bool m_selfOk = false;                     // self-test scratch
    int m_selfLoads = 0;
    double m_selfPos = 0;                      // self-test scratch
    QString m_selfPath;                        // self-test scratch
    bool m_muteRestored = false;               // the saved mute state has been applied; from here on changes are remembered
    bool m_shots = false;                     // screenshot run: keep the library open
    bool m_sideAuto = false;                 // the library was hidden by the idle fade (it comes back at the left edge)
    void togglePanel(const QString& which);
    void closePanel();
    void poke();
    void idleTick();
    void saveGeometryNow();
    void applyAppPalette();
    void screenshotFrame();
    void cycleTheme();
    void updateGlass();
    void openSettings();
    void privacyAction(const QString& id);
    void cleanIdentity(bool announce);
    bool saveHandoff();                        // before an update restart: remember the open file, second and queue (false when privacy says no)
    void restoreHandoff();                     // first start after an update: go back there
    void wipeLogs();
    void confirm(const QString& title, const QString& body, const QString& okText, std::function<void()> onOk, bool danger = true, const QString& cancelText = {});
    void promptText(const QString& title, const QString& label, const QString& text, const QString& okText, std::function<void(const QString&)> onOk);
    void savePlaylist();
    void renamePlaylist(const QString& name);
    void deletePlaylist(const QString& name);
    void trashCurrent();
    void trashRejected();
    void trashFiles(const QStringList& files);
    void openPicker(bool folder);
    void startAnnotate();
    void notify(const QString& text, bool error = false);
    bool captionInSidebar(const QPoint& p) const;
    bool pointerOverChrome() const;
    void applyDwmFrame(bool on);
    QString diagnosticsText() const;
    void applySetting(const QString& key);
    void applyHideDelay();
    void restartSlideshow();
    void setSleepTimer(int minutes);
    void showSleepMenu();
    void showAspectMenu();
    void setTopmost(bool on);

    QStringList m_pendingOpen;
    Handoff::Session m_handoff;                // what the previous version was doing when it restarted for an update
    QRect m_normalGeo;
    JsonStore* m_set;
    JsonStore* m_resume;
    MpvPlayer* m_mpv = nullptr;
    PlayerController* m_ctl = nullptr;
    VideoWidget* m_video = nullptr;
    QWidget* m_content = nullptr;
    ControlBar* m_bar = nullptr;
    TopBar* m_top = nullptr;
    Toast* m_toast = nullptr;
    EmptyState* m_empty = nullptr;
    AudioStage* m_audio = nullptr;
    Sidebar* m_side = nullptr;
    SidePanel* m_panel = nullptr;
    CommandPalette* m_palette = nullptr;
    Annotator* m_annot = nullptr;
    PreviewTip* m_tip = nullptr;
    PreviewService* m_preview = nullptr;
    ThumbService* m_thumbs = nullptr;
    LibraryIndex* m_index = nullptr;
    Updater* m_updater = nullptr;

    QHash<QString, QString> m_keys;
    QTimer m_idle, m_stateTimer, m_geoTimer;
    QVariantAnimation m_sideAnim;
    bool m_sidebarOpen = true;
    int m_sidebarW = 320;
    qreal m_sideT = 1.0;          // 0 = hidden, 1 = fully open
    bool m_mini = false, m_pinned = false, m_full = false;
    QRect m_restoreGeo;
    bool m_restoreSidebar = true;
    bool m_wasMax = false;
    bool m_glassOn = false;
    bool m_vidOff = false;
    bool m_closing = false;
    double m_seekRef = 0;
    FadeCover* m_cover = nullptr;
    QTimer m_slide, m_sleep;
    QElapsedTimer m_fsGuard;                 // debounce for fullscreen toggles
    quint64 m_fsStyle = 0;
    int m_fsRestoreX = 0, m_fsRestoreY = 0, m_fsRestoreW = 0, m_fsRestoreH = 0;
    QPoint m_lastMouse;
    QString m_previewPath;
    bool m_selfTest = false;
};
