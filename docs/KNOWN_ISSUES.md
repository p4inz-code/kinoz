# Known issues

An honest list of what isn't finished or hasn't been verified yet.

| Area | Status |
| --- | --- |
| macOS and Linux | Builds are produced by CI (`.github/workflows/build.yml`) but have **not been run on real machines yet**. Window move/resize, fullscreen and media keys use different code there and may need fixes. |
| Fullscreen (Windows) | Rewritten to avoid a screen blink seen on some GPUs (window is kept 1 px short of the monitor edge). Tested on one machine. If your screen blinks, turn on **Settings → Safe fullscreen**. |
| System media controls | Windows: media keys and the "now playing" card (volume flyout, lock screen) work (tested without a screen). macOS Now Playing and Linux MPRIS (media keys, the shell's now-playing card) are written but, like the rest of macOS and Linux, have not been run on real machines yet. |
| Gapless playback | Songs that follow each other in the queue are handed to the engine ahead of time and run into each other (Settings → Playback → Gapless songs). Tested on Windows with generated files, not by ear on real albums. It is off for shuffle, repeat-one and video. |
| 4K / 8K | Decoding and rendering is tested with synthetic 4K and 8K sources and passes. Real 8K HEVC/AV1 files with hardware decode have not been tried; very high bitrates may need a fast disk and GPU. |
| Screen readers and keyboard | Settings works with the keyboard (Tab, Space, Enter, arrows) and the custom-drawn controls have names and descriptions for screen readers, but this has not been tried with NVDA, VoiceOver or Orca. The painted control bar is described as one item; every control has its keyboard shortcut. |
| HEIC / AVIF images | Depends on the bundled FFmpeg build; not tested. |
| Memory | Around 280 MB per window while playing 1080p. A minimized video drops its picture decoding to save CPU. |
| Album art | The Music lists and the library show embedded covers and folder.jpg / cover.jpg next to the songs. The media overlay and the big now-playing card still use only an embedded cover. |
| Windows 11 right-click menu | kinoz's "Play with kinoz" appears under "Show more options" (the classic menu). The new top-level menu needs a packaged identity, which is not done. |
| Music tab, playlist dialogs, update restart prompt | Built and checked by automated tests; not yet confirmed by eye on a real screen. |
| Updates end to end | The download, verify, resume, retry, restart-or-later and apply-on-next-launch logic is tested on Windows. The in-app update on Linux (AppImage replaces itself) and macOS (app swapped from the disk image) is new and has not run on a real machine yet. If it fails you get a message in the app and the old version keeps working; please send a test report. |
| Name in other languages | The interface is English only. |

Found something not listed? [Open an issue](https://github.com/p4inz-code/kinoz/issues/new/choose).
