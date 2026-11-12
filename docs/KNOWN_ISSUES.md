# Known issues

An honest list of what isn't finished or hasn't been verified yet.

| Area | Status |
| --- | --- |
| macOS and Linux | Builds are produced by CI (`.github/workflows/build.yml`) but have **not been run on real machines yet**. Window move/resize, fullscreen and media keys use different code there and may need fixes. |
| Fullscreen (Windows) | Rewritten to avoid a screen blink seen on some GPUs (window is kept 1 px short of the monitor edge). Tested on one machine. If your screen blinks, turn on **Settings → Safe fullscreen**. |
| System media controls | Windows: media keys and the "now playing" card (volume flyout, lock screen) work (tested without a screen). macOS Now Playing and Linux MPRIS are not done. |
| Gapless playback | Not implemented; a short gap can occur between tracks. |
| 4K / 8K | Decoding and rendering is tested with synthetic 4K and 8K sources and passes. Real 8K HEVC/AV1 files with hardware decode have not been tried; very high bitrates may need a fast disk and GPU. |
| HEIC / AVIF images | Depends on the bundled FFmpeg build; not tested. |
| Memory | Around 280 MB per window while playing 1080p. A minimized video drops its picture decoding to save CPU. |
| Audio without a cover | Shows an icon thumbnail. Album art is not shown in the Music lists or the media overlay yet. |
| Windows 11 right-click menu | kinoz's "Play with kinoz" appears under "Show more options" (the classic menu). The new top-level menu needs a packaged identity, which is not done. |
| Music tab, playlist dialogs, update restart prompt | Built and checked by automated tests; not yet confirmed by eye on a real screen. |
| Updates end to end | The download, verify, restart-or-later and apply-on-next-launch logic is tested; a real 0.1.3 to 0.1.4 update on a real machine is still to be confirmed. |
| Name in other languages | The interface is English only. |

Found something not listed? [Open an issue](https://github.com/p4inz-code/kinoz/issues/new/choose).
