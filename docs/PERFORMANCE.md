# Performance

Goal: it should feel instant and never lag, even when opened and closed rapidly, with fast clicking, or in several windows at once.

## Measured (one Windows machine, 1080p H.264, hardware decode on)
| Metric | Result |
| --- | --- |
| Launch → window visible | ~0.44 s |
| CPU while playing | ~3% of one core |
| Memory per window | ~280 MB working set |
| Self-test | 57 checks pass (open/close, seek, volume, 30 rapid next-clicks, panels, library, thumbnails, annotate, update check…) |

Not yet measured: macOS/Linux, 4K, very large libraries (the model is built for 10,000+ files but not benchmarked), frame pacing with an external tool.

## How it stays responsive
- Window first, engine second: libmpv initialises on a worker thread.
- All mpv commands are asynchronous; state is cached. Seeks collapse to the latest one in flight; stale load events are ignored.
- Folder scans, thumbnails (LIFO, cancellable, disk-cached, low priority) and the seek preview (one lazily created decoder) run off the UI thread.
- Closing hides the window at once and tears the engine down in the background.
- A minimized video keeps its sound but stops decoding the picture.
- Repaints are event-driven: nothing redraws while idle.

## Run the checks yourself
`kinoz --selftest <a video>` (use `KINOZ_SILENT=1 KINOZ_OFFSCREEN=1` to keep it quiet and off-screen) → `data/logs/selftest.txt`.

## Skimming benchmark (`--bench`)

`KINOZ_SILENT=1 KINOZ_OFFSCREEN=1 kinoz --bench <a video>` copies the video into 8 clips, then clicks through them, spams Next 150 times, drags the seek bar 300 times and prints PASS/FAIL against budgets to `data/logs/bench.txt`. A 4 ms heartbeat on the UI thread records the longest freeze.

| Check | Budget | Measured (Windows, 1080p clip) |
|---|---|---|
| First frame after launch | 3000 ms | ~600 ms |
| Click to picture, average / slowest | 700 / 1500 ms | ~275 / ~285 ms |
| Longest UI freeze, clicking through | 100 ms | 5-10 ms |
| Skip spam (150 clicks): longest UI freeze | 150 ms | 5-25 ms |
| Skip spam: files really opened | 12 | 2 |
| Seek-bar drag: longest UI freeze | 150 ms | 5-10 ms |
| Memory after all of it | 700 MB | ~255 MB |
