#pragma once
#include <QImage>
#include <QList>
#include <QMutex>
#include <QObject>
#include <QThread>
#include <QVector>
#include <QWaitCondition>

// Seek-bar hover preview.
//  * exact path: one lazily created decoder, newest request wins, destroyed after idle.
//  * storyboard: on the first hover over a long video a second, low-priority decoder builds a strip of frames
//    (coarse first, then finer) kept in memory. Moving the pointer then shows the nearest strip frame at once
//    and the exact frame replaces it a moment later, so the preview never feels like it is waiting.
class PreviewService : public QObject {
    Q_OBJECT
public:
    explicit PreviewService(QObject* parent = nullptr);
    ~PreviewService() override;
    void request(const QString& path, double sec, int maxW, double duration = 0);
    void cancel();
    int boardFrames(const QString& path) const;     // how many strip frames are ready (diagnostics / tests)
signals:
    void frame(const QImage& img, double sec, const QString& path);
private:
    struct Board { QString path; double dur = 0; QVector<QImage> img; int done = 0; };
    static constexpr int kSlots = 48;
    void loop();
    void boardLoop();
    Board* findBoard(const QString& path);          // callers hold m_mx
    QMutex m_mx;
    QWaitCondition m_cv, m_cvBoard;
    QString m_path;
    double m_sec = 0;
    int m_w = 240;
    bool m_has = false, m_quit = false;
    QList<Board> m_boards;                          // newest last, at most 4 kept
    QString m_boardPath;
    double m_boardDur = 0;
    bool m_wantBoard = false;
    QThread* m_thread = nullptr;
    QThread* m_boardThread = nullptr;
};
