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

#ifndef _WIN32
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
