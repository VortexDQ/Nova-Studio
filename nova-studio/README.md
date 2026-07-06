# Nova Studio

Nova Studio is an open-source, cross-platform, GPU-accelerated video editor
written in modern C++23. This repository contains a **real, working
vertical slice** of the full architecture described in
[`docs/VISION.md`](docs/VISION.md) and [`docs/ROADMAP.md`](docs/ROADMAP.md):
decode a video file, place it on a timeline, play it back, and grade it with
a live GPU shader — all wired end to end, nothing mocked out.

> **Scope note.** A professional NLE that matches DaVinci Resolve or Premiere
> feature-for-feature is a multi-year, multi-person engineering effort. This
> repository is the architected foundation for that project: real modules,
> real build system, real tests, a real running app — sized so it compiles
> and runs today, with a clear roadmap for the rest.

## What works right now

- **Import** a video file (MP4/MOV/MKV/AVI/WebM — anything your FFmpeg build
  demuxes) via the Media Library panel
- **Decode** it through a real FFmpeg pipeline (`nova_media`)
- **Place** it on a real timeline data model (`nova_timeline`) — tracks,
  clips, ripple-trim, frame-accurate positioning
- **Render** it through a real OpenGL 3.3 core-profile pipeline
  (`nova_renderer`) with a live GLSL brightness/contrast/saturation shader
  driven by Inspector sliders
- **Play / pause / seek / loop** with a real playback loop, and click-to-seek
  on the timeline widget
- Dockable panels (Media Library, Timeline, Inspector) around a central
  preview, built on Qt 6 Widgets

## What's next

See [`docs/ROADMAP.md`](docs/ROADMAP.md) for the prioritized path from this
slice to the full vision: multi-clip editing interactions (drag/blade/roll),
audio pipeline, effect stacking, plugin SDK, proxy/background rendering,
export queue, and the mobile/AI-module groundwork.

## Building

See [`docs/BUILDING.md`](docs/BUILDING.md). Short version, on a machine with
Qt 6 and FFmpeg dev packages installed:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
./build/nova_studio
```

## Architecture

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for module boundaries and
how this slice maps onto the full modular design (core / media / timeline /
renderer / ui / playback / effects / audio / export / plugin / ...).

## License

Apache License 2.0 — see [`LICENSE`](LICENSE). Qt 6 is used under LGPLv3 via
dynamic linking; see [`docs/THIRD_PARTY_LICENSES.md`](docs/THIRD_PARTY_LICENSES.md).

## Contributing

See [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md).
