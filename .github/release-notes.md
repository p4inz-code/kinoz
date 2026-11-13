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
