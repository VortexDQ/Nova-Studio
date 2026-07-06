# Building Nova Studio

Verified on **Ubuntu 24.04 (noble), GCC 13.3, Qt 6.4, FFmpeg 6.1** as part of
this repository's own development. Windows/macOS instructions below are
written to the same package names/flags but have not yet been build-verified
in CI (see `docs/ROADMAP.md` Milestone 8) — please file an issue with any
corrections.

## One-command setup (easiest)

| Platform | Command |
|---|---|
| **Windows** | `.\scripts\setup.ps1` |
| **Linux (apt)** | `./scripts/setup.sh` |
| **macOS (Homebrew)** | `./scripts/setup.sh` |

What the scripts do:

- **Windows:** uses `winget` to install Git, CMake, Python, and MSVC Build Tools when
  missing; downloads prebuilt Qt into `.qt/`; installs FFmpeg from the repo's
  `vcpkg.json` manifest with the Release-only `x64-windows-release` triplet;
  configures with the best available Visual Studio generator; builds and runs
  tests.
- **Linux/macOS:** installs system packages (apt or Homebrew), then configures
  with the `linux` or `macos` preset, builds, and runs tests.

Useful flags:

```powershell
# Windows
.\scripts\setup.ps1 -Run          # build, deploy DLLs, and launch
.\scripts\run.ps1                 # launch only (after first setup)
.\scripts\setup.ps1 -DeployOnly   # copy Qt/FFmpeg DLLs next to the exe
.\scripts\setup.ps1 -SkipTests    # skip ctest
.\scripts\setup.ps1 -Force          # force reconfigure/redeploy
```

```bash
# Linux / macOS
./scripts/setup.sh --run
./scripts/setup.sh --skip-tests
./scripts/setup.sh --skip-build
```

**First Windows build note:** the setup script downloads prebuilt Qt binaries
instead of compiling Qt from source. FFmpeg still builds once via vcpkg on a
fresh machine. Expect **10–20 minutes** once; incremental rebuilds are much
faster afterward.

## Manual build (if you manage deps yourself)

### Dependencies

| Dependency | Debian/Ubuntu package | Notes |
|---|---|---|
| CMake ≥ 3.24 | `cmake` | |
| A C++23 compiler | `g++` / `clang` / MSVC 2022 17.8+ | |
| Qt 6 (Widgets, OpenGLWidgets) | `qt6-base-dev` | |
| FFmpeg dev libs | `libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev` | |
| OpenGL dev headers | `libgl1-mesa-dev` | |
| pkg-config | `pkg-config` | used to locate FFmpeg |

### Linux (Debian/Ubuntu)

Prefer `./scripts/setup.sh`. Manual equivalent:

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake pkg-config qt6-base-dev libgl1-mesa-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev

cmake --preset linux
cmake --build --preset linux -j"$(nproc)"
ctest --preset linux
./build/nova_studio
```

### macOS

Prefer `./scripts/setup.sh`. Manual equivalent:

```bash
brew install cmake qt ffmpeg pkg-config
cmake --preset macos
cmake --build --preset macos -j"$(sysctl -n hw.ncpu)"
ctest --preset macos
open build/nova_studio.app
```

### Windows (prebuilt Qt + vcpkg FFmpeg)

Prefer `.\scripts\setup.ps1`. Manual equivalent:

```powershell
git clone --depth 1 https://github.com/microsoft/vcpkg.git .vcpkg
.\.vcpkg\bootstrap-vcpkg.bat -disableMetrics
python -m pip install aqtinstall
python -m aqt install-qt windows desktop 6.8.3 win64_msvc2022_64 -m qtopenglwidgets --outputdir .qt
cmake --preset windows
cmake --build --preset windows -j
ctest --preset windows
.\build\Release\nova_studio.exe
```

Dependencies are declared in the repo-root `vcpkg.json` (FFmpeg only); Qt is
downloaded as prebuilt binaries into `.qt/`. The default `windows` preset
targets Visual Studio 2026; use `windows-vs2022` instead if your machine only
has Visual Studio 2022.

## Build options

| CMake option | Default | Effect |
|---|---|---|
| `NOVA_BUILD_TESTS` | `ON` | Builds `tests/` and registers them with ctest |
| `NOVA_ENABLE_SANITIZERS` | `OFF` | Adds `-fsanitize=address,undefined` (GCC/Clang only) |

## Running the app headless (CI / sandboxes without a display)

`QOpenGLWidget` needs a real GL context, so the `offscreen` Qt platform
plugin is **not sufficient**. Use Xvfb with software rendering instead:

```bash
Xvfb :99 -screen 0 1280x800x24 &
DISPLAY=:99 LIBGL_ALWAYS_SOFTWARE=1 ./build/nova_studio
```

## Troubleshooting

- `Could NOT find PkgConfig` → install `pkg-config` (Linux) or `pkgconf`
  (macOS/Homebrew).
- `Found libavcodec ... too old` — pin to FFmpeg ≥ 5.0; earlier versions
  are missing APIs `nova_media` depends on.
- Blank/black preview after Import — check the app's stdout log; `Decoder`
  logs the specific FFmpeg error (unsupported codec, corrupt file, etc.) via
  `nova_core`'s logger rather than failing silently.
