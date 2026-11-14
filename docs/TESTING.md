# Testing kinoz on macOS and Linux

kinoz is built and smoke-tested on macOS and Linux by CI on every push, but nobody has used those builds on a real machine yet. If you have a Mac or a Linux desktop, a few minutes of your time is the most useful thing you can give the project.

## What to try

1. Install it: the `.dmg` on a Mac (`-macos.dmg` for Apple Silicon, `-macos-intel.dmg` for an Intel Mac), or the `.AppImage` on Linux (`-x86_64` for PCs, `-aarch64` for ARM; `chmod +x`, then run it).
2. Open a video, a song and a picture, from the window and from your file manager ("Open with").
3. Move and resize the window, go fullscreen with `F` and back, and try the mini player (`Ctrl+M`, `Cmd+M` on a Mac).
4. Press the media keys. On macOS the Now Playing card appears in Control Center. On Linux, GNOME and KDE show a media card, and `playerctl status` works.
5. Click **Make kinoz your default** (the start screen, or **Settings → General**) and see whether double-clicking a video opens kinoz.
6. Open **Settings** from the gear and try the **Privacy** page: Private mode, then Clean identity.

## Reporting

Open a [macOS or Linux test report](https://github.com/p4inz-code/kinoz/issues/new?template=platform_test.md). Tick what works and describe what doesn't. A screenshot and the file `logs/app.log` from the data folder (path shown in **Settings → Privacy**) help a lot.

## Known gaps

These are listed in [KNOWN_ISSUES.md](KNOWN_ISSUES.md). The ones most likely to show up on your machine: window move and resize use the system's own handling on macOS and Linux and may need fixes, the media integration has only been compiled, not run, and the Wayland compositors differ in how they treat a frameless window.

## For maintainers

The CI jobs for macOS and Linux run `kinoz --selftest` on a generated clip as an informational step (it never blocks a release). The numbers there are not comparable to Windows, because CI has no real GPU or sound device.
