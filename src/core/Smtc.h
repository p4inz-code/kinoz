#pragma once
#include <QObject>
#include <QString>
#include <memory>

class QWidget;

// Windows "System Media Transport Controls": the now-playing card in the volume flyout and on the lock screen,
// with play / pause / next / previous / seek that reach kinoz. Does nothing on other systems (they have MPRIS / Now Playing).
class Smtc : public QObject {
    Q_OBJECT
public:
    struct State { QString title, artist, album; bool video = false, hasMedia = false, playing = false; double pos = 0, dur = 0; };
    explicit Smtc(QWidget* window, QObject* parent = nullptr);
    ~Smtc() override;
    bool ok() const;
    void update(const State& s);
signals:
    void playRequested();
    void pauseRequested();
    void nextRequested();
    void previousRequested();
    void stopRequested();
    void seekRequested(double sec);
private:
    struct Impl;
    std::unique_ptr<Impl> d;
};
