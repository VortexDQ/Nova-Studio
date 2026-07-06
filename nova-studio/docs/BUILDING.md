# Building Nova Studio

Verified on **Ubuntu 24.04 (noble), GCC 13.3, Qt 6.4, FFmpeg 6.1** as part of
this repository's own development. Windows/macOS instructions below are
written to the same package names/flags but have not yet been build-verified
in CI (see `docs/ROADMAP.md` Milestone 8) — please file an issue with any
corrections.

## Dependencies

| Dependency | Debian/Ubuntu package | Notes |
|---|---|---|
| CMake ≥ 3.24 | `cmake` | |
| A C++23 compiler | `g++` / `clang` / MSVC 2022 17.8+ | |
| Qt 6 (Widgets, OpenGLWidgets) | `qt6-base-dev` | |
| FFmpeg dev libs | `libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev` | |
| OpenGL dev headers | `libgl1-mesa-dev` | |
| pkg-config | `pkg-config` | used to locate FFmpeg |

### Linux (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y cmake qt6-base-dev libgl1-mesa-dev \
    libavcodec-dev libavformat-dev libavutil-dev libswscale-dev libswresample-dev

cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
ctest --test-dir build --output-on-failure
./build/nova_studio
```

### macOS

```bash
brew install cmake qt ffmpeg pkg-config
cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build -j"$(sysctl -n hw.ncpu)"
ctest --test-dir build --output-on-failure
open build/nova_studio.app
```

### Windows (MSVC + vcpkg)

```powershell
vcpkg install qtbase[opengl] ffmpeg
cmake -B build -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake"
cmake --build build --config Release -j
ctest --test-dir build -C Release --output-on-failure
.\build\Release\nova_studio.exe
```

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
