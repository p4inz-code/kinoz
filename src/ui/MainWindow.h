#pragma once
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
    void showMoreMenu();
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
    void setSidebarOpen(bool open, bool animate = true);
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
    void confirm(const QString& title, const QString& body, const QString& okText, std::function<void()> onOk, bool danger = true);
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
