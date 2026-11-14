# Maintaining kinoz

Everything you need to keep kinoz healthy, learned the hard way. For releases, follow [RELEASING.md](RELEASING.md).

## Rebuild from a fresh clone

1. `git clone https://github.com/p4inz-code/kinoz.git`
2. `powershell -ExecutionPolicy Bypass -File scripts\setup-toolchain.ps1` downloads Qt 6.8.3 (qtbase only, through aqtinstall) and the pinned libmpv build (tag `20261111`) into `.toolchain\`, and checks the libmpv checksum. It needs Visual Studio with the C++ workload; Inno Setup comes through winget.
3. `scripts\build.ps1` builds to `dist\app` (limited to four cores on purpose).
4. `scripts\release.ps1` builds, refreshes the screenshots, packs the payload and compiles `Setup.exe` with its `.sha256`.

Nothing is installed outside the project folder. `.toolchain`, `build` and `dist` are generated and ignored by git. Every released installer is also on the Releases page.

## Keeping it safe over time

kinoz opens untrusted media through libmpv and FFmpeg, so their security fixes are the main ongoing risk. Every few months, bump the libmpv tag and asset name in `scripts/setup-toolchain.ps1` (new checksum included), rebuild, and run the self-test and the benchmark. macOS and Linux CI take libmpv from Homebrew and apt at build time, so they pick up new versions by themselves.

## Testing

Run everything silent and off-screen, and never toggle fullscreen on a real display unless you mean to. These variables keep it quiet:

```powershell
$env:KINOZ_SILENT='1'; $env:KINOZ_OFFSCREEN='1'; $env:KINOZ_DATA_DIR="$PWD\.toolchain\tmp\data"
dist\app\kinoz.exe --selftest <a video of 10 seconds or more>
dist\app\kinoz.exe --bench <a video>
```

The self-test runs 120 checks and writes `data\logs\selftest.txt`. With a very short clip alone in its folder, four checks fail for that reason only: pause and resume race with the end of the clip, and thumbnails and the index need other files in the folder. The benchmark writes `data\logs\bench.txt` and prints PASS or FAIL against budgets (see [PERFORMANCE.md](PERFORMANCE.md)). `--shots <dir>` and `--installer-shots <dir>` render the screenshots; `scripts/refresh-visuals.ps1` runs them with generated demo media. `KINOZ_TEST_FULLSCREEN` and `KINOZ_TEST_HIRES` exist for the two tests that touch the real display or stress the GPU, and are off by default.

## Things that bit us

- **Exit crash.** libmpv aborts if its core is destroyed while a render context is alive. Qt deletes children in creation order, so `MainWindow` frees the video widget first. Keep it that way.
- **Blocking mpv calls.** `mpv_get_property` blocks until the core answers, and the core is busy while it opens a file. Keep reads on the UI thread to cached values.
- **Settings writes.** They happen on one background thread, because a disk flush on the UI thread can take hundreds of milliseconds. Closing the app flushes synchronously.
- **C++/WinRT and Qt.** In `src/core/Smtc.cpp` the WinRT headers must come before any Qt header, and the first `#ifdef` must use `_WIN32`, since `Q_OS_WIN` isn't defined yet at that point.
- **macOS link error.** Current macOS SDKs removed the `AGL` framework. `CMakeLists.txt` sets an empty `__opengl_agl_fw_path` and strips AGL from the Qt and mpv link metadata.
- **Network paths.** Never call `QFileInfo::exists` or open a path that came from a playlist before checking it is local.
- **Taskbar and the right-click menu.** The "Play with kinoz" entries use the classic registry verbs, which Windows 11 shows under "Show more options". The new top-level menu needs a packaged identity.
- **Setup and a running app.** Setup closes a running kinoz that lives in the same folder (a polite close first, an end-process after six seconds), excluding itself.
- **CI queue.** The macOS runner can wait several minutes in GitHub's queue before the three-to-five minute build starts. That's normal.

## CI and releases

`.github/workflows/build.yml` builds Windows, macOS and Linux on every push. A tag like `v0.2.1` also runs a `visuals` job that fails the release if the screenshots and the README banner did not change since the previous tag (see RELEASING.md), then publishes the Windows installer, the macOS dmg and the Linux AppImage with checksums. The in-app updater looks for exactly those file names. Dependabot opens version-bump PRs for the actions; the artifact upload and download bumps change how files pass between jobs, so test them on a branch before merging.

## Rules worth keeping

- Every release refreshes the screenshots and the README (`scripts/refresh-visuals.ps1`, `scripts/make-hero.ps1`).
- Deleting a user's file always asks and goes to the Recycle Bin.
- Playlists and paths that come from outside accept local files only.
- Updates only install a file whose SHA-256 matches, checked again right before installing. The updater never opens a web page; failures are shown in the app. Every decision is logged under `[updater]` in `logs/app.log` (the last two runs are kept as `app.previous.log`, `app.previous2.log`). `kinoz --doctor` / `--doctor-text` / Health check (Ctrl+K) report the state of settings, crash files, update leftovers and the engine.
- The credit stays: author attribution, copyright notices, the SPDX header on every source file, and a different name for modified versions.
