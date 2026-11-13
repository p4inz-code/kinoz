# Changelog

All notable changes. Dates are release dates.

## Unreleased (0.2.1)
**Easy to use**
- Settings and the "⋯" menu are always in the title bar, including on the empty first screen. Settings has a real gear icon.
- Every icon has a short label with its shortcut after a brief hover (Windows, macOS and Linux key names are shown natively).
- The controls fade out after 2 seconds of stillness while playing, leaving the plain picture, like the Windows 11 media player. The library hides with them (switch off under Settings → Playback) and returns when you touch the left edge.
- The empty screen offers "Make kinoz your default".

**Privacy**
- New Settings → Privacy page: Private mode, Forget on exit, Clean identity, one button per kind of history, "Forget this file", and `kinoz --wipe`. History stays on by default.

**macOS and Linux**
- macOS: opening files from Finder and "Open With", a proper app icon and document types, traffic-light window buttons, ⌘ ⌥ ⇧ shortcut labels, Now Playing and media keys.
- Linux: a fuller desktop entry (file types, keywords, "New window"), "Make kinoz your default" works from an AppImage, and MPRIS for media keys and the desktop's now-playing card.
- CI runs the self-test on macOS and Linux as an informational step.

## 0.2.0 — first public release
Everything from 0.1.1 to 0.1.4 (setup with Doctor, fast skimming, Music and playlists, media overlay, updates that finish the job) plus:
- Licensing and credit made airtight: the fonts' OFL license texts now ship next to the fonts, `THIRD_PARTY_NOTICES.md` lists exact versions and where each component's source is (with a written source offer), every source file carries a copyright and SPDX line, README explains the credit in plain words.
- Windows hardening: Control Flow Guard, high-entropy ASLR and DEP are on, and libraries load only from kinoz's folder and System32.
- README: a "Make kinoz your default player" section for each system, an honest comparison with the built-in players, and an up-to-date status. `docs/KNOWN_ISSUES.md` and `SECURITY.md` brought up to date.

## 0.1.4 — updates that finish the job, audit fixes
**Updates that finish the job**
- Hit "Update" and a real progress bar shows the background download; the file is verified (SHA-256) and kinoz asks **Restart now / Later**.
- Choose Later (or ignore it) and the verified update installs by itself the next time kinoz starts. It rechecks the file's checksum first, gives up after two attempts so it can never loop, and tells you if it couldn't.
- A failed update reopens the previous working version instead of leaving you without a player.
- **Settings → About → Updates**: Automatic (default: download quietly, ask to restart), Notify me, or Off. "Download page" and "Direct download" buttons are always there for doing it yourself.
- Opting out (Off) cancels any staged update. A background download never interrupts a playing video; it shows a small notice instead.

**Audit fixes**
- The Music list no longer jumps back to the top every half second while tags are still being read.
- Playlist files can no longer point at network paths (`\server\share`): checking such a file makes Windows sign in to that server, which could leak the user's login hash. Local files only.
- With Automatic updates and kinoz left open for days, the daily check no longer downloads an already staged update again.

## 0.1.3 — fresh visuals and release checks
- Every screenshot and the README banner are regenerated from the current build (library marks, Music, playlists, queue, all setup pages, Settings), using generated demo media. README updated for skimming, music, playlists, the media overlay, setup and the new shortcuts.
- Releasing now always refreshes the visuals: `scripts/refresh-visuals.ps1`, `scripts/make-hero.ps1` (run by `scripts/release.ps1`), a checklist in docs/RELEASING.md, and a CI check that fails a release tag if the images did not change since the previous release.

## 0.1.2 — fast skimming, music library, playlists, media overlay
**Stability**
- Crash-loop protection: if kinoz ends abnormally twice in a row (crash, driver reset, power cut) it starts in safe mode: software decoding, no glass effect, taskbar-friendly fullscreen, with a notice. Settings can switch it all back.
- Default decoding is "Auto": every file starts in software; only 1440p and larger video uses the GPU.

**Music, playlists and the Windows media overlay**
- New **Music** tab in the sidebar: Artists, Albums, Songs and Playlists, drilling down from artist to album to track. Tags (title, artist, album, track, length) are read once in the background from the folders in your library, cached in `data/music.json`, and updated as files change.
- **Saved playlists** are plain `.m3u8` files in `data/playlists`. Save the queue from the Queue tab, the Playlists list or `Ctrl+K` ("Save the queue as a playlist"); rename or delete from inside a playlist. `.m3u`, `.m3u8` and `.pls` files open as a queue (drop one on the window or open it).
- **Windows media overlay (SMTC)**: the now-playing card in the volume flyout and on the lock screen shows the title, artist and progress, with working play, pause, next, previous, stop and seeking.

**Fast skimming (built in, always on)**
- Clicking through files or spamming Next opens only the last file (150 clicks measured: 2 files opened, UI never frozen more than 25 ms). The next file's head and tail are pre-read so it opens faster.
- Hover preview on the seek bar is served from a frame strip built in the background (coarse first), then replaced by the exact frame.
- `1`-`9` jump to 10%-90% of a video or track, `0` back to the start. `Y` keeps a file, `N` marks it to reject, `U` jumps to the next file you haven't seen, `Delete` moves the current file to the Recycle Bin after a confirmation. Palette: "Delete all rejected files...".
- The library and queue show keep / reject badges, a watched-progress bar and an unseen dot. Sort menu: Show everything / not seen yet / kept / marked to reject.
- Video decoding "Auto" starts every file in software (about 30% faster click to picture) and switches 1440p and larger to the GPU; GPU and CPU-only are still options.
- Controls fade after 2 s of no pointer activity (setting: 2 s / 3 s / 5 s / never).
- Settings are written by a background thread, never on the UI thread.
- `kinoz --bench <video>` measures all of this and checks it against budgets (docs/PERFORMANCE.md).

**Fullscreen**
- Entering fullscreen from a maximised window no longer plays the restore animation first (the blink); leaving goes straight back to maximised.
- Rapid fullscreen toggles (double-click, F-F) are ignored for 350 ms.
- Unplugging or adding a monitor while fullscreen re-fits the window; leaving fullscreen can't put the window off-screen.

- Uninstall is one click inside kinoz: Settings → General → Uninstall… (Windows) or `Ctrl+K` → "Uninstall kinoz…". Opens the same Remove screen with the delete-all-data switch.

## 0.1.1 — custom setup, Doctor, default player

**Setup**
- Custom setup window drawn by the app itself (Install, Update/Reinstall, Repair, Remove), light and dark.
- Setup files are verified against a SHA-256 manifest; copy keeps the old files aside and rolls back on any failure; checks free space and refuses system folders. Log at `%TEMP%\kinoz-setup.log`.
- Built-in Doctor (`kinoz.exe --doctor`, also in the command palette): verifies program files, runtime, shortcut, Apps entry, right-click entries, data folder, settings files, disk space, and fixes what it can.
- Remove with one switch to delete all data too (settings, resume points, thumbnails, logs, exported frames). Your own media is never touched. Silent flags: `--uninstall --silent [--delete-data]`.

**Default player**
- Settings → Default player, the palette command "Make kinoz my default player", and a button on setup's last page. On Windows kinoz is registered under Settings → Default apps and its page opens there; Linux uses `xdg-mime`; macOS shows the Get Info steps.
- `` ` `` (backtick) toggles the library sidebar, as well as `Tab`.

**Motion**
- Apple's standard easing curve on sheets, menus, the sidebar and the side panel (longer, softer landings); switches use a springy knob that stretches as it travels; segmented controls glide their selection pill; buttons shrink slightly while pressed. All of it is off with Animations → Off.

**Fixes**
- Closing kinoz no longer ends with a crash inside libmpv (the video widget is now freed before the player core).
- Setup only ever removes what it installed: files listed in its manifest (plus `data` if you chose to delete data). A folder without kinoz's file list is refused, and unrelated files in a shared folder are left alone.

## 0.1.0 — first public build
First version. Windows build is the one that has been run and tested; the macOS and Linux builds are produced by CI and have **not** been tested by hand yet (see [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md)).

**Playback**
- Plays video, audio, images and animated GIFs in one window (libmpv + FFmpeg, no codec packs).
- Speed 0.25×–4× with pitch correction, frame step, resume position per file, subtitle and audio-track menus, subtitle delay and size.
- Picture adjust (brightness, contrast, saturation, gamma, hue), 10-band equalizer with presets, loudness normalizer, output-device picker.
- Images and GIFs: zoom, pan, rotate, browse a folder with the arrow keys.

**Library**
- Sidebar with your folders, thumbnails, list/grid view, type filter, search, sort, and a drag-reorderable queue.
- `Ctrl+K` command palette that searches your library and every command.
- Seek-bar hover shows a frame preview.

**Interface**
- Fully custom: no OS title bar, menus or dialogs. Light and dark theme (follows the system or manual).
- Glass control bar with a blurred backdrop; liquid-metal accents.
- Mini always-on-top player, fullscreen, "Safe fullscreen" option.
- Windows Media Player-style keyboard shortcuts, editable in `keymap.json`.

**Tools**
- Annotate a frame or image (pen, highlighter, arrow, shapes, text) and export a full-resolution PNG, or copy it.

**Settings and polish**
- Settings: General (theme, six accent colours, animations System/On/Off, glass, density, hardware decoding, safe fullscreen), Playback (resume, autoplay, queue folder, normalize, starting speed, seek steps, volume step, controls auto-hide, slideshow), Shortcuts (every key and mouse action, with a keymap.json editor), Info (system, graphics, engine, current file, formats, storage, copy diagnostics), About, Legal.
- A-B loop, sleep timer, aspect ratio, audio and subtitle sync, show in folder, copy path.
- Open/close fades and soft menu animations (all removed when animations are off).
- 4K and 8K handling: larger read-ahead for big frames and high-quality downscaling.
- Hardening: no external tools or scripts, no playlist references, crash dumps for diagnosis.

**Everyday**
- Several independent windows (`Ctrl+N`); a minimized video keeps playing sound without decoding the picture.
- Errors are shown plainly; a broken file is skipped instead of stopping the queue.
- Auto-updater (GitHub Releases, SHA-256 verified, can be switched off).
- Free software under GPLv3 with author credit ([ADDITIONAL_TERMS.md](ADDITIONAL_TERMS.md)).
