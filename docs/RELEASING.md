# Releasing kinoz

Every release ships fresh visuals. This is a rule, not a nicety: the repository pictures must match the app people download. CI enforces it (the `visuals` job fails a `v*` tag if `assets/screenshots` and `assets/readme/hero.png` did not change since the previous tag).

## Checklist

1. **Code and tests**
   - `.\scripts\build.ps1`
   - `KINOZ_SILENT=1 KINOZ_OFFSCREEN=1 kinoz --selftest <a video>` and `kinoz --bench <a video>` (see docs/PERFORMANCE.md).
2. **Visual assets (never skip)**
   - `.\scripts\refresh-visuals.ps1` regenerates every screenshot, silently and off-screen, from demo media it generates itself (no third-party content): main window, Settings pages, About, palette, library marks, Music, playlists, queue, and every setup page.
   - `.\scripts\make-hero.ps1` rebuilds `assets/readme/hero.png` from them.
   - `python scripts\make-promo.py` rebuilds the 30-second tour (`assets/readme/promo.mp4` and its poster) from the same screenshots. It needs Pillow and ffmpeg. Update the captions in the script when a feature changes.
   - Look at the pictures. If a new feature has no picture yet, add a step to `MainWindow::runShots` (screens) or `Installer.cpp` (`--installer-shots`).
   - Update the README (features table, screenshots, shortcuts) and `CHANGELOG.md` for everything new.
3. **Version**: bump `CMakeLists.txt`, `packaging/installer.iss`, `resources/app.rc`, `.github/workflows/build.yml` (`APP_VERSION`) and the top `CHANGELOG.md` heading.
4. **Build the installer**: `.\scripts\release.ps1` (builds, refreshes the visuals, packs the payload, compiles Setup, writes the `.sha256`).
5. **Commit, push, wait for CI green on all three platforms**, then tag `vX.Y.Z` and push the tag. The release job attaches the Windows installer, the macOS dmg and the Linux AppImage with checksums.
6. Check the release page lists all six files.
