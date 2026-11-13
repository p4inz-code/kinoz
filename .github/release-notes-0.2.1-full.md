## What's new in 0.2.1

**Easier to use**
- Settings (a gear) and the **⋯** menu are always in the title bar, even on the empty first screen.
- Hover any icon for a second and a small label shows its name and shortcut.
- While a video plays, the controls and the library fade away after two seconds, so the picture gets the whole window. Move the mouse for the controls, touch the left edge for the library.
- The start screen has a "Make kinoz your default" button.

**Private when you want it**
- New **Settings → Privacy** page: Private mode, Forget on exit, and **Clean identity**, which wipes recent files, resume points, marks, thumbnails, the music index and logs in one click. Each kind of history has its own clear button, files have **Forget this file**, and `kinoz --wipe` does the same from a terminal. History is still saved by default.

**macOS and Linux**
- macOS: opening files from Finder and "Open With", an app icon and document types, traffic-light window buttons, ⌘ shortcut labels, Now Playing and media keys.
- Linux: a fuller desktop entry, "Make kinoz your default" from an AppImage, and MPRIS (media keys and the desktop's now-playing card).
- Fixed: the playback engine could refuse to start under a locale that writes decimals with a comma, and files would not open at all with libmpv older than 0.38 (Ubuntu 22.04 ships 0.34).

**Also**: a 30-second video tour and a clearer banner in the README.

## Which file do I download?

Pick the one for your system. You only need one.

- **Windows 10 or 11:** `kinoz-Setup-<version>.exe`. Run it. It installs for your user only, needs no administrator rights, and offers a drive other than `C:`.
- **Mac with Apple Silicon (M1 and later):** `kinoz-<version>-macos.dmg`. Open it and drag kinoz to Applications.
- **Linux, 64-bit Intel or AMD:** `kinoz-<version>-x86_64.AppImage`. Make it executable (`chmod +x`), then run it. No install needed.

The `.sha256` files are checksums, not programs. You can ignore them unless you want to check your download: the in-app updater uses them, and so can you (`sha256sum <file>` on Linux, `shasum -a 256 <file>` on a Mac, `Get-FileHash <file>` in PowerShell).

Intel Macs, ARM Linux and 32-bit systems don't have a build yet.

## Good to know

- Windows is the platform I test hardest. The macOS and Linux builds are built and smoke-tested by CI but have not been used on real machines yet. If you try one, a [test report](https://github.com/p4inz-code/kinoz/issues/new?template=platform_test.md) is the most useful thing you can send. What to try is in [docs/TESTING.md](https://github.com/p4inz-code/kinoz/blob/main/docs/TESTING.md).
- kinoz sends nothing about you anywhere. The only network request is the optional update check ([privacy](https://github.com/p4inz-code/kinoz/blob/main/PRIVACY.md)).
- Known gaps are listed in [docs/KNOWN_ISSUES.md](https://github.com/p4inz-code/kinoz/blob/main/docs/KNOWN_ISSUES.md).

The full list of changes is in [CHANGELOG.md](https://github.com/p4inz-code/kinoz/blob/main/CHANGELOG.md). The automatic list of commits follows below.
