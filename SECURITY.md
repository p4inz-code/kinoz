# Security

## Reporting a vulnerability
Please report security problems **privately** through GitHub: **Security → Report a vulnerability** on <https://github.com/p4inz-code/kinoz/security/advisories/new>.
Don't open a public issue for something that could be exploited. You'll get an answer as soon as I can; this is a solo project.

## How updates are protected
- The updater only talks to `api.github.com` and `github.com/p4inz-code/kinoz/releases`.
- It installs a download only if its **SHA-256** equals the checksum published with the release, and only after you press the button.
- If a release has no checksum, kinoz refuses to run it automatically and just opens the release page.

## Verify a download yourself
Every release lists the SHA-256 of each file. On Windows:
```
certutil -hashfile kinoz-Setup-<version>.exe SHA256
```

## Honest limits
- Builds are **not code-signed** yet, so Windows SmartScreen (and macOS Gatekeeper) will warn on first run. The checksum is how you confirm the file is the one that was published.
- A checksum protects against a corrupted or swapped download. It cannot protect against a compromised GitHub account; that's why signing is on the [roadmap](ROADMAP.md).
- kinoz opens media files with FFmpeg/libmpv. Like every media player, it can be affected by flaws in those libraries; I update the bundled build when they release fixes.
