# Contributing

Contributions are welcome — kinoz is free software and stays that way.

## Before you start
- Small fixes: just open a pull request.
- Bigger changes: open an issue first so we agree on the direction.
- By contributing you agree that your work is released under the project's license (GPLv3 + [additional terms](ADDITIONAL_TERMS.md)). Your authorship is kept in the git history.

## Build
- **Windows:** Visual Studio (C++ workload), then `scripts\build.ps1` (uses the project-local Qt 6.8 and libmpv in `.toolchain/`).
- **macOS:** `brew install qt mpv pkg-config cmake`, then `cmake -B build -DCMAKE_PREFIX_PATH=$(brew --prefix qt) && cmake --build build`.
- **Linux:** `sudo apt install qt6-base-dev libgl-dev libmpv-dev pkg-config cmake`, then `cmake -B build && cmake --build build`.

## Test
`kinoz --selftest <a video file>` runs the built-in checks (set `KINOZ_SILENT=1` and `KINOZ_OFFSCREEN=1` so it neither makes sound nor opens on your screen). It prints PASS/FAIL per step to `data/logs/selftest.txt`.

## Code style
C++20, Qt idioms, no `using namespace`, small functions, comments explain *why*. Keep the UI fully custom (no native OS widgets) and keep the UI thread free of blocking work.
