<h1 align="center">kinoz</h1>

<p align="center">
  <img src="assets/screenshots/home-dark.png" alt="kinoz first-run screen in the dark theme: library sidebar and a drop target" width="100%">
</p>

![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-0c6a6e)
![Status](https://img.shields.io/badge/status-early%20(0.1)-orange)
![License](https://img.shields.io/badge/license-GPLv3-blue)

**A calm, fast, offline media player for videos, music, images and GIFs.**

kinoz plays almost anything with no codec packs, keeps your library one click away in a sidebar, and gets out of the way while you watch. The whole interface is custom (no OS title bar, menus or dialogs) with a light and a dark theme. No account, no cloud, no telemetry.

> **Status: early (0.1).** The Windows build is the one that has been run and tested. The macOS and Linux builds are produced by CI and **have not been tested on real machines yet**; they may need fixes. See [Known issues](docs/KNOWN_ISSUES.md) for the honest list.

---

## Screenshots

These are the app's own screens, rendered by the app itself (the first-run state, with no media loaded).

<table>
  <tr>
    <td width="50%">
      <img src="assets/screenshots/home-light.png" alt="Light theme: library sidebar and drop target" width="100%">
      <p align="center"><em>Light theme</em></p>
    </td>
    <td width="50%">
      <img src="assets/screenshots/palette-dark.png" alt="Command palette searching your library and every command" width="100%">
      <p align="center"><em><code>Ctrl+K</code>: search your library and every command</em></p>
    </td>
  </tr>
  <tr>
    <td width="50%">
      <img src="assets/screenshots/settings-light.png" alt="Settings" width="100%">
      <p align="center"><em>Settings</em></p>
    </td>
    <td width="50%">
      <img src="assets/screenshots/about-light.png" alt="About: version, engine, license, credit, updates" width="100%">
      <p align="center"><em>About: everything in one place</em></p>
    </td>
  </tr>
</table>

---

## What kinoz does

- **One player for everything.** Video, audio, images and animated GIFs open in the same window; the stage adapts to what you play. Built on libmpv/FFmpeg, so MKV, MP4, AVI, WebM, HEVC, FLAC, MP3, SRT, ASS and most other formats just work.
- **A library that stays out of the way.** A sidebar with your folders, thumbnails, list or grid view, a type filter, search, sort and a drag-reorderable queue. Hover the seek bar to see a frame preview. Resume picks up where you left off, per file.
- **`Ctrl+K` command palette.** Search your whole library and every command from the keyboard.
- **Playback tools.** Speed 0.25×–4× with pitch correction, frame step, subtitle and audio-track menus, subtitle delay and size, picture adjust (brightness, contrast, saturation, gamma, hue), a 10-band equalizer with presets, a loudness normalizer, output-device picker.
- **Images and GIFs.** Zoom, pan, rotate, and browse a folder with the arrow keys.
- **Annotate any frame.** Pause or open an image, draw with pen, highlighter, arrow, shapes and text, then export a full-resolution PNG or copy it.
- **Glass and metal, calmly.** The floating control bar blurs the video behind it; accents are liquid-metal. Light and dark follow your system, or pick one.
- **Built for heavy use.** Several independent windows (`Ctrl+N`), a mini always-on-top player, a minimized video keeps its sound but stops decoding the picture, a broken file is skipped instead of stopping the queue, and a built-in `--selftest`.
- **Familiar keys.** Windows Media Player-style shortcuts, editable in a JSON file.
- **Updates you control.** An optional update check against this repository's Releases, verified with SHA-256, installed only when you press the button.

## What kinoz does *not* do

Being upfront:

- No accounts, no cloud sync, no streaming-service integrations, no ads, no telemetry, no crash reporting.
- No code signing yet, so Windows SmartScreen and macOS Gatekeeper warn on first run. Verify the SHA-256 from the release page.
- No system "now playing" integration yet (Windows flyout, macOS Now Playing, Linux MPRIS). Media keys do work.
- No gapless playback yet. A tiny gap can occur between tracks.
- macOS and Linux are not tested by hand yet (see the status note above).

## Download

Releases are published on the [Releases](../../releases) page: a Windows installer (`kinoz-Setup-<version>.exe`), a macOS `.dmg` and a Linux `.AppImage`, each with a `.sha256` file. The first release is being prepared.

On Windows the installer offers a non-`C:` install folder by default and keeps everything kinoz saves inside the install folder.

## Privacy

kinoz works fully offline. It stores your settings, resume points and thumbnails locally, in its data folder, and sends nothing anywhere. The only network request it can make is the optional update check to `github.com` (no user data, can be turned off in Settings → About). Full details: [PRIVACY.md](PRIVACY.md).

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

More in [CONTRIBUTING.md](CONTRIBUTING.md) and [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md).

## Documentation

- [Known issues](docs/KNOWN_ISSUES.md)
- [Changelog](CHANGELOG.md)
- [Roadmap](ROADMAP.md)
- [Design notes](docs/DESIGN.md) and [performance notes](docs/PERFORMANCE.md)
- [Privacy](PRIVACY.md) · [Security](SECURITY.md) · [Support](SUPPORT.md) · [Contributing](CONTRIBUTING.md)

## Support & feedback

- Bugs and ideas: [open an issue](../../issues/new/choose)
- Chat: [Discord](https://discord.gg/8UKt8s5FbW)
- Security problems: see [SECURITY.md](SECURITY.md) (please report privately)

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
