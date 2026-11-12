<p align="center">
  <img src="assets/readme/hero.png" alt="kinoz screens arranged like prints on a wall: the dark home screen, a light theme, the Ctrl+K command palette, Settings and About." width="100%">
</p>

<h1 align="center">kinoz</h1>
<p align="center"><b>Everything you play, in one calm window.</b><br>A free, open-source, offline media player for videos, music, images and GIFs. Windows, macOS and Linux.</p>

<p align="center">
  <a href="https://github.com/p4inz-code/kinoz/releases/latest"><img src="assets/readme/btn-get.svg" alt="Download kinoz, free for Windows, macOS and Linux" width="260"></a>
  <a href="https://github.com/p4inz-code/kinoz/issues/new/choose"><img src="assets/readme/btn-issues.svg" alt="Report a bug or an idea" width="260"></a>
  <a href="https://discord.gg/8UKt8s5FbW"><img src="assets/readme/btn-discord.svg" alt="Join the Discord" width="260"></a>
</p>

<p align="center">
  <a href="https://github.com/p4inz-code/kinoz/releases/latest"><img src="https://img.shields.io/github/v/release/p4inz-code/kinoz?style=flat-square&labelColor=14140f&color=4fd1d6&label=release" alt="Latest release"></a>
  <a href="https://github.com/p4inz-code/kinoz/releases"><img src="https://img.shields.io/github/downloads/p4inz-code/kinoz/total?style=flat-square&labelColor=14140f&color=f2b24d&label=downloads" alt="Downloads"></a>
  <a href="https://github.com/p4inz-code/kinoz/stargazers"><img src="https://img.shields.io/github/stars/p4inz-code/kinoz?style=flat-square&labelColor=14140f&color=c0a8ff&label=stars" alt="Stars"></a>
  <a href="LICENSE"><img src="https://img.shields.io/github/license/p4inz-code/kinoz?style=flat-square&labelColor=14140f&color=ff8fb1&label=license" alt="GPLv3 license"></a>
</p>

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

You have films in one folder, music in another, a pile of GIFs and screenshots somewhere else, and the player that opens them is slow, forgets where you stopped and makes you hunt through a file dialog. kinoz is one window for all of it.

- **Plays almost anything** with no codec packs: video, music, images and GIFs, in one window (libmpv + FFmpeg).
- **Fast when you click fast.** Click through a folder or hammer Next and only the last file opens. About 0.3 s from click to picture.
- **Skim and clean up.** `Y` keep, `N` reject, `U` next unseen, `Delete` to the Recycle Bin, watched marks on every file.
- **Music too.** Artists, albums, songs, saved playlists, and the now-playing card on Windows.
- **Looks the same everywhere.** Custom interface, light and dark, a real animations-off switch. No OS title bar, menus or dialogs.
- **Free, offline, no account, no telemetry.** GPLv3, with credit kept.

## Get kinoz

| Your system | Download | Install |
|---|---|---|
| **Windows 11** (10 should work) | [`kinoz-Setup-<version>.exe`](https://github.com/p4inz-code/kinoz/releases/latest) | Run it. Per-user, no administrator rights, installs outside `C:` by default. |
| **macOS** (Apple Silicon) | [`kinoz-<version>-macos.dmg`](https://github.com/p4inz-code/kinoz/releases/latest) | Open it, drag kinoz to Applications. |
| **Linux** (x86-64) | [`kinoz-<version>-x86_64.AppImage`](https://github.com/p4inz-code/kinoz/releases/latest) | `chmod +x` it and run it. |

Every file has a `.sha256` next to it. [Updates and uninstalling](#updates-and-uninstall) are one click, and [making kinoz your default player](#make-kinoz-your-default-player) is one button.

> **Status: 0.2, usable every day on Windows.** Windows is tested hardest (an automated self-test of 94 checks and a speed benchmark). The macOS and Linux builds come from CI and have **not** been run on real machines yet, so expect rough edges there. The honest list of what is missing is in [Known issues](docs/KNOWN_ISSUES.md) and the [roadmap](ROADMAP.md).
>
> **On macOS or Linux?** You are the testers. [Open an issue](https://github.com/p4inz-code/kinoz/issues/new/choose) and say what worked and what did not: moving and resizing the window, fullscreen, media keys, opening your files.

<table>
<tr>
<td width="50%" align="center">
<sub>Dark theme</sub><br><br>
<img src="assets/screenshots/home-dark.png" alt="kinoz in the dark theme with the library sidebar and a video playing" width="100%">
</td>
<td width="50%" align="center">
<sub>Skim fast: keep, reject, watched progress</sub><br><br>
<img src="assets/screenshots/library-marks-light.png" alt="Library with a keep mark, a reject mark, watched-progress bars and unseen dots" width="100%">
</td>
</tr>
<tr>
<td width="50%" align="center">
<sub>Music: artists, albums, songs, playlists</sub><br><br>
<img src="assets/screenshots/music-album-light.png" alt="Music tab drilled into an album" width="100%">
</td>
<td width="50%" align="center">
<sub>Light theme (<code>Ctrl+D</code> switches, no restart)</sub><br><br>
<img src="assets/screenshots/home-light.png" alt="kinoz in the light theme" width="100%">
</td>
</tr>
</table>

<details>
<summary>More screenshots: playlists, queue, setup, command palette, Settings, About</summary>
<br>

<p align="center">
  <img src="assets/screenshots/playlists-light.png" alt="Saved playlists" width="720">
  <br><sub>Saved playlists are plain <code>.m3u8</code> files in the data folder. Save the queue from the Queue tab or <code>Ctrl+K</code>.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/queue-light.png" alt="The queue with the save-as-playlist button" width="720">
  <br><sub>The queue: drag to reorder, save it as a playlist, clear it.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/setup-maintenance-light.png" alt="kinoz setup: Update, Repair, Remove" width="720">
  <br><sub>The Windows setup is drawn by kinoz itself: Install, Update, Repair (the Doctor), and Remove with an optional delete-all-my-data switch.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/setup-doctor-dark.png" alt="The Doctor checking an installation" width="720">
  <br><sub>The Doctor checks files, shortcuts, right-click entries, settings and disk space, and fixes what it can.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/palette-dark.png" alt="The Ctrl+K command palette" width="720">
  <br><sub><code>Ctrl+K</code> searches your whole library and every command.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/settings-light.png" alt="Settings, General: theme, accent colour, animations, glass effect, density" width="720">
  <br><sub>Settings: theme, six accent colours, animations on or off, glass effect, density.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/settings-playback-light.png" alt="Settings, Playback: resume, autoplay, starting speed, seek steps, slideshow" width="720">
  <br><sub>Playback: resume, autoplay, starting speed, seek steps, volume step, controls auto-hide, slideshow.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/settings-shortcuts-light.png" alt="Settings, Shortcuts: every key and mouse action" width="720">
  <br><sub>Every keyboard shortcut and mouse action is listed, and you can add your own.</sub>
</p>

<p align="center">
  <img src="assets/screenshots/about-light.png" alt="About: version, license, credit, updates" width="720">
  <br><sub>About keeps the version, license, credit and update check in one place.</sub>
</p>

</details>

<p align="center">
  <a href="#get-kinoz">Download</a> &nbsp;|&nbsp;
  <a href="#a-one-minute-tour">Tour</a> &nbsp;|&nbsp;
  <a href="#make-kinoz-your-default-player">Default player</a> &nbsp;|&nbsp;
  <a href="#compared-with-the-default-apps">Compared with the defaults</a> &nbsp;|&nbsp;
  <a href="#what-you-get">What you get</a> &nbsp;|&nbsp;
  <a href="#keyboard-shortcuts">Shortcuts</a> &nbsp;|&nbsp;
  <a href="#faq">FAQ</a> &nbsp;|&nbsp;
  <a href="#troubleshooting">Troubleshooting</a> &nbsp;|&nbsp;
  <a href="#build-from-source">Build from source</a>
</p>

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## A one-minute tour

1. **Add a folder.** Click the **+** in the sidebar and pick where your media lives. Thumbnails appear.
2. **Play and skim.** Click a file. Click through the list fast; only the last one opens. Hover the seek bar for an instant frame preview.
3. **Decide.** `Y` keeps a file, `N` marks it to reject, `U` jumps to the next one you have not seen. `Delete` moves the current file to the Recycle Bin (it asks first), and `Ctrl+K` then "Delete all rejected files" clears the pile.
4. **Find anything.** `Ctrl+K` searches every file under your folders and every command.
5. **Music.** The **Music** tab shows artists, albums and songs read from your files' tags. Save the queue as a playlist from the **Queue** tab.
6. **Make it yours.** `Ctrl+D` flips light and dark. **Settings, Animations, Off** stops all motion. **Settings, General, Default player** makes kinoz open your files.

## Why people switch

1. **One player for everything.** Video, audio, images and GIFs share a window, and the stage adapts: letterboxed video, a cover-art card for music, a zoom-and-pan viewer for pictures, a looping player for GIFs.
2. **Built for people who click fast.** In the benchmark 150 rapid clicks opened 2 files and the interface never froze for more than 25 ms. The next file is pre-read, the seek-bar preview never waits, and memory stays around 255 MB. `kinoz --bench <video>` reproduces the numbers on your machine.
3. **Real glass, and an off switch for everything.** The control bar blurs the video behind it, live on the GPU. Prefer it flat, or nothing moving at all? One switch each.
4. **Your library, one keystroke away.** Folders in a sidebar, list or grid, filter, search, a drag-reorderable queue, and resume from where you stopped.
5. **Tools you reach for.** Annotate any frame or image and export a full-resolution PNG. A-B loop, sleep timer, aspect ratio, audio and subtitle sync, a 10-band equalizer, picture adjust, speed 0.25x to 4x with pitch correction.
6. **Honest and safe.** No account, no telemetry; the only network request is the optional update check. It never runs as administrator, and deleting always asks and goes to the Recycle Bin.

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## Make kinoz your default player

No program may silently take over your file types, and kinoz doesn't try. It does the part that is allowed and makes the rest one click:

| System | How |
|---|---|
| **Windows** | **Settings → General → Default player → Make default** (or `Ctrl+K`, "Make kinoz my default player"). kinoz registers itself and opens its page in Windows **Settings → Apps → Default apps**, where you pick it for video, music and pictures. The installer's last page has the same button. |
| **macOS** | Select a video in Finder, press `Cmd+I`, set **Open with** to kinoz, then click **Change All**. |
| **Linux** | The same menu entry runs `xdg-mime` for the common video, audio and image types. |

Right-click entries ("Play with kinoz", "Open folder in kinoz") and the "Open with" list are added by the Windows setup. To undo any of it, **Settings → General → Uninstall…** removes kinoz, its entries and, if you choose, all of its data.

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## Compared with the default apps

Every system ships players for video, music and pictures. They are fine at the basics and good at what the vendor sells (stores, cloud libraries, casting). kinoz is for the rest: your own files, played well, with one app and one look everywhere. This compares kinoz with the apps you get out of the box. They change between versions and distributions, so treat it as a guide, and [corrections are welcome](https://github.com/p4inz-code/kinoz/issues/new/choose).

### Windows 11

| Default app | What it is for | Where kinoz differs |
|---|---|---|
| **Media Player** (replaced Groove Music) | Music library, playlists, and video in one app | Plays almost any file with no extra codec downloads; skim tools for folders of videos (keep, reject, next unseen); frame preview on the seek bar; per-file resume; one window for video, music, images and GIFs; no Microsoft account |
| **Movies & TV** | Video playback and the Microsoft store | Local files only, so no store or DRM video; plays MKV, AVI, WebM, FLAC and similar directly; equalizer, picture adjust, A-B loop, frame step, annotate any frame |
| **Photos** | Pictures, albums, cloud sync | Opens images and GIFs next to your videos with zoom, pan and a slideshow; no cloud sync and no photo editing |
| **Snipping Tool** (frame grabs) | Screenshots | Save any video frame at full resolution, or draw on it first, from the player itself |

**The Windows apps do better at:** syncing with a Microsoft account, store purchases and DRM streaming, casting, and CD ripping and burning.

### macOS

| Default app | What it is for | Where kinoz differs |
|---|---|---|
| **QuickTime Player** | Playing and simple recording of video and audio | Plays formats QuickTime does not open (for example many MKV and AVI files) with no converter; a real library sidebar and queue instead of one file at a time; playlists; speed with pitch correction; equalizer |
| **Music** | Your Apple Music and iTunes library, local files, playlists | Works straight from your folders with no library import and no Apple account; artists, albums and songs read from the files' tags; the same app plays your videos |
| **Preview / Photos** | Images, PDFs, your photo library | Browse a folder of pictures and GIFs with the arrow keys, zoom, rotate, slideshow, next to your videos |

**The Mac apps do better at:** AirPlay and iCloud, Apple Music streaming, deep system integration, and the Photos library. kinoz's Now Playing integration on macOS is not done yet.

### Linux (GNOME, KDE and others)

| Default app | What it is for | Where kinoz differs |
|---|---|---|
| **GNOME Videos / Showtime, KDE Dragon Player** | Simple video playback | A library sidebar, queue, resume, skim tools, picture adjust, equalizer and annotate; the same window plays music and images; consistent look on any desktop |
| **Rhythmbox, Elisa** | Music libraries | Artists, albums, songs and playlists from your folders, in the same app as your videos; saved playlists are plain `.m3u8` files |
| **Image viewers** (Eye of GNOME, Gwenview) | Pictures | Pictures and GIFs open in the same window as video and music |

**The Linux apps do better at:** following your desktop theme exactly, MPRIS and desktop integration (kinoz's MPRIS support is not done yet), and being packaged by your distribution.

### Across all three

| | The built-in apps | kinoz |
|---|---|---|
| Formats | Common files; some need extensions, converters or store downloads | Almost anything out of the box (libmpv and FFmpeg) |
| Clicking through many files | Fine one at a time | Only the last click opens; the next file is pre-read; instant hover preview; measured 0.3 s to a picture |
| Cleaning up a folder | Open each file, delete by hand | `Y` keep, `N` reject, `U` next unseen, `Delete` to the Recycle Bin, watched marks |
| Look | Follows the system | One look on every system, light and dark, and a real animations-off switch |
| Cost, ads, accounts | Tied to the OS and its services | Free, GPLv3, no account, no telemetry |
| Your data | Library and history often live in a system or cloud service | Plain files in kinoz's own folder |
| Updates | With the OS | Its own: download in the background, then restart now or later, or switch it off |

**Not in kinoz, and not planned:** casting to TVs and speakers, CD and DVD features, streaming-service integration, cloud sync.

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## What you get

| | |
|---|---|
| Every kind of media | MKV, MP4, AVI, WebM, HEVC, FLAC, MP3, JPG, PNG, WebP, GIF and many more, with SRT and ASS subtitles. No codec packs. |
| A library that stays out of the way | Folders, thumbnails (cached on disk), list or grid, type filter, search, sort, queue, resume position per file, recent items. |
| Fast skimming | Click through files or spam Next and only the last one opens (measured: 150 clicks, 2 files opened, UI never frozen more than 25 ms). Keep or reject files with `Y` / `N`, jump with `1`-`9`, see what you have already watched, and delete rejects to the Recycle Bin in one go. |
| Music library | Artists, Albums, Songs and Playlists, read from your folders' tags once and cached. Saved playlists are plain `.m3u8` files; `.m3u`, `.m3u8` and `.pls` open as a queue. |
| Windows media overlay | The now-playing card in the volume flyout and on the lock screen, with play, pause, next, previous and seeking. |
| Default player | One button in Settings (and `Ctrl+K`) registers kinoz with Windows and opens its Default apps page. |
| Setup that looks after itself | A setup window drawn by kinoz: verifies its files, rolls back on failure, repairs with the Doctor, removes everything (optionally your data too) in one switch. Safe mode starts automatically after two abnormal exits. |
| `Ctrl+K` command palette | Find any file or command from the keyboard. |
| Seek-bar frame preview | Hover the timeline to see the frame you are about to jump to; it is served from a background-built strip, so it never waits. |
| Playback tools | Speed with pitch correction, frame step, A-B loop, sleep timer, aspect ratio, audio and subtitle delay, track menus. |
| Sound | 10-band equalizer with presets, loudness normalizer, output-device picker. |
| Picture | Brightness, contrast, saturation, gamma and hue, live. |
| Images and GIFs | Zoom, pan, rotate, browse a folder with the arrow keys, optional slideshow. |
| Annotation | Draw on a paused frame or an image, export a full-resolution PNG or copy it. |
| Windows and modes | Fullscreen, a mini always-on-top player, several independent windows, "Safe fullscreen" if your display blinks. |
| Look | Light and dark theme, six accent colours, glass effect, compact density, animations System / On / Off. |
| Settings that explain themselves | Six tabs: General, Playback, Shortcuts, Info (system, graphics, engine, current file, copy diagnostics), About, Legal. |
| Resilient | A broken or missing file is skipped with a clear message instead of stopping the queue. Plays local files only, with no external tools or scripts run. |
| Offline | No account, no telemetry. An optional, SHA-256-verified update check (Automatic / Notify me / Off) is the only network request. |

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## Supported file types

kinoz recognises these in the library and in "Open with". The engine can also play other formats you drop onto it.

| Kind | Extensions |
|---|---|
| Video | `mp4` `m4v` `mkv` `avi` `webm` `mov` `wmv` `flv` `ts` `m2ts` `mts` `mpg` `mpeg` `3gp` `ogv` `vob` `divx` `f4v` `asf` `rm` `rmvb` |
| Audio | `mp3` `flac` `wav` `ogg` `oga` `opus` `m4a` `aac` `wma` `aiff` `aif` `ape` `alac` `mka` `wv` `mpc` `dsf` `ac3` `dts` |
| Images | `jpg` `jpeg` `png` `bmp` `tif` `tiff` `webp` `ico` `jfif`, and `avif` `heic` `heif` `jxl` where the bundled FFmpeg supports them |
| Animated | `gif` `apng` |
| Subtitles | `srt` `ass` `ssa` `vtt` `sub` |

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## Keyboard shortcuts

The full list is in **Settings → Shortcuts**, and you can add your own in `keymap.json`. The ones to know:

| Key | Action | Key | Action |
|---|---|---|---|
| `Space` / `Ctrl+P` | Play / pause | `Ctrl+K` | Search and commands |
| `Left` / `Right` | Back / forward (small step) | `F` / `Alt+Enter` | Fullscreen |
| `Shift+Left` / `Shift+Right` | Back / forward (large step) | `Ctrl+M` | Mini player |
| `Up` / `Down` | Volume | `Ctrl+N` | New window |
| `M` / `F7` | Mute | `Tab` | Show / hide the library |
| `]` / `[` | Faster / slower | `A` | Annotate this frame |
| `.` / `,` | Next / previous frame | `B` | A-B loop |
| `Ctrl+B` / `Ctrl+F` | Previous / next | `Ctrl+E` / `Ctrl+J` | Equalizer / adjust picture |
| `Ctrl+H` / `Ctrl+T` | Shuffle / repeat | `Ctrl+D` | Switch theme |
| `Ctrl+Shift+S` | Save this frame as an image | `Ctrl+,` | Settings |
| `1` ... `9` | Jump to 10% ... 90%, `0` to the start | `` ` `` | Show / hide the library (also `Tab`) |
| `Y` / `N` | Keep / mark to reject | `U` | Next file I haven't seen |
| `Delete` | Move this file to the Recycle Bin (asks first) | | |

On an image: mouse wheel or `=` / `-` zooms, drag pans, `R` rotates, `0` fits to the window.

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## Updates and uninstall

**Updates (Windows).** About 8 seconds after you start kinoz it checks GitHub, at most once a day. In **Automatic** mode (the default) a new version downloads in the background with a progress bar, is checked against its published SHA-256, and kinoz asks **Restart now / Later**. If you pick Later, it installs by itself the next time you open kinoz. **Settings, About, Updates** switches between **Automatic**, **Notify me** and **Off**, and **Download page** and **Direct download** are always one click away. A failed update reopens the version you had. On macOS and Linux the update button opens the release page.

**Uninstall (Windows)**, any one of these: **Settings, General, Uninstall...**, or `Ctrl+K` and type "uninstall", or Windows **Settings, Apps, Installed apps, kinoz, Uninstall**. Leave **Delete all my data too** on to remove everything (settings, resume points, thumbnails, logs, exported frames); your own media files are never touched. Silent: `kinoz.exe --uninstall --silent --delete-data`. macOS: drag kinoz out of Applications. Linux: delete the AppImage. The Windows setup also has a **Doctor** that checks and repairs an installation: run `kinoz.exe --doctor` or use `Ctrl+K` and "Doctor".

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## FAQ

**Does it send anything about me?** No accounts, no analytics, no crash reports. The only network request kinoz can make is the optional update check to `github.com` (a plain request for the latest release). Turn it off under Settings, About, Updates, Off. Details: [PRIVACY.md](PRIVACY.md).

**Where does it keep my settings?** In a `data` folder inside the install folder, so nothing is spread over `C:`. **Settings, General, Open data folder** opens it. Delete a file there to reset that part.

**Does it change or delete my media?** Never on its own. `Delete` moves the current file to the Recycle Bin and always asks first.

**Can I use it at work, or in a company?** Yes. It is GPLv3: free for any use. If you share a modified version, keep it GPLv3, keep the credit and give it a different name.

**Which formats?** Almost anything FFmpeg opens. The common ones are listed [below](#supported-file-types), and you can drop other files onto it.

**Why does the Windows setup look different?** It is drawn by kinoz itself instead of the stock wizard, with the same look, a built-in Doctor and a one-click full uninstall.

**Can I add my own shortcuts?** Yes, in `keymap.json`. The current ones are listed under Settings, Shortcuts.

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## Troubleshooting

| Problem | Try |
|---|---|
| The screen blinks or goes black going fullscreen | **Settings, General, Safe fullscreen**. |
| Choppy playback or a graphics glitch | **Settings, General, Video decoding, CPU**, then reopen the file. |
| kinoz closed unexpectedly twice | It starts in **safe mode** by itself (software decoding, no glass). Change it back in Settings. |
| A file will not play | Copy the details with **Settings, Info, Copy diagnostics** and [open an issue](https://github.com/p4inz-code/kinoz/issues/new/choose). |
| Something in the install looks broken | `kinoz.exe --doctor` (Windows) checks files, shortcuts, right-click entries and settings, and fixes what it can. |
| Start over | Close kinoz and delete the `data` folder next to it, or use Uninstall with "Delete all my data too". |

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## Privacy

kinoz works fully offline. Settings, resume points, thumbnails and exports are stored in its data folder on your own computer and nothing is sent anywhere. The only network request it can make is the optional update check to `github.com` (a plain request for the latest release, no user data), which you can turn off in Settings → About. Full details: [PRIVACY.md](PRIVACY.md).

## Build from source

Everything needed is in this repository. Qt and libmpv are downloaded by a script, not committed.

**Windows** (Visual Studio with the C++ workload):
```powershell
powershell -ExecutionPolicy Bypass -File scripts\setup-toolchain.ps1   # once: Qt 6.8, libmpv, Inno Setup into .toolchain\
scripts\build.ps1                                                      # builds to dist\app
scripts\release.ps1                                                    # builds + the installer
```

**macOS:** `brew install qt mpv pkg-config cmake`, then `cmake -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt) && cmake --build build`

**Linux:** `sudo apt install qt6-base-dev libgl-dev libmpv-dev pkg-config cmake`, then `cmake -B build && cmake --build build`

More in [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md). A built-in self-test (`kinoz --selftest <file>`) drives every control and checks the result.

## Documentation

[Known issues](docs/KNOWN_ISSUES.md) · [Changelog](CHANGELOG.md) · [Roadmap](ROADMAP.md) · [Architecture](docs/ARCHITECTURE.md) · [Design notes](docs/DESIGN.md) · [Performance notes](docs/PERFORMANCE.md) · [Privacy](PRIVACY.md) · [Security](SECURITY.md) · [Support](SUPPORT.md) · [Contributing](CONTRIBUTING.md)

## Support and feedback

- Bugs and ideas: [open an issue](../../issues/new/choose)
- Chat: [Discord](https://discord.gg/8UKt8s5FbW)
- Security problems: see [SECURITY.md](SECURITY.md) and report privately

## License and credit

kinoz is **free software** under the **GNU General Public License v3** ([LICENSE](LICENSE)), with a few additional terms about credit and naming ([ADDITIONAL_TERMS.md](ADDITIONAL_TERMS.md)).

In plain words:

- **You may** use kinoz for anything, free of charge, including at work; study it; change it; and share it, modified or not.
- **You must** keep the author credit ("Created by Atharva Patil, P4inz Interactive Labs") and the copyright notices, keep the source open under the same GPLv3 license if you share a version, and give a modified version its own name instead of "kinoz".
- **Nobody can** take it closed-source or sell it as their own work.

Every source file carries the copyright and `SPDX-License-Identifier: GPL-3.0-only` line, and the About screen shows the credit.

Third-party components keep their own licenses: [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).

Copyright © 2026 Atharva Patil.

---

Built by [P4inz Interactive Labs](https://github.com/p4inz-code).

---

<!-- SUPPORT-BLOCK:START -->
<div align="center">

### Support my work

I make these tools on my own and keep them free. If one helped you, you can chip in by UPI. Any amount.

<a href="https://p4inz-code.github.io/donate/"><img src="https://raw.githubusercontent.com/p4inz-code/donate/main/qr.svg" alt="UPI QR code. Scan it with any UPI app." width="200"></a>

`9321614988@jio`

On your phone? [Open the donation page](https://p4inz-code.github.io/donate/).

Outside India, or prefer a card?

<a href="https://buymeacoffee.com/p4inz"><img src="https://img.shields.io/badge/Buy%20me%20a%20coffee-p4inz-FFDD00?style=for-the-badge&logo=buymeacoffee&logoColor=black" alt="Buy me a coffee"></a>

</div>
<!-- SUPPORT-BLOCK:END -->
