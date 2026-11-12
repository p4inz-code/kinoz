# Changelog

All notable changes. Dates are release dates.

## 0.1.0 — first public build (unreleased)
First version. Windows build is the one that has been run and tested; the macOS and Linux builds are produced by CI and have **not** been tested by hand yet (see [docs/KNOWN_ISSUES.md](docs/KNOWN_ISSUES.md)).

**Setup**
- Custom setup window drawn by the app itself (Install, Update/Reinstall, Repair, Remove), light and dark.
- Setup files are verified against a SHA-256 manifest; copy keeps the old files aside and rolls back on any failure; checks free space and refuses system folders. Log at `%TEMP%\kinoz-setup.log`.
- Built-in Doctor (`kinoz.exe --doctor`, also in the command palette): verifies program files, runtime, shortcut, Apps entry, right-click entries, data folder, settings files, disk space, and fixes what it can.
- Remove with one switch to delete all data too (settings, resume points, thumbnails, logs, exported frames). Your own media is never touched. Silent flags: `--uninstall --silent [--delete-data]`.

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
