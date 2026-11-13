## What's new in 0.2.1

**Easier to use**
- Settings (a gear) and the **⋯** menu are always in the title bar, even on the empty first screen.
- Hover any icon for a second and a small label shows its name and shortcut.
- While a video plays, the controls and the library fade away after two seconds, so the picture gets the whole window. Move the mouse for the controls, touch the left edge for the library.
- The start screen has a "Make kinoz your default" button.

**Private when you want it**
- New **Settings → Privacy** page: Private mode, Forget on exit, and **Clean identity**, which wipes recent files, resume points, marks, thumbnails, the music index and logs in one click. Each kind of history has its own clear button, files have **Forget this file**, and `kinoz --wipe` does the same from a terminal. History is still saved by default.

**macOS and Linux**
- macOS: opening files from Finder and "Open With", an app icon and document types, traffic-light window buttons, ⌘ shortcut labels, Now Playing and media keys.
- Linux: a fuller desktop entry, "Make kinoz your default" from an AppImage, and MPRIS (media keys and the desktop's now-playing card).
- Fixed: the playback engine could refuse to start under a locale that writes decimals with a comma, and files would not open at all with libmpv older than 0.38 (Ubuntu 22.04 ships 0.34).

**Also**: a 30-second video tour and a clearer banner in the README.

