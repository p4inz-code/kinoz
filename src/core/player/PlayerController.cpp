#include "core/player/PlayerController.h"
#include "core/JsonStore.h"
#include "core/player/MpvPlayer.h"
#include <QCollator>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFutureWatcher>
#include <QJsonArray>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QtConcurrent>
#include <cmath>

namespace {
QStringList listMedia(const QString& dir, int family) {
    QDir d(dir);
    QStringList out;
    for (const QFileInfo& fi : d.entryInfoList(QDir::Files | QDir::Readable, QDir::NoSort)) {
        const MediaKind k = Media::kindFor(fi.fileName());
        if (k == MediaKind::Unknown) continue;
        if (family && Media::family(k) != family) continue;
        out << fi.absoluteFilePath();
    }
    QCollator col;
    col.setNumericMode(true);
    col.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(out.begin(), out.end(), [&](const QString& a, const QString& b) { return col.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0; });
    return out;
}
}

PlayerController::PlayerController(MpvPlayer* mpv, JsonStore* settings, JsonStore* resume, QObject* parent)
    : QObject(parent), m_mpv(mpv), m_set(settings), m_resume(resume) {
    m_shuffle = m_set->flag("shuffle", false);
    m_repeat = Repeat(int(m_set->num("repeat", 0)));
    connect(m_mpv, &MpvPlayer::eofReached, this, &PlayerController::onEof);
    connect(m_mpv, &MpvPlayer::fileLoaded, this, &PlayerController::onFileLoaded);
    connect(m_mpv, &MpvPlayer::pauseChanged, this, &PlayerController::onPause);
    connect(m_mpv, &MpvPlayer::fileEnded, this, &PlayerController::onEnded);
    m_resumeTimer.setInterval(5000);
    connect(&m_resumeTimer, &QTimer::timeout, this, &PlayerController::saveResumeNow);
    m_resumeTimer.start();
    connect(&m_queue, &QAbstractItemModel::rowsInserted, this, &PlayerController::queueChanged);
    connect(&m_queue, &QAbstractItemModel::rowsRemoved, this, &PlayerController::queueChanged);
    connect(&m_queue, &QAbstractItemModel::modelReset, this, &PlayerController::queueChanged);
    for (const auto& c : {"brightness", "contrast", "saturation", "gamma", "hue"}) m_color[c] = int(m_set->num(QStringLiteral("color.") + c, 0));
}

QString PlayerController::key(const QString& p) const { return QDir::fromNativeSeparators(p).toLower(); }

QString PlayerController::title() const {
    const QString t = m_mpv->metadata().value("title").toString();
    if (!t.isEmpty()) return t;
    if (m_path.isEmpty()) return {};
    return QFileInfo(m_path).completeBaseName();
}
QString PlayerController::artist() const { return m_mpv->metadata().value("artist").toString(); }
QString PlayerController::album() const { return m_mpv->metadata().value("album").toString(); }

// ---------- opening ----------
void PlayerController::open(const QStringList& paths) {
    QStringList files, dirs;
    for (const QString& p : paths) {
        QFileInfo fi(p);
        if (fi.isDir()) dirs << fi.absoluteFilePath();
        else if (Media::isMedia(p)) files << fi.absoluteFilePath();
    }
    if (files.size() == 1 && dirs.isEmpty()) { play(files.first()); return; }
    if (!files.isEmpty()) { setQueue(files, files.first()); return; }
    if (!dirs.isEmpty()) {
        const quint64 gen = ++m_gen;
        auto* w = new QFutureWatcher<QStringList>(this);
        const QString dir = dirs.first();
        connect(w, &QFutureWatcher<QStringList>::finished, this, [this, w, gen] {
            const QStringList l = w->result();
            w->deleteLater();
            if (gen != m_gen || l.isEmpty()) { if (l.isEmpty()) emit message(tr("No playable files in that folder")); return; }
            setQueue(l, l.first());
        });
        w->setFuture(QtConcurrent::run([dir] { return listMedia(dir, 0); }));
    }
}

void PlayerController::setQueue(const QStringList& paths, const QString& playPath) {
    m_queue.setStringList(paths);
    m_history.clear();
    play(playPath);
}

void PlayerController::play(const QString& path) {
    const bool inQueue = m_queue.stringList().contains(path);
    load(path);
    if (!inQueue) {
        if (m_set->flag("siblings", true)) buildQueueFromSiblings(path, m_gen);
        else m_queue.setStringList({path});
    }
}

void PlayerController::onEnded(bool, bool err, const QString& reason) {
    if (!err) { m_failStreak = 0; return; }
    const QString name = QFileInfo(m_path).fileName();
    emit error(tr("Can't play “%1” — %2").arg(name, reason.isEmpty() ? tr("unsupported or damaged file") : reason));
    if (++m_failStreak <= 3) {
        const int i = pickNext(+1);
        if (i >= 0 && m_queue.stringList().value(i) != m_path) QTimer::singleShot(700, this, [this, i] { playIndex(i); });
    } else m_failStreak = 0;
}

void PlayerController::buildQueueFromSiblings(const QString& path, quint64 gen) {
    const int fam = Media::family(Media::kindFor(path));
    auto* w = new QFutureWatcher<QStringList>(this);
    connect(w, &QFutureWatcher<QStringList>::finished, this, [this, w, gen, path] {
        const QStringList l = w->result();
        w->deleteLater();
        if (gen != m_gen || m_path != path || l.isEmpty()) return;
        m_queue.setStringList(l);
        emit pathChanged();
    });
    w->setFuture(QtConcurrent::run([path, fam] { return listMedia(QFileInfo(path).absolutePath(), fam); }));
}

void PlayerController::playIndex(int i) {
    const QStringList l = m_queue.stringList();
    if (i >= 0 && i < l.size()) load(l[i]);
}

void PlayerController::enqueue(const QStringList& paths) {
    QStringList l = m_queue.stringList();
    for (const QString& p : paths)
        if (Media::isMedia(p) && !l.contains(p)) l << p;
    m_queue.setStringList(l);
}
void PlayerController::removeAt(int i) { QStringList l = m_queue.stringList(); if (i >= 0 && i < l.size()) { l.removeAt(i); m_queue.setStringList(l); } }
void PlayerController::clearQueue() { m_queue.setStringList({}); }

void PlayerController::load(const QString& path) {
    saveResumeNow();
    if (!QFileInfo::exists(path)) {
        emit error(tr("“%1” was moved or deleted").arg(QFileInfo(path).fileName()));
        QStringList l = m_queue.stringList();
        const int i = l.indexOf(path);
        if (i >= 0) { l.removeAt(i); m_queue.setStringList(l); if (!l.isEmpty() && ++m_failStreak <= 3) { load(l.value(qMin(i, int(l.size()) - 1))); return; } }
        m_failStreak = 0;
        return;
    }
    if (Media::kindFor(path) == MediaKind::Unknown) { emit error(tr("That isn't a media file I can open")); return; }
    ++m_gen;
    m_path = path;
    m_kind = Media::kindFor(path);
    if (m_history.isEmpty() || m_history.last() != path) m_history << path;
    if (m_history.size() > 200) m_history.removeFirst();
    double start = -1;
    if ((m_kind == MediaKind::Video || m_kind == MediaKind::Audio) && m_set->flag("resume", true)) {
        const QJsonObject r = m_resume->get(key(path)).toObject();
        const double pos = r.value("pos").toDouble(), dur = r.value("dur").toDouble();
        if (pos > 10 && (dur <= 0 || pos < dur - 12)) start = pos;
    }
    resetView();
    m_mpv->setProp("pause", false);
    m_mpv->loadFile(path, start, m_kind == MediaKind::Gif);
    addRecent(path);
    emit pathChanged();
}

void PlayerController::onFileLoaded() {
    applyEq();
    for (auto it = m_color.begin(); it != m_color.end(); ++it) m_mpv->setPropInt(it.key().toUtf8().constData(), it.value());
    if (m_kind == MediaKind::Video || m_kind == MediaKind::Audio) {
        const double ds = m_set->num("defaultSpeed", 1.0);
        if (ds >= 0.25 && ds <= 4.0) m_mpv->setSpeed(ds);
    }
    m_failStreak = 0;
    if (m_set->flag("normalize", false)) m_mpv->setProp("replaygain", QStringLiteral("track"));
    else m_mpv->setProp("replaygain", QStringLiteral("no"));
    emit pathChanged();
}

// ---------- transport ----------
void PlayerController::togglePause() {
    if (!hasMedia()) return;
    if (m_mpv->eof() && (m_kind == MediaKind::Video || m_kind == MediaKind::Audio)) { m_mpv->seek(0, true); m_mpv->setPause(false); return; }
    m_mpv->setPause(!m_mpv->paused());
}
void PlayerController::pause() { m_mpv->setPause(true); }
void PlayerController::stop() {
    saveResumeNow();
    m_mpv->stopPlayback();
    m_path.clear(); m_kind = MediaKind::Unknown;
    emit pathChanged();
}

int PlayerController::pickNext(int dir) {
    const QStringList l = m_queue.stringList();
    if (l.isEmpty()) return -1;
    const int cur = l.indexOf(m_path);
    if (m_shuffle && l.size() > 1) {
        if (dir < 0) {
            if (m_history.size() >= 2) { const int i = l.indexOf(m_history.at(m_history.size() - 2)); if (i >= 0) { m_history.removeLast(); m_history.removeLast(); return i; } }
            return cur;
        }
        int i; do { i = int(QRandomGenerator::global()->bounded(l.size())); } while (i == cur);
        return i;
    }
    int n = cur + dir;
    if (n < 0) return m_repeat == RepeatAll ? l.size() - 1 : -1;
    if (n >= l.size()) return m_repeat == RepeatAll ? 0 : -1;
    return n;
}

void PlayerController::next() {
    const int i = pickNext(+1);
    if (i >= 0) playIndex(i);
}
void PlayerController::prev() {
    if ((m_kind == MediaKind::Video || m_kind == MediaKind::Audio) && m_mpv->position() > 3.0) { m_mpv->seek(0, true); return; }
    const int i = pickNext(-1);
    if (i >= 0) playIndex(i);
}

void PlayerController::onEof() {
    if (m_kind != MediaKind::Video && m_kind != MediaKind::Audio) return;
    if (m_repeat == RepeatOne) { m_mpv->seek(0, true); m_mpv->setPause(false); return; }
    if (m_stopAfterCurrent) { m_stopAfterCurrent = false; m_mpv->setPause(true); emit message(tr("Sleep timer: stopped at the end of the file")); return; }
    if (!m_set->flag("autoplayNext", true)) return;
    const int i = pickNext(+1);
    if (i >= 0) playIndex(i);
    else {
        QJsonObject o; m_resume->remove(key(m_path));   // finished: forget resume point
    }
}

void PlayerController::onPause(bool) { saveResumeNow(); }

void PlayerController::seekFrac(double f, bool exact) {
    const double d = m_mpv->duration();
    if (d > 0) m_mpv->seek(qBound(0.0, f, 1.0) * d, exact);
}
void PlayerController::seekBy(double sec) { m_mpv->seekRelative(sec); }
void PlayerController::setShuffle(bool s) { m_shuffle = s; m_set->set("shuffle", s); emit modeChanged(); }
void PlayerController::cycleRepeat() { m_repeat = Repeat((m_repeat + 1) % 3); m_set->set("repeat", int(m_repeat)); emit modeChanged(); }
void PlayerController::setSpeed(double s) { s = qBound(0.25, s, 4.0); m_mpv->setSpeed(std::round(s * 100) / 100); emit message(tr("Speed %1×").arg(s, 0, 'g', 3)); }
void PlayerController::speedStep(int dir) {
    static const double steps[] = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5, 1.75, 2.0, 2.5, 3.0, 4.0};
    const double cur = m_mpv->speed();
    int idx = 0;
    for (int i = 0; i < 11; ++i) if (steps[i] <= cur + 0.001) idx = i;
    idx = qBound(0, idx + dir, 10);
    setSpeed(steps[idx]);
}
void PlayerController::setVolume(double v) { v = qBound(0.0, v, 130.0); m_mpv->setVolume(v); m_mpv->setMute(false); m_set->set("volume", v); }
void PlayerController::volumeStep(int dir) {
    const int step = qBound(1, int(m_set->num("volStep", 5)), 20);
    setVolume(m_mpv->volume() + step * dir);
    emit message(tr("Volume %1%").arg(int(m_mpv->volume())));
}
void PlayerController::toggleMute() { m_mpv->setMute(!m_mpv->muted()); }

// ---------- picture view ----------
void PlayerController::zoomBy(double steps) {
    m_zoom = qBound(-1.0, m_zoom + steps * 0.25, 6.0);
    m_mpv->setProp("video-zoom", m_zoom);
    if (m_zoom <= 0.001) { m_panX = m_panY = 0; m_mpv->setProp("video-pan-x", 0.0); m_mpv->setProp("video-pan-y", 0.0); }
    emit viewChanged();
}
void PlayerController::panByPixels(const QPoint& d, const QSizeF& view, qreal dpr) {
    const int dw = m_mpv->dispW(), dh = m_mpv->dispH();
    if (dw <= 0 || dh <= 0 || m_zoom <= 0.001) return;
    const double scale = qMin(view.width() * dpr / dw, view.height() * dpr / dh) * std::pow(2.0, m_zoom);
    m_panX += d.x() * dpr / (dw * scale);
    m_panY += d.y() * dpr / (dh * scale);
    m_mpv->setProp("video-pan-x", m_panX);
    m_mpv->setProp("video-pan-y", m_panY);
}
void PlayerController::resetView() {
    m_zoom = m_panX = m_panY = 0; m_rot = 0;
    m_mpv->setProp("video-zoom", 0.0); m_mpv->setProp("video-pan-x", 0.0); m_mpv->setProp("video-pan-y", 0.0); m_mpv->setPropInt("video-rotate", 0);
    emit viewChanged();
}
void PlayerController::rotate(int deg) { m_rot = ((m_rot + deg) % 360 + 360) % 360; m_mpv->setPropInt("video-rotate", m_rot); emit viewChanged(); }
void PlayerController::fit100() {
    const int w = m_mpv->dispW(), h = m_mpv->dispH();
    if (w <= 0) return;
    m_zoom = 0.001; // actual-size is resolved by mpv: scale 1:1 using video-unscaled
    m_mpv->setProp("video-unscaled", QStringLiteral("yes"));
    emit viewChanged();
}

// ---------- tools ----------
void PlayerController::setColor(const QString& prop, int v) {
    v = qBound(-100, v, 100);
    m_color[prop] = v;
    m_set->set(QStringLiteral("color.") + prop, v);
    m_mpv->setPropInt(prop.toUtf8().constData(), v);
}
void PlayerController::resetColor() { for (const auto& c : {"brightness", "contrast", "saturation", "gamma", "hue"}) setColor(QString::fromLatin1(c), 0); }

void PlayerController::applyEq() {
    static const int freqs[10] = {31, 62, 125, 250, 500, 1000, 2000, 4000, 8000, 16000};
    const bool on = m_set->flag("eq.enabled", false);
    QStringList filters;
    if (on) {
        const QJsonArray bands = m_set->get("eq.bands").toArray();
        const double pre = m_set->num("eq.preamp", 0);
        if (std::abs(pre) > 0.05) filters << QStringLiteral("volume=%1dB").arg(pre, 0, 'f', 1);
        for (int i = 0; i < 10; ++i) {
            const double g = i < bands.size() ? bands[i].toDouble() : 0.0;
            if (std::abs(g) > 0.05) filters << QStringLiteral("equalizer=f=%1:t=o:w=1.2:g=%2").arg(freqs[i]).arg(g, 0, 'f', 1);
        }
    }
    if (m_set->flag("normalize", false)) filters << QStringLiteral("dynaudnorm=f=250:g=15:p=0.9");
    if (filters.isEmpty()) m_mpv->command({QStringLiteral("af"), QStringLiteral("clr"), QString()});
    else m_mpv->setProp("af", QStringLiteral("lavfi=[%1]").arg(filters.join(',')));
}

void PlayerController::setSubDelay(double d) { m_mpv->setProp("sub-delay", d); emit message(tr("Subtitle delay %1 s").arg(d, 0, 'f', 1)); }
void PlayerController::setSubScale(double s) { m_mpv->setProp("sub-scale", s); }
void PlayerController::selectTrack(const QString& type, int id) {
    const char* p = type == "audio" ? "aid" : type == "sub" ? "sid" : "vid";
    if (id <= 0) m_mpv->setProp(p, QStringLiteral("no")); else m_mpv->setPropInt(p, id);
}
void PlayerController::loadSubtitleFile(const QString& p) { m_mpv->command({QStringLiteral("sub-add"), p, QStringLiteral("select")}); }

// ---------- history ----------
void PlayerController::addRecent(const QString& p) {
    QJsonArray a = m_set->get("recent").toArray();
    for (int i = a.size() - 1; i >= 0; --i) if (a[i].toString() == p) a.removeAt(i);
    a.prepend(p);
    while (a.size() > 40) a.removeLast();
    m_set->set("recent", a);
}
QStringList PlayerController::recent() const {
    QStringList l;
    for (const auto& v : m_set->get("recent").toArray()) l << v.toString();
    return l;
}
void PlayerController::saveResumeNow() {
    if (m_path.isEmpty() || (m_kind != MediaKind::Video && m_kind != MediaKind::Audio)) return;
    const double pos = m_mpv->position(), dur = m_mpv->duration();
    if (dur < 30) return;
    if (pos < 10 || pos > dur - 12) { m_resume->remove(key(m_path)); return; }
    QJsonObject o; o["pos"] = pos; o["dur"] = dur; o["t"] = double(QDateTime::currentSecsSinceEpoch());
    m_resume->set(key(m_path), o);
}
