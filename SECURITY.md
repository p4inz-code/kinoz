# Security

## Reporting a vulnerability
Please report security problems **privately** through GitHub: **Security → Report a vulnerability** on <https://github.com/p4inz-code/kinoz/security/advisories/new>.
Don't open a public issue for something that could be exploited. You'll get an answer as soon as I can; this is a solo project.

## How updates are protected
- The updater only talks to `api.github.com` and `github.com/p4inz-code/kinoz/releases`.
- It installs a download only if its **SHA-256** equals the checksum published with the release, and checks the checksum again right before installing. The file must sit in its own staging folder; a changed file is deleted, never run.
- In **Automatic** mode (default) it downloads in the background and asks "Restart now / Later"; with Later it installs at the next start, gives up after two attempts, and a failed update reopens the previous working version. **Notify me** and **Off** change who decides; **Off** never contacts the network on its own.
- If a release has no checksum, kinoz refuses to run it automatically and just opens the release page.

## Verify a download yourself
Every release lists the SHA-256 of each file. On Windows:
```
certutil -hashfile kinoz-Setup-<version>.exe SHA256
```

## Other protections
- kinoz runs entirely as your user: no administrator rights, no drivers, no services, no kernel code, nothing that starts at boot.
- Playlists accept local files only. Network paths (`\server\share`) and URLs are refused, because merely checking one makes Windows sign in to that server.
- Deleting a file always asks first and goes to the Recycle Bin or Trash, never permanent.
- The setup removes only files it installed and refuses a folder that is not a kinoz install. Setup files are checked against a SHA-256 list before anything is copied, and a failed copy is rolled back.
- Crash-loop protection: after two abnormal exits kinoz starts in safe mode (software decoding, no glass effect).
- The Windows build is linked with DEP, ASLR (high entropy) and Control Flow Guard, and only loads libraries from its own folder and System32.

## Honest limits
- kinoz opens media files with FFmpeg/libmpv. Like every media player, it can be affected by flaws in those libraries; I update the bundled build when they release fixes.
