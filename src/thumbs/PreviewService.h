#pragma once
#include <QImage>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QWaitCondition>

// Seek-bar hover preview: one lazily created decoder, newest request wins, destroyed after idle.
class PreviewService : public QObject {
    Q_OBJECT
public:
    explicit PreviewService(QObject* parent = nullptr);
    ~PreviewService() override;
    void request(const QString& path, double sec, int maxW);
    void cancel();
signals:
    void frame(const QImage& img, double sec, const QString& path);
private:
    void loop();
    QMutex m_mx;
    QWaitCondition m_cv;
    QString m_path;
    double m_sec = 0;
    int m_w = 240;
    bool m_has = false, m_quit = false;
    QThread* m_thread = nullptr;
};
