# Roadmap

This maps the full Nova Studio vision onto milestones, starting from the
vertical slice in this repository. Rough sizing is in engineer-weeks for a
single experienced C++/graphics engineer; parallelize across a team to
compress wall-clock time.

## Milestone 1 — Timeline interaction layer (~3-4 weeks)
- Drag-to-move clips, blade/split, ripple/roll/slip/slide trim via mouse
- Multi-clip selection, grouping, snapping, magnetic timeline
- Undo/redo (command pattern over `Timeline` mutations)
- Markers, color labels, track lock/solo/mute wired into playback

## Milestone 2 — Audio pipeline (~4-6 weeks)
- `nova_audio` module: decode via FFmpeg audio streams, resample (libswresample)
- Real-time mixer (per-clip gain/pan, master bus)
- Waveform generation and display in the timeline lanes
- Sync playback clock across video + audio (this is the trickiest part —
  budget extra time for A/V sync correctness under variable-speed playback)

## Milestone 3 — Effect graph (~6-8 weeks)
- Generalize the single shader pass into a node graph: multiple stacked,
  reorderable, keyframeable effects rendered via ping-ponged FBOs
- Port the existing brightness/contrast/saturation shader into a node
- Add: blur, sharpen, LUT application, transform/crop/mask, chroma key
- Effect parameter keyframing (linear/bezier/ease interpolation) feeding
  into the Inspector's existing slider wiring

## Milestone 4 — Proxy & background rendering (~3-4 weeks)
- Background proxy transcode (lower-res H.264/ProRes proxy) on import
- Frame cache (RAM + disk tiers) in a dedicated `playback` module
- Background/incremental render queue with pause/resume/cancel/priority

## Milestone 5 — Plugin SDK (~4-6 weeks)
- Stable C ABI for dynamically loaded effect/transition/exporter plugins
  (dlopen/LoadLibrary), versioned so plugins don't need app recompiles
- Example plugin in `/plugins` demonstrating an effect + a UI panel

## Milestone 6 — Export pipeline (~4-5 weeks)
- Hardware-accelerated encode (NVENC/QuickSync/VideoToolbox) via FFmpeg
- Platform presets (YouTube/TikTok/Instagram/cinema/lossless)
- Batch queue with pause/resume/cancel, background export while editing

## Milestone 7 — Color management (~5-7 weeks)
- Scopes (waveform, vector scope, RGB parade, histogram) as real-time
  GPU-computed overlays
- LUT import/application, color wheels, curves
- ACES-ready working color space; Rec.709/Rec.2020 output transforms

## Milestone 8 — Cross-platform ports (~6-10 weeks)
- Windows/macOS CI build legs (this repo currently validates Linux only)
- Mobile (Android/iOS): the `nova_core`/`nova_timeline`/`nova_media` modules
  have no Qt dependency and are the right layer to port first; mobile UI is
  a separate, largely-new `nova_ui_mobile` target rather than a port of the
  Qt desktop shell

## Explicitly future / out of scope for the offline-first core
- Cloud sync, networking, and AI modules (speech-to-text, scene detection,
  auto subtitles, upscaling) are designed as optional plugins per the
  Plugin SDK (Milestone 5), never a hard dependency of the core app.

## Immediate next PRs (good first contributions)
1. Multi-clip drag-and-drop onto the timeline (extends `TimelineWidget`)
2. Waveform rendering for audio clips once `nova_audio` lands
3. A second GLSL effect (e.g. Gaussian blur) to validate the node-graph
   design before committing to its full API
4. GitHub Actions legs for Windows (MSVC) and macOS (Clang) builds
