// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Atharva Patil, P4inz Interactive Labs. Free software: see LICENSE and ADDITIONAL_TERMS.md (the credit must be kept).
// macOS: the Now Playing card in Control Center and the keyboard media keys (play/pause, next, previous) reach kinoz.
#include "core/Smtc.h"

#import <Foundation/Foundation.h>
#import <MediaPlayer/MediaPlayer.h>

#include <QMetaObject>

static void post(Smtc* owner, void (Smtc::*sig)()) {
    QMetaObject::invokeMethod(owner, [owner, sig] { (owner->*sig)(); }, Qt::QueuedConnection);   // the handlers run on a system thread
}

void macMediaInit(Smtc* owner) {
    MPRemoteCommandCenter* c = [MPRemoteCommandCenter sharedCommandCenter];
    auto add = [](MPRemoteCommand* cmd, Smtc* o, void (Smtc::*sig)()) {
        cmd.enabled = YES;
        [cmd addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent*) { post(o, sig); return MPRemoteCommandHandlerStatusSuccess; }];
    };
    add(c.playCommand, owner, &Smtc::playRequested);
    add(c.pauseCommand, owner, &Smtc::pauseRequested);
    add(c.nextTrackCommand, owner, &Smtc::nextRequested);
    add(c.previousTrackCommand, owner, &Smtc::previousRequested);
    add(c.stopCommand, owner, &Smtc::stopRequested);
    c.changePlaybackPositionCommand.enabled = YES;
    [c.changePlaybackPositionCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent* ev) {
        const double t = ((MPChangePlaybackPositionCommandEvent*)ev).positionTime;
        QMetaObject::invokeMethod(owner, [owner, t] { emit owner->seekRequested(t); }, Qt::QueuedConnection);
        return MPRemoteCommandHandlerStatusSuccess;
    }];
    // The play/pause media key sends the toggle command; map it onto whichever action makes sense.
    c.togglePlayPauseCommand.enabled = YES;
    [c.togglePlayPauseCommand addTargetWithHandler:^MPRemoteCommandHandlerStatus(MPRemoteCommandEvent*) {
        const bool playing = [MPNowPlayingInfoCenter defaultCenter].playbackState == MPNowPlayingPlaybackStatePlaying;
        post(owner, playing ? &Smtc::pauseRequested : &Smtc::playRequested);
        return MPRemoteCommandHandlerStatusSuccess;
    }];
}

void macMediaUpdate(const Smtc::State& s) {
    MPNowPlayingInfoCenter* ic = [MPNowPlayingInfoCenter defaultCenter];
    if (!s.hasMedia) {
        ic.nowPlayingInfo = nil;
        ic.playbackState = MPNowPlayingPlaybackStateStopped;
        return;
    }
    NSMutableDictionary* d = [NSMutableDictionary dictionary];
    d[MPMediaItemPropertyTitle] = s.title.toNSString();
    if (!s.artist.isEmpty()) d[MPMediaItemPropertyArtist] = s.artist.toNSString();
    if (!s.album.isEmpty()) d[MPMediaItemPropertyAlbumTitle] = s.album.toNSString();
    d[MPMediaItemPropertyPlaybackDuration] = @(s.dur);
    d[MPNowPlayingInfoPropertyElapsedPlaybackTime] = @(s.pos);
    d[MPNowPlayingInfoPropertyPlaybackRate] = @(s.playing ? 1.0 : 0.0);
    ic.nowPlayingInfo = d;
    ic.playbackState = s.playing ? MPNowPlayingPlaybackStatePlaying : MPNowPlayingPlaybackStatePaused;
}
