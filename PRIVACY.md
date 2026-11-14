# Privacy

**kinoz does not collect, send or sell any information about you.** There are no accounts, no analytics, no ads and no crash reporting.

## What stays on your computer
Everything kinoz remembers is stored locally, in its data folder (shown in Settings → About):

| What | Why | Where |
| --- | --- | --- |
| Settings (theme, volume, window size, places you added, equalizer…) | so it looks the way you left it | `settings.json` |
| Resume positions | so a video continues where you stopped | `resume.json` |
| Recently played list | for the start screen and `Ctrl+K` | inside `settings.json` |
| Thumbnails | so folders load fast | `cache/thumbs/` (size-capped, "Clear thumbnails" in Settings) |
| Frames and images you export | you asked for them | `exports/` |
| A small log | to diagnose problems | `logs/app.log` (never uploaded) |

Delete the data folder and all of it is gone. kinoz only reads the folders you add or files you open.

## Wiping what kinoz remembers
History is saved by default, because most people want "continue where I stopped". When you want less, **Settings → Privacy** has everything in one place:

- **Private mode** (also in the top bar's "⋯" menu): while it is on, kinoz does not record recent files, resume points, review marks or thumbnails to disk. A small "Private" tag shows in the title bar so you always know. It is off by default.
- **Forget on exit**: wipes the history below every time kinoz closes.
- **Clean identity**: one button that forgets everything kinoz has learned about you (recent files, resume points, marks, thumbnails, the music index, logs and the last folder). It keeps your settings, the folders you added, your playlists and anything you exported.
- Separate buttons for each kind of history, with the amount stored shown next to them. Right-click a file and choose **Forget this file** to remove just that one.
- From a terminal: `kinoz --wipe` does the same as Clean identity (close kinoz first).

Nothing here talks to a server. It only deletes or avoids writing files on your own computer.

## The one network request
kinoz can check for updates. This is the only thing it ever sends over the network:

- a plain `GET` to `https://api.github.com/repos/p4inz-code/kinoz/releases/latest` (or, when GitHub's API is rate limited, to `https://github.com/p4inz-code/kinoz/releases/latest`) and, if an update exists, a download from `github.com/p4inz-code/kinoz/releases`;
- with a generic `User-Agent` of `kinoz/<version>`;
- **no** user id, no file names, no usage data, no system information.

As with any request, GitHub sees your IP address and handles it under [its own privacy statement](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement). No server run by the author is involved.

It runs at most once a day. **Settings → About → Updates** chooses what happens: **Automatic** (download in the background, then ask to restart; "Later" installs it on the next launch), **Notify me** (tell me, I decide), or **Off** (never contacts the network on its own). You can always check manually. The Doctor (`--doctor`, Health check) can open a plain connection to github.com on port 443 to see whether it is reachable; it sends nothing.

Downloaded updates are only installed if their SHA-256 checksum matches the one published with the release, and only when you press the button.

## Third-party components
The playback engine (libmpv/FFmpeg) is used offline for local files. If *you* open a network address, that component connects to it, as any player would.

## Changes
If this policy changes, the change is listed in [CHANGELOG.md](CHANGELOG.md).
