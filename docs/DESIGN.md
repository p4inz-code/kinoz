# Design

Calm, minimal, content first. The media is the interface; chrome appears when you move the mouse and fades after about 2.6 seconds.

## Principles
1. Content first. Controls fade out while a video plays.
2. One idea per surface. Everything secondary lives behind the sidebar, a side panel, or `Ctrl+K`.
3. Instant feedback. Every click responds within a frame; work happens after the visual response.
4. Interruptible motion. Every animation can be reversed mid-flight, so fast clicking never queues up lag.
5. Quiet. No sounds or popups; errors are one-line toasts.

## Tokens
Warm paper neutrals with a teal accent, 8 px radius. Light and dark follow the system or a manual choice.

| Token | Light | Dark |
| --- | --- | --- |
| background | `#fdfcf9` | `#14140f` |
| surface | `#f3f1ea` | `#1c1c16` |
| raised | `#ffffff` | `#25251e` |
| border | `#dbd7cb` | `#35352c` |
| text | `#1b1a15` | `#edebe2` |
| muted text | `#57544a` | `#aba99b` |
| accent | `#0c6a6e` | `#4fd1d6` |

Fonts: **Atkinson Hyperlegible Next** (interface) and **Familjen Grotesk** (headings), both SIL OFL, bundled in `resources/fonts`.
Motion: 120–260 ms, ease-out; turned off by "Reduce motion".

## Stages
One window, four kinds of media: **video** (letterboxed, auto-hiding controls), **audio** (cover art or a clean card with title/artist), **image** (fit, wheel zoom, drag pan, rotate, arrow keys through the folder), **GIF** (loops, with pause, frame step and speed).

## Layout
- **Sidebar:** Library (folders, thumbnails, list/grid, type filter, search, sort) and Queue.
- **Control bar:** floating glass bar: seek bar with frame preview, transport, volume, speed, tracks, adjust/equalizer, mini player, fullscreen.
- **Side panel:** Adjust picture and Equalizer slide in from the right.
- **`Ctrl+K`:** palette for files and commands. **Settings:** General, Shortcuts, About, Legal.
- **Annotate:** a floating tool pill over a paused frame.

## Shortcuts
Windows Media Player-style plus modern keys, in `resources/keymaps/default.json`; change any of them in Settings → Shortcuts (stored as overrides in `keymap.json` inside the data folder). The full list is in Settings → Shortcuts.