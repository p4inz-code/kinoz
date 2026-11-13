// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
// The WinRT headers must come before Qt's (Qt defines keyword macros such as `emit`).
#ifdef _WIN32
#include <unknwn.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Media.h>
#include <systemmediatransportcontrolsinterop.h>
#endif

#include "core/Smtc.h"
#include <QMetaObject>
#include <QWidget>
#include <chrono>

#if defined(__linux__) && defined(KINOZ_HAS_DBUS)
// ---- Linux: MPRIS over the session bus (media keys, the GNOME / KDE now-playing card, playerctl) ----
#include "core/MprisAdaptors.h"
#include <QDBusConnection>
#include <QDBusMessage>
#include <QCoreApplication>

void MprisRoot::Raise() { if (!m_s->window) return; m_s->window->showNormal(); m_s->window->raise(); m_s->window->activateWindow(); }
void MprisPlayer::Next() { emit m_s->owner->nextRequested(); }
void MprisPlayer::Previous() { emit m_s->owner->previousRequested(); }
void MprisPlayer::Pause() { emit m_s->owner->pauseRequested(); }
void MprisPlayer::Play() { emit m_s->owner->playRequested(); }
void MprisPlayer::PlayPause() { if (m_s->status == QLatin1String("Playing")) emit m_s->owner->pauseRequested(); else emit m_s->owner->playRequested(); }
void MprisPlayer::Stop() { emit m_s->owner->stopRequested(); }
void MprisPlayer::Seek(qlonglong offsetUs) { emit m_s->owner->seekRequested(qMax(0.0, double(m_s->posUs + offsetUs) / 1e6)); }
void MprisPlayer::SetPosition(const QDBusObjectPath&, qlonglong posUs) { emit m_s->owner->seekRequested(qMax(0.0, double(posUs) / 1e6)); }

struct Smtc::Impl {
    MprisShared sh;
    bool registered = false;
    QString lastStatus;
    QVariantMap lastMeta;
};
Smtc::Smtc(QWidget* window, QObject* parent) : QObject(parent), d(new Impl) {
    d->sh.owner = this; d->sh.window = window;
    QDBusConnection bus = QDBusConnection::sessionBus();
    if (!bus.isConnected()) return;                                             // no session bus (SSH, minimal setups): quietly skip
    new MprisRoot(&d->sh, this);
    new MprisPlayer(&d->sh, this);
    QString name = QStringLiteral("org.mpris.MediaPlayer2.kinoz");
    if (!bus.registerService(name)) {                                           // a second kinoz window gets its own name
        name += QStringLiteral(".instance%1").arg(QCoreApplication::applicationPid());
        if (!bus.registerService(name)) return;
    }
    d->registered = bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this, QDBusConnection::ExportAdaptors);
}
Smtc::~Smtc() = default;
bool Smtc::ok() const { return d->registered; }
void Smtc::update(const State& s) {
    if (!d->registered) return;
    MprisShared& sh = d->sh;
    sh.hasMedia = s.hasMedia;
    sh.status = !s.hasMedia ? QStringLiteral("Stopped") : s.playing ? QStringLiteral("Playing") : QStringLiteral("Paused");
    sh.posUs = qlonglong(s.pos * 1e6);
    QVariantMap m;
    if (s.hasMedia) {
        m.insert(QStringLiteral("mpris:trackid"), QVariant::fromValue(QDBusObjectPath(QStringLiteral("/org/kinoz/track/1"))));
        m.insert(QStringLiteral("mpris:length"), qlonglong(s.dur * 1e6));
        m.insert(QStringLiteral("xesam:title"), s.title);
        if (!s.artist.isEmpty()) m.insert(QStringLiteral("xesam:artist"), QStringList{s.artist});
        if (!s.album.isEmpty()) m.insert(QStringLiteral("xesam:album"), s.album);
    }
    sh.metadata = m;
    QVariantMap changed;
    if (sh.status != d->lastStatus) changed.insert(QStringLiteral("PlaybackStatus"), sh.status);
    if (m != d->lastMeta) changed.insert(QStringLiteral("Metadata"), m);
    d->lastStatus = sh.status; d->lastMeta = m;
    if (changed.isEmpty()) return;
    QDBusMessage sig = QDBusMessage::createSignal(QStringLiteral("/org/mpris/MediaPlayer2"), QStringLiteral("org.freedesktop.DBus.Properties"), QStringLiteral("PropertiesChanged"));
    sig << QStringLiteral("org.mpris.MediaPlayer2.Player") << changed << QStringList();
    QDBusConnection::sessionBus().send(sig);
}
#elif defined(__APPLE__)
// ---- macOS: Now Playing + media keys, implemented in SmtcMac.mm ----
void macMediaInit(Smtc* owner);
void macMediaUpdate(const Smtc::State& s);
struct Smtc::Impl { bool ok = false; };
Smtc::Smtc(QWidget*, QObject* parent) : QObject(parent), d(new Impl) { macMediaInit(this); d->ok = true; }
Smtc::~Smtc() = default;
bool Smtc::ok() const { return d->ok; }
void Smtc::update(const State& s) { macMediaUpdate(s); }
#elif !defined(_WIN32)
struct Smtc::Impl {};
Smtc::Smtc(QWidget*, QObject* parent) : QObject(parent) {}
Smtc::~Smtc() = default;
bool Smtc::ok() const { return false; }
void Smtc::update(const State&) {}
#else

using namespace winrt::Windows::Media;

struct Smtc::Impl {
    SystemMediaTransportControls smtc{nullptr};
    winrt::event_token button{}, position{};
    QString title, artist, album;
    bool video = false, hadMedia = false;
    int lastStatus = -1;
};

namespace {
winrt::hstring hs(const QString& s) { return winrt::hstring(reinterpret_cast<const wchar_t*>(s.utf16()), uint32_t(s.size())); }
winrt::Windows::Foundation::TimeSpan span(double sec) { return std::chrono::duration_cast<winrt::Windows::Foundation::TimeSpan>(std::chrono::duration<double>(sec < 0 ? 0 : sec)); }
}

Smtc::Smtc(QWidget* window, QObject* parent) : QObject(parent), d(new Impl) {
    try {
        auto interop = winrt::get_activation_factory<SystemMediaTransportControls, ISystemMediaTransportControlsInterop>();
        winrt::check_hresult(interop->GetForWindow(reinterpret_cast<HWND>(window->winId()), winrt::guid_of<ISystemMediaTransportControls>(), winrt::put_abi(d->smtc)));
        d->smtc.IsEnabled(true);
        d->smtc.IsPlayEnabled(true); d->smtc.IsPauseEnabled(true); d->smtc.IsStopEnabled(true);
        d->smtc.IsNextEnabled(true); d->smtc.IsPreviousEnabled(true);
        // These arrive on a Windows thread-pool thread: hop to the UI thread before touching anything.
        d->button = d->smtc.ButtonPressed([this](SystemMediaTransportControls const&, SystemMediaTransportControlsButtonPressedEventArgs const& a) {
            const auto b = a.Button();
            QMetaObject::invokeMethod(this, [this, b] {
                switch (b) {
                case SystemMediaTransportControlsButton::Play: emit playRequested(); break;
                case SystemMediaTransportControlsButton::Pause: emit pauseRequested(); break;
                case SystemMediaTransportControlsButton::Stop: emit stopRequested(); break;
                case SystemMediaTransportControlsButton::Next: emit nextRequested(); break;
                case SystemMediaTransportControlsButton::Previous: emit previousRequested(); break;
                default: break;
                }
            }, Qt::QueuedConnection);
        });
        d->position = d->smtc.PlaybackPositionChangeRequested([this](SystemMediaTransportControls const&, PlaybackPositionChangeRequestedEventArgs const& a) {
            const double sec = std::chrono::duration<double>(a.RequestedPlaybackPosition()).count();
            QMetaObject::invokeMethod(this, [this, sec] { emit seekRequested(sec); }, Qt::QueuedConnection);
        });
    } catch (...) {
        d->smtc = nullptr;                      // Windows without SMTC: the app simply goes without it
    }
}

Smtc::~Smtc() {
    try {
        if (d->smtc) {
            d->smtc.ButtonPressed(d->button);
            d->smtc.PlaybackPositionChangeRequested(d->position);
            d->smtc.IsEnabled(false);
        }
    } catch (...) {}
}

bool Smtc::ok() const { return d->smtc != nullptr; }

void Smtc::update(const State& s) {
    if (!d->smtc) return;
    try {
        if (!s.hasMedia) {
            if (d->hadMedia) { d->smtc.PlaybackStatus(MediaPlaybackStatus::Closed); d->smtc.DisplayUpdater().ClearAll(); d->smtc.DisplayUpdater().Update(); }
            d->hadMedia = false; d->lastStatus = -1; d->title.clear();
            return;
        }
        d->hadMedia = true;
        const int status = s.playing ? 1 : 2;
        if (status != d->lastStatus) { d->smtc.PlaybackStatus(s.playing ? MediaPlaybackStatus::Playing : MediaPlaybackStatus::Paused); d->lastStatus = status; }
        if (s.title != d->title || s.artist != d->artist || s.album != d->album || s.video != d->video) {
            d->title = s.title; d->artist = s.artist; d->album = s.album; d->video = s.video;
            auto du = d->smtc.DisplayUpdater();
            du.ClearAll();
            if (s.video) { du.Type(MediaPlaybackType::Video); du.VideoProperties().Title(hs(s.title)); }
            else { du.Type(MediaPlaybackType::Music); auto m = du.MusicProperties(); m.Title(hs(s.title)); m.Artist(hs(s.artist)); m.AlbumTitle(hs(s.album)); }
            du.Update();
        }
        if (s.dur > 0) {
            SystemMediaTransportControlsTimelineProperties tp;
            tp.StartTime(span(0)); tp.MinSeekTime(span(0));
            tp.EndTime(span(s.dur)); tp.MaxSeekTime(span(s.dur));
            tp.Position(span(s.pos));
            d->smtc.UpdateTimelineProperties(tp);
        }
    } catch (...) {}
}
#endif
