# Development

How the repository is laid out and how to rebuild everything from a fresh clone. The repository is the complete backup: delete your working copy and nothing is lost.

## Restore on a new Windows machine
1. Install **Visual Studio** (Desktop development with C++) and **Python 3** and **Git**.
2. `git clone https://github.com/p4inz-code/kinoz.git`
3. `cd kinoz` then `powershell -ExecutionPolicy Bypass -File scripts\setup-toolchain.ps1`
   Downloads Qt 6.8.3 (via aqtinstall), a pinned libmpv build (SHA-256 checked) and Inno Setup into `.toolchain\` (git-ignored). Nothing is written outside the folder.
4. `scripts\build.ps1` → `dist\app\kinoz.exe`
5. `scripts\release.ps1` → `dist\installer\kinoz-Setup-<version>.exe`

## Layout
| Path | What |
| --- | --- |
| `src/app` | `main.cpp`: startup, single-instance, command-line flags |
| `src/core` | player wrapper (`MpvPlayer`), app logic (`PlayerController`), settings store, updater, paths |
| `src/ui` | main window, custom widgets, control bar, sidebar, overlays, annotation, video surface (GL + glass) |
| `src/library`, `src/thumbs`, `src/media` | folder model, thumbnail and preview services, media-type detection |
| `resources/` | fonts (SIL OFL), keymaps, icon |
| `packaging/` | Windows installer script, Linux desktop entry |
| `scripts/` | toolchain setup, build, release |
| `.github/workflows/build.yml` | CI for Windows, macOS, Linux; tag `v*` publishes a Release |
| `docs/` | design, performance, known issues |

## Command-line flags
`--new` (extra independent window) · `--action=<id>` (send a command to the running window) · `--selftest <file>` · `--shots <dir>`.
Environment: `KINOZ_SILENT=1` (no sound), `KINOZ_OFFSCREEN=1` (window off-screen), `KINOZ_DATA_DIR=<dir>` (use another data folder).

## Releasing
1. Update the version in `CMakeLists.txt`, `packaging/installer.iss`, `.github/workflows/build.yml`, and add a section to `CHANGELOG.md`.
2. Commit to `main`, then `git tag v<version>` and `git push --tags`.
3. CI builds all three platforms, attaches the installers plus a `.sha256` for each, and creates the GitHub Release. The in-app updater looks for exactly those names.

## Branching
`main` is always releasable. Work happens on short-lived branches (`feat/…`, `fix/…`) and lands through pull requests; commit messages follow [Conventional Commits](https://www.conventionalcommits.org/).
