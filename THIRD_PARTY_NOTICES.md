# Third-party software

kinoz is free software under the **GNU General Public License v3** (see `LICENSE`).
It is built on the following components, which keep their own licenses:

| Component | Use | License |
|---|---|---|
| [mpv / libmpv](https://mpv.io) (Windows build by shinchiro, mpv-winbuild-cmake) | playback engine | GPL v2+ (this build), includes FFmpeg and libass |
| [FFmpeg](https://ffmpeg.org) | decoding (inside libmpv) | GPL/LGPL, depending on build |
| [Qt 6](https://www.qt.io) | user interface | LGPL v3 (dynamically linked) |
| Atkinson Hyperlegible Next | interface font | SIL Open Font License 1.1 |
| Familjen Grotesk | heading font | SIL Open Font License 1.1 |
| Microsoft Visual C++ runtime | C++ runtime | Microsoft redistributable terms |

Because the bundled libmpv/FFmpeg build is GPL, distributing kinoz under GPL v3 is consistent with all of the above.
Source code for kinoz is in this repository; sources for the third-party parts are available from the links above.
