// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
#pragma once
#include <QObject>
#include <QString>
#include <QUrl>

class QNetworkAccessManager;
class QNetworkReply;
class QFile;
class QTimer;
class JsonStore;

// Self-update from GitHub Releases, entirely inside the app: one click (or none, in Automatic mode), then "Restart".
// Safe by design:
//  - only talks to api.github.com / github.com for this project's own repository
//  - only installs a file whose SHA-256 matches the checksum published with the release (checked again right before installing)
//  - sends nothing about the user (a plain GET with a generic User-Agent)
//  - never sends the user to a web page: when something fails the message stays in the app, with "Try again"
//
// How it feels:
//  - Automatic (default): check once a day (and every 6 hours while kinoz stays open), download in the background, verify,
//    then ask "Restart now / Later". If the answer is Later, the verified update is applied the next time kinoz starts.
//  - Notify me: check and tell the user; downloading and installing wait for a click on "Update".
//  - Off: never contacts the network on its own. "Check for updates" in Settings still works.
// Windows runs the new setup silently. Linux (AppImage) replaces its own file and relaunches, keeping the old one as ".previous".
// macOS replaces the app bundle from the downloaded disk image through a small helper script and reopens it.
//
// Robustness: a failed check is retried (1 h, then 6 h), a dropped download is retried and continued where it stopped,
// the GitHub API's rate limit is bypassed through the plain releases page, a version can be skipped, and every decision is
// written to the log (area "updater").
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
    bool canInstall() const { return m_assetUrl.isValid(); }          // a build for this system was found and has a checksum
    qint64 received() const { return m_received; }
    qint64 total() const { return m_total; }

    int mode() const;
    void setMode(int m);

    static QString current();
    static bool isNewer(const QString& candidate, const QString& installed);
    static QString stagingDir();
    // Empty when this copy of kinoz can replace itself; otherwise a short reason ("not running as an AppImage", "folder is read-only").
    static QString unsupportedReason();

    void checkNow(bool manual);
    void checkAutoIfDue();
    void startBackground();                                           // first due-check now, then every 6 hours while kinoz stays open
    void skipLatest();                                                // "don't remind me about this version"
    void download(bool automatic = false);
    bool installAndQuit();

    // Called once at start-up, before any window exists. If a verified update was left staged ("Later"),
    // starts applying it and returns true (the caller must then exit at once; the update reopens kinoz).
    // Gives up after two attempts so a broken update can never trap the user in a loop. dryRun: decide, but don't launch.
    static bool applyPendingOnStartup(JsonStore* settings, bool dryRun = false);

    // Small pure helpers, also used by the self-test.
    static bool isSkipped(JsonStore* settings, const QString& version);
    static QString tagFromLocation(const QUrl& releaseUrl);                       // ".../releases/tag/v1.2.3" -> "v1.2.3", or empty for anything else
    static bool shouldRetryDownload(int httpStatus, int networkError, int tries); // only dropped connections and 5xx are worth another go
    static QString assetNameFor(const QString& version);                          // this platform's file name for a version

signals:
    void stateChanged();
    void progress(qint64 received, qint64 total);
    void updateFound(const QString& version);                   // automatic checks, when the user must act
    void readyToInstall(const QString& version, bool automatic); // downloaded and verified: ask "restart now / later"

private:
    void setState(State s, const QString& err = {}, const QString& info = {});
    void onChecked(QNetworkReply* r);
    void checkViaRedirect();                                    // when the API refuses (rate limit): read the tag from github.com/.../releases/latest
    void checkFailed(const QString& why);
    void afterAssetChosen(const QString& shaUrl);
    void finishAvailable();
    void startDownload();
    void onDownloaded();
    void failDownload(const QString& why);
    void stage();
    static void discardStaged();
    static bool urlAllowed(const QUrl& u);
    static bool launchApply(const QString& file);               // platform specific: hand over to the new version
    QNetworkAccessManager* net();

    JsonStore* m_set;
    QNetworkAccessManager* m_net = nullptr;
    QNetworkReply* m_reply = nullptr;
    State m_state = Idle;
    bool m_manual = false, m_auto = false;
    QString m_latest, m_notes, m_error, m_info, m_assetName, m_sha;
    QUrl m_assetUrl, m_page;
    qint64 m_received = 0, m_total = 0;
    QString m_file;
    QFile* m_out = nullptr;
    QTimer* m_retry = nullptr;            // next attempt after a failed check
    QTimer* m_dlRetry = nullptr;          // next attempt after a dropped download
    QTimer* m_periodic = nullptr;
    int m_checkFails = 0, m_dlTries = 0;
    qint64 m_assetSize = 0, m_resumeFrom = 0;
    bool m_noResume = false, m_statusChecked = false;
};
