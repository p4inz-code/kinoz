# History of kinoz

A short, honest record of how kinoz got to where it is and why it is built the way it is. The version-by-version list is in [CHANGELOG.md](../CHANGELOG.md); this file keeps the reasoning behind the big choices.

## Where it started

kinoz began as a plan for a fast, offline Windows player to replace the built-in Windows 11 Media Player. The goals were quick start and low CPU and memory use, no codec packs, a sidebar to browse folders and switch files without a file dialog, speed control with pitch correction, live picture adjustment, and drawing on a frame with image export. Everything had to live in the project folder, there would be no native OS controls (custom title bar, menus, dialogs, sliders and file picker), and it needed a light and a dark theme. The look follows a calm, minimal style: warm paper neutrals, a teal accent, 8 px radius, Atkinson Hyperlegible Next for text and Familjen Grotesk for headings. The design notes are in [DESIGN.md](DESIGN.md).

It was first named vidPlayer. The name kinoz was chosen later, and the name lives in one place (`APP_NAME` in `CMakeLists.txt` and `packaging/installer.iss`).

## Decisions and the reasons for them

**C++, Qt 6 Widgets and libmpv.** One engine plays video, audio, images and GIFs. libmpv renders through its OpenGL render API into a `QOpenGLWidget`. The window is frameless with its own hit testing. All widgets are painted by kinoz, which is how the look stays identical on every system. Thumbnails and the seek-bar preview come from a second, software-rendered libmpv context.

**GPLv3 with additional terms, not MIT.** The first plan was MIT. The Windows libmpv build that is bundled is GPL, so distributing the whole thing under GPLv3 is the consistent choice, and it also does what the author wanted: free for anyone, with the credit kept. The extra terms (author credit, copyright notices, a modified version gets its own name) are the ones GPLv3 section 7 allows, in [ADDITIONAL_TERMS.md](../ADDITIONAL_TERMS.md). Third-party licenses and the source offer are in [THIRD_PARTY_NOTICES.md](../THIRD_PARTY_NOTICES.md).

**Fullscreen is a manual borderless window.** Qt's `showFullScreen` blanked or blinked the screen on some GPUs. The Windows build instead removes the window frame, keeps the window one pixel short of the monitor's bottom edge (a window that exactly covers the monitor can trigger the driver's special flip mode), and sits above the taskbar. **Safe fullscreen** in Settings fills the work area and leaves the taskbar visible. Going fullscreen from a maximised window reads the normal rectangle with `GetWindowPlacement`; un-maximising first plays Windows' restore animation, which looked like a blink. Toggles are ignored for 350 ms so a double-click plus `F` cannot thrash the window style.

**Software decoding first.** Starting a hardware decoder costs about 200 ms per file, which is what you feel when clicking through many videos. The default decoding mode, Auto, starts every file in software and moves 1440p and larger video to the GPU as soon as its size is known. GPU-always and CPU-only are options. This also keeps the GPU driver out of the picture for most videos.

**The app is its own installer.** `Setup.exe` is a thin Inno Setup wrapper. It unpacks `payload.zip` with `tar.exe` and runs `kinoz.exe --installer`, so the setup window is drawn by kinoz. Install is per user (current-user registry only, no administrator rights, no drivers, no services) and defaults to a drive other than `C:`. `scripts/make-payload.ps1` writes a `manifest.json` with a SHA-256 for every file. Setup verifies it before copying, keeps the old files aside during the copy, and rolls back on failure. The Doctor re-checks everything against the same manifest. Uninstall removes only files listed in the manifest (plus the `data` folder if the user chose that) and refuses a folder that has no manifest.

**Updates finish the job.** The updater downloads in the background with a progress bar, checks the SHA-256 against the published checksum, and asks "Restart now / Later". The verified installer stays in a staging folder. If the user picks Later, it is applied at the next start, after checking the checksum again. It gives up after two attempts so it can never loop, and a failed update reopens the previous version. Modes are Automatic (default), Notify me and Off.

**Fast skimming.** Clicks are coalesced: the first click on a calm moment loads immediately, a burst loads only its last file after 70 ms. Resume points belong to the file mpv actually has open, not the one just clicked. The next file's head and tail are pre-read into the OS cache. The hover preview shows the nearest frame from a strip built in the background, then the exact frame. Review marks (keep, reject, seen and how far) are stored in `data/marks.json`. Deleting always asks and uses the Recycle Bin.

**Music.** Tags and lengths are read once by a headless mpv on a low-priority thread and cached in `data/music.json`. Playlists are `.m3u8` files. Playlist loading refuses network paths, because merely checking that a `\\server\share` file exists makes Windows sign in to that server.

**Stability.** If kinoz ends abnormally twice in a row (a marker file is left behind), it starts in safe mode: software decoding, no glass effect, safe fullscreen. A crash dump is written to the logs folder.

## Releases

- 0.1.0: first build, Windows tested.
- 0.1.1: the self-drawn setup with Doctor and rollback; default-player button; one-click uninstall.
- 0.1.2: fast skimming, Music tab, playlists, Windows now-playing card.
- 0.1.3: refreshed screenshots and the release checks.
- 0.1.4: updates that finish the job, plus audit fixes (the Music list jumping while scanning, network paths in playlists, a repeat download).
- 0.2.0: the first public release. Licensing and credit tightened, Windows hardening, a README with comparisons to the default players.

## What is not verified

The macOS and Linux builds come from CI on every release and have not been run on real machines. On Windows, the fullscreen fix, the Music tab and playlist dialogs, the now-playing buttons and a real end-to-end update were checked only by automated tests, without looking at a screen. [KNOWN_ISSUES.md](KNOWN_ISSUES.md) keeps the live list.
