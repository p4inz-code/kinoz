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

## The one network request
kinoz can check for updates. This is the only thing it ever sends over the network:

- a plain `GET` to `https://api.github.com/repos/p4inz-code/kinoz/releases/latest` (and, if an update exists, a download from `github.com/p4inz-code/kinoz/releases`);
- with a generic `User-Agent` of `kinoz/<version>`;
- **no** user id, no file names, no usage data, no system information.

As with any request, GitHub sees your IP address and handles it under [its own privacy statement](https://docs.github.com/en/site-policy/privacy-policies/github-general-privacy-statement). No server run by the author is involved.

It runs at most once a day and can be switched off: **Settings → About → "Check for updates automatically"**. You can also check manually, or never.

Downloaded updates are only installed if their SHA-256 checksum matches the one published with the release, and only when you press the button.

## Third-party components
The playback engine (libmpv/FFmpeg) is used offline for local files. If *you* open a network address, that component connects to it, as any player would.

## Changes
If this policy changes, the change is listed in [CHANGELOG.md](CHANGELOG.md).
