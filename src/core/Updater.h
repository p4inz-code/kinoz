#pragma once
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class JsonStore;

// Self-update from GitHub Releases. Safe by design:
//  - only talks to api.github.com / github.com for this project's own repository
//  - only installs a file whose SHA-256 matches the checksum published with the release (checked again right before installing)
//  - sends nothing about the user (a plain GET with a generic User-Agent)
//
// How it feels (Windows):
//  - Automatic (default): check once a day, download in the background, verify, then ask "Restart now / Later".
//    If the answer is Later (or nobody answers), the verified installer is applied the next time kinoz starts.
//  - Notify me: check once a day and tell the user; downloading and installing wait for a click.
//  - Off: never contacts the network on its own. "Check for updates" in Settings still works.
// macOS and Linux open the release page instead (the user installs the file themselves).
class Updater : public QObject {
    Q_OBJECT
public:
    enum State { Idle, Checking, UpToDate, Available, Downloading, Ready, Failed };
    enum Mode { Automatic = 0, Notify = 1, Off = 2 };

    explicit Updater(JsonStore* settings, QObject* parent = nullptr);

    State state() const { return m_state; }
    QString latest() const { return m_latest; }
    QString notes() const { return m_notes; }
    QString error() const { return m_error; }
    QString info() const { return m_info; }
    QUrl releasePage() const { return m_page; }
    QUrl directDownload() const { return m_directUrl; }    // this platform's installer, valid after a check found a newer version
    qint64 received() const { return m_received; }
    qint64 total() const { return m_total; }

    int mode() const;
    void setMode(int m);

    static QString current();
    static bool isNewer(const QString& candidate, const QString& installed);
    static QString stagingDir();

    void checkNow(bool manual);
    void checkAutoIfDue();
    void download(bool automatic = false);
    bool installAndQuit();

    // Called once at start-up, before any window exists. If a verified update was left staged ("Later"),
    // starts its installer and returns true (the caller must then exit at once; the installer reopens kinoz).
    // Gives up after two attempts so a broken installer can never trap the user in a loop. dryRun: decide, but don't launch.
    static bool applyPendingOnStartup(JsonStore* settings, bool dryRun = false);

signals:
    void stateChanged();
    void progress(qint64 received, qint64 total);
    void updateFound(const QString& version);                   // automatic checks, when the user must act
    void readyToInstall(const QString& version, bool automatic); // downloaded and verified: ask "restart now / later"

private:
    void setState(State s, const QString& err = {}, const QString& info = {});
    void onChecked(QNetworkReply* r);
    void fetchChecksum();
    void finishAvailable();
    void onDownloaded();
    void stage();
    static void discardStaged();
    static bool urlAllowed(const QUrl& u);
    QNetworkAccessManager* net();

    JsonStore* m_set;
    QNetworkAccessManager* m_net = nullptr;
    QNetworkReply* m_reply = nullptr;
    State m_state = Idle;
    bool m_manual = false, m_auto = false;
    QString m_latest, m_notes, m_error, m_info, m_assetName, m_sha;
    QUrl m_assetUrl, m_page, m_directUrl;
    qint64 m_received = 0, m_total = 0;
    QString m_file;
    QFile* m_out = nullptr;
};
