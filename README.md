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

### Quick install (recommended)

**Windows** — open PowerShell in the project folder and run:

```powershell
.\scripts\setup.ps1
```

The script installs Git, CMake, Python, and MSVC if they're missing, downloads
prebuilt Qt binaries, uses vcpkg for FFmpeg only, then builds and tests the
app. First run usually takes **10–20 minutes** (mostly Qt download + FFmpeg
build); later builds are much faster.

Add `-Run` to launch the app when the build finishes:

```powershell
.\scripts\setup.ps1 -Run
```

**Linux / macOS:**

```bash
chmod +x scripts/setup.sh   # once, if needed
./scripts/setup.sh
```

On Ubuntu/Debian this installs apt packages for you. On macOS it uses
Homebrew. Add `--run` to open the app after a successful build.

### Manual install

If you prefer to install dependencies yourself, or you're on a distro the
script doesn't cover yet, see [`docs/BUILDING.md`](docs/BUILDING.md).

Short version once deps are installed:

```bash
cmake --preset linux    # or macos / windows
cmake --build --preset linux -j
ctest --preset linux --output-on-failure
./build/nova_studio
```

### Get the code

**Option A — clone with Git (recommended):**

```bash
git clone https://github.com/VortexDQ/Nova-Studio.git
cd Nova-Studio
```

**Option B — download a ZIP (no Git required):**

1. On the repo's GitHub page, click the green **Code** button → **Download ZIP**.
2. Extract it (right-click → Extract All on Windows, or `unzip` on Linux/macOS).
3. Open a terminal in the extracted folder and run the setup script above.

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
