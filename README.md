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

You have films in one folder, music in another, a pile of GIFs and screenshots somewhere else, and the player that opens them is slow to start, forgets where you stopped and makes you hunt through a file dialog every time. kinoz is one window for all of it: your folders in a sidebar, thumbnails, a frame preview when you hover the seek bar, and a command palette that finds any file or command in a keystroke.

It plays almost anything with no codec packs, because the engine underneath is libmpv and FFmpeg. The whole interface is drawn by kinoz itself: no OS title bar, no OS menus, no OS dialogs, a light and a dark theme, and a real switch that turns every animation off. There is no account, no cloud and no telemetry.

<table>
<tr>
<td width="50%" align="center">
<sub>Dark theme</sub><br><br>
<img src="assets/screenshots/home-dark.png" alt="kinoz in the dark theme: library sidebar and a drop target" width="100%">
</td>
<td width="50%" align="center">
<sub>Light theme</sub><br><br>
<img src="assets/screenshots/home-light.png" alt="kinoz in the light theme" width="100%">
</td>
</tr>
</table>
<p align="center"><sub>First-run screens, rendered by the app itself. <code>Ctrl+D</code> switches theme, no restart needed.</sub></p>

<details>
<summary>More screenshots: command palette, Settings, About</summary>
<br>

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
  <a href="#download-and-install">Download</a> &nbsp;|&nbsp;
  <a href="#what-makes-kinoz-different">What is different</a> &nbsp;|&nbsp;
  <a href="#what-you-get">What you get</a> &nbsp;|&nbsp;
  <a href="#supported-file-types">File types</a> &nbsp;|&nbsp;
  <a href="#keyboard-shortcuts">Shortcuts</a> &nbsp;|&nbsp;
  <a href="#build-from-source">Build from source</a>
</p>

> **Status: early (0.1).** The Windows build is the one that has been run and tested hardest. The macOS and Linux builds come from CI and have not been tried by hand yet, so they may need fixes. [Known issues](docs/KNOWN_ISSUES.md) is the honest list.

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## Download and install

Pick the file for your system from the [latest release](https://github.com/p4inz-code/kinoz/releases/latest). There is no account, no sign-in and nothing to configure.

| Your system | Download | Notes |
|---|---|---|
| Windows | `kinoz-Setup-<version>.exe` | The installer, drawn by kinoz itself (no stock wizard). Per-user, no administrator rights, non-`C:` folder by default. Right-click "Play with kinoz", "Open with", Start Menu shortcut. Checks every file against a SHA-256 list before copying, rolls back on failure, and has a built-in **Doctor** (check and repair) and a one-switch **Remove kinoz + all my data**. Everything kinoz saves stays inside the install folder. |
| macOS, Apple Silicon (M1 and newer) | `kinoz-<version>-macos.dmg` | Open it and drag kinoz into Applications. |
| Linux | `kinoz-<version>-x86_64.AppImage` | `chmod +x` it and run it. |

Each file has a `.sha256` next to it. The in-app updater (optional) only installs a download whose SHA-256 matches. Your first five minutes: open kinoz, press **Add a folder**, point it at where your media lives, and click anything. Press `Ctrl+K` to search, `?`-style help is in **Settings → Shortcuts**.

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## What makes kinoz different

1. **One player for everything.** Video, audio, images and animated GIFs open in the same window and the stage adapts to what you play: letterboxed video, a cover-art now-playing card for music, a zoom-and-pan viewer for pictures, a looping player for GIFs.
2. **Real glass, not a screenshot of glass.** The floating control bar blurs the video behind it, live, on the GPU, with a slow liquid-metal sheen and a rim light. Prefer it flat? One switch. Prefer nothing moving at all? **Settings → Animations → Off** removes every fade and glow.
3. **Your library is one keystroke away.** A sidebar with your folders, thumbnails, list or grid view, a type filter, search and a drag-reorderable queue. `Ctrl+K` searches every file under your folders and every command. Resume continues each video from where you left off.
4. **Built for heavy use.** It opens and closes instantly (the window appears before the engine finishes starting), keeps working when you click fast, runs several independent windows at once (`Ctrl+N`), and a minimized video keeps its sound while dropping the picture to save CPU. 4K and 8K frames decode and render in the tests.
5. **Tools you actually reach for.** Annotate any frame or image with a pen, highlighter, arrows, shapes and text and export a full-resolution PNG. A-B loop, sleep timer, aspect ratio, audio and subtitle sync, a 10-band equalizer with presets, picture adjust, speed 0.25×–4× with pitch correction.
6. **Free, offline and yours.** GPLv3, so it stays free and the credit stays. No account, no telemetry; the only network request it can ever make is the optional update check, and you can switch it off.

<p align="center"><img src="assets/readme/divider.svg" width="70%" alt=""></p>

## What you get

| | |
|---|---|
| Every kind of media | MKV, MP4, AVI, WebM, HEVC, FLAC, MP3, JPG, PNG, WebP, GIF and many more, with SRT and ASS subtitles. No codec packs. |
| A library that stays out of the way | Folders, thumbnails (cached on disk), list or grid, type filter, search, sort, queue, resume position per file, recent items. |
| `Ctrl+K` command palette | Find any file or command from the keyboard. |
| Seek-bar frame preview | Hover the timeline to see the frame you are about to jump to. |
| Playback tools | Speed with pitch correction, frame step, A-B loop, sleep timer, aspect ratio, audio and subtitle delay, track menus. |
| Sound | 10-band equalizer with presets, loudness normalizer, output-device picker. |
| Picture | Brightness, contrast, saturation, gamma and hue, live. |
| Images and GIFs | Zoom, pan, rotate, browse a folder with the arrow keys, optional slideshow. |
| Annotation | Draw on a paused frame or an image, export a full-resolution PNG or copy it. |
| Windows and modes | Fullscreen, a mini always-on-top player, several independent windows, "Safe fullscreen" if your display blinks. |
| Look | Light and dark theme, six accent colours, glass effect, compact density, animations System / On / Off. |
| Settings that explain themselves | Six tabs: General, Playback, Shortcuts, Info (system, graphics, engine, current file, copy diagnostics), About, Legal. |
| Resilient | A broken or missing file is skipped with a clear message instead of stopping the queue. Plays local files only, with no external tools or scripts run. |
| Offline | No account, no telemetry. An optional, SHA-256-verified update check is the only network request. |

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

On an image: mouse wheel or `=` / `-` zooms, drag pans, `R` rotates, `0` fits to the window.

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

In plain words: use it for anything, change it, share it. If you share a version, its source stays open under the same license, the author credit stays, and a modified version gets its own name. Nobody can take it closed-source.

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
