# Architecture

kinoz is a C++20 / Qt 6 Widgets application on top of libmpv. One engine plays video, audio, images and GIFs, so there is one rendering path to keep fast.

```
UI (Qt Widgets, custom painted)
  MainWindow ─ Sidebar ─ Stage (video / now-playing / image) ─ ControlBar ─ TopBar ─ Overlays (palette, panels, sheets, annotation)
        │ signals / slots
  PlayerController   queue, shuffle/repeat, resume, error handling, per-media behaviour
        │
  MpvPlayer          the only code that touches libmpv's client API; async commands; cached state
        │
  VideoWidget        QOpenGLWidget + mpv render API; draws the glass (blurred backdrop) for floating UI
  ThumbService / PreviewService / FrameGrabber   headless libmpv (software render) for thumbnails and the seek preview
  LibraryModel / LibraryIndex   folder listing off the UI thread; name index for Ctrl+K
  Updater            GitHub Releases, SHA-256 verified
```

## Rules the code follows
1. **Only `MpvPlayer` talks to libmpv's client API.** The rest sees small Qt-friendly signals and methods.
2. **The UI thread never blocks.** mpv commands are asynchronous, folder scans and thumbnails run on worker threads, teardown happens off-thread so closing is instant.
3. **Rapid input is coalesced.** Seeks collapse to the latest one in flight; loading a file bumps a generation so stale events are ignored; volume steps are optimistic.
4. **Fully custom UI.** Icons are vector paths, controls are painted, menus and dialogs are our own widgets. The title bar is ours; on Windows the frame is handled through `WM_NCHITTEST`/`WM_NCCALCSIZE`, elsewhere through `startSystemMove/Resize`.
5. **Portable data.** On Windows everything kinoz saves lives in `data/` next to the program (falling back to `%LOCALAPPDATA%` if that is not writable); on macOS/Linux it uses the standard per-user data folder.

## Glass
`VideoWidget` renders the mpv frame, then for each floating surface copies the backdrop at quarter resolution, blurs it (two separable Gaussian passes), and composites it with a tint, a slow specular band, a pointer glint and a rim light, masked to a rounded rectangle. The Qt overlay widgets draw the icons and text on top with transparent backgrounds. Without GL support they fall back to a solid tint.

## Windows-specific parts
Frameless window handling, manual fullscreen (kept 1 px short of the monitor edge to avoid the display blinking), media-key hotkeys, topmost handling and the Inno Setup installer. macOS/Linux have equivalents in `#ifndef Q_OS_WIN` branches; those are built by CI and not yet tested by hand.