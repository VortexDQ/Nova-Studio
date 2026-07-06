# Nova Studio

Nova Studio is an open-source, cross-platform, GPU-accelerated video editor
written in modern C++23. This repository contains a **real, working
vertical slice** of the full architecture described in
[`docs/VISION.md`](docs/VISION.md) and [`docs/ROADMAP.md`](docs/ROADMAP.md):
decode a video file, place it on a timeline, play it back, and grade it with
a live GPU shader   all wired end to end, nothing mocked out.

> **Scope note.** A professional NLE that matches DaVinci Resolve or Premiere
> feature-for-feature is a multi-year, multi-person engineering effort. This
> repository is the architected foundation for that project: real modules,
> real build system, real tests, a real running app   sized so it compiles
> and runs today, with a clear roadmap for the rest.

## What works right now

- **Import** a video file (MP4/MOV/MKV/AVI/WebM   anything your FFmpeg build
  demuxes) via the Media Library panel
- **Decode** it through a real FFmpeg pipeline (`nova_media`)
- **Place** it on a real timeline data model (`nova_timeline`)   tracks,
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

## Getting started

### 1. Get the code

**Option A   clone with Git (recommended, easiest to update later):**

```bash
git clone https://github.com/VortexDQ/Nova-Studio/blob/main/README.md
cd nova-studio
```

**Option B   download a ZIP (no Git required):**

1. On the repo's GitHub page, click the green **Code** button → **Download ZIP**.
2. Extract it (`unzip nova-studio-main.zip` on Linux/macOS, or right-click →
   Extract All on Windows).
3. Open a terminal in the extracted folder   the folder name may be
   `nova-studio-main` rather than `nova-studio`; `cd` into whichever it is.

**Option C   you already have a `nova-studio.tar.gz`:**

```bash
tar -xzf nova-studio.tar.gz
cd nova-studio
```

### 2. Install dependencies

Nova Studio needs a C++23 compiler, CMake, Qt 6, and FFmpeg's dev libraries.
Full instructions for Linux, macOS, and Windows are in
[`docs/BUILDING.md`](docs/BUILDING.md)   short version for Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y cmake qt6-base-dev libgl1-mesa-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev
```

### 3. Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 4. Test (optional, but a good sanity check on a fresh machine)

```bash
ctest --test-dir build --output-on-failure
```

### 5. Run

```bash
./build/nova_studio
```

Once it's open: **File → Import Media...** to load a clip, double-click it
in the Media Library to send it to the timeline and preview, then hit
**Play**. The Inspector panel's sliders (brightness/contrast/saturation)
apply live via the GPU shader pipeline.

If you're on a headless machine or a container with no display, see the
"Running the app headless" section of [`docs/BUILDING.md`](docs/BUILDING.md).

## Architecture

See [`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md) for module boundaries and
how this slice maps onto the full modular design (core / media / timeline /
renderer / ui / playback / effects / audio / export / plugin / ...).

## License

Apache License 2.0   see [`LICENSE`](LICENSE). Qt 6 is used under LGPLv3 via
dynamic linking; see [`docs/THIRD_PARTY_LICENSES.md`](docs/THIRD_PARTY_LICENSES.md).

## Contributing

See [`docs/CONTRIBUTING.md`](docs/CONTRIBUTING.md).
