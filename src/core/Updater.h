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
//  - only installs a file whose SHA-256 matches the checksum published with the release
//  - never runs anything unless the user presses the button
//  - sends nothing about the user (a plain GET with a generic User-Agent)
class Updater : public QObject {
    Q_OBJECT
public:
    enum State { Idle, Checking, UpToDate, Available, Downloading, Ready, Failed };

    explicit Updater(JsonStore* settings, QObject* parent = nullptr);

    State state() const { return m_state; }
    QString latest() const { return m_latest; }
    QString notes() const { return m_notes; }
    QString error() const { return m_error; }
    QString info() const { return m_info; }
    QUrl releasePage() const { return m_page; }
    qint64 received() const { return m_received; }
    qint64 total() const { return m_total; }

    static QString current();
    static bool isNewer(const QString& candidate, const QString& installed);

    void checkNow(bool manual);
    void checkAutoIfDue();
    void download();
    bool installAndQuit();

signals:
    void stateChanged();
    void progress(qint64 received, qint64 total);
    void updateFound(const QString& version);     // automatic checks only

private:
    void setState(State s, const QString& err = {}, const QString& info = {});
    void onChecked(QNetworkReply* r);
    void fetchChecksum();
    void finishAvailable();
    void onDownloaded();
    static bool urlAllowed(const QUrl& u);
    QNetworkAccessManager* net();

    JsonStore* m_set;
    QNetworkAccessManager* m_net = nullptr;
    QNetworkReply* m_reply = nullptr;
    State m_state = Idle;
    bool m_manual = false;
    QString m_latest, m_notes, m_error, m_info, m_assetName, m_sha;
    QUrl m_assetUrl, m_page;
    qint64 m_received = 0, m_total = 0;
    QString m_file;
    QFile* m_out = nullptr;
};
