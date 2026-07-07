# Packaging Nova Studio as a working `.exe`

This guide has two audiences:

| Who | What to do |
|-----|------------|
| **End users** (no Python, no Visual Studio) | Download the **ZIP** from [GitHub Releases](https://github.com/VortexDQ/Nova-Studio/releases), extract, double-click `nova_studio.exe` |
| **Developers** (build from source) | Follow the steps below, then run `.\scripts\package.ps1` to create that ZIP |

End users **never** run `setup.ps1` or install Python.

---

## For end users (just run the app)

1. Download `NovaStudio-0.1.0-win64.zip` (or latest release).
2. Right-click → **Extract All** to Desktop or Downloads.
3. Open the `NovaStudio` folder.
4. Double-click **`nova_studio.exe`**.

No Python. No Visual Studio. No CMake. No account needed.

The ZIP already includes Qt, FFmpeg, project templates, and (when built on a dev machine) the **Microsoft C++ runtime DLLs** so most PCs work out of the box.

If the app still will not start, run **`Install-VC-Runtime.bat`** once inside the folder (one-time, may ask for admin). Then try `nova_studio.exe` again.

Read **`README.txt`** inside the ZIP for the same instructions.

---

## Quick path for developers (recommended)

From the project root in PowerShell:

```powershell
# 1) Build + deploy runtime DLLs (first time or after code changes)
.\scripts\setup.ps1

# 2) Create a distributable folder + ZIP
.\scripts\package.ps1
```

Output:

| Path | What it is |
|------|------------|
| `dist\NovaStudio\` | Portable app folder — copy anywhere and run `nova_studio.exe` |
| `dist\NovaStudio-0.1.0-win64.zip` | Same folder, zipped for sharing |

Launch locally without packaging:

```powershell
.\scripts\run.ps1
# or
.\build\Release\nova_studio.exe
```

---

## What “working exe” actually means

`nova_studio.exe` is **not** a single self-contained file. Nova Studio uses:

- **Qt 6** (UI, OpenGL preview, audio playback, screen/camera recording)
- **FFmpeg DLLs** (decode, export, media probe)
- **Qt plugins** (Windows platform, image formats, multimedia backends)
- **Microsoft VC++ runtime** (usually already on the PC)

The setup and package scripts copy all of that next to the executable so double-click works.

### Folder layout after packaging

```
dist/NovaStudio/
  nova_studio.exe          ← main app
  Qt6Core.dll              ← Qt runtime
  Qt6Gui.dll
  Qt6Widgets.dll
  Qt6OpenGL.dll
  Qt6OpenGLWidgets.dll
  Qt6Multimedia.dll
  Qt6Network.dll
  Qt6Svg.dll
  avcodec-62.dll           ← FFmpeg (decode/export pipeline)
  avformat-62.dll
  avutil-60.dll
  swresample-6.dll
  swscale-9.dll
  opengl32sw.dll           ← software OpenGL fallback
  dxcompiler.dll
  dxil.dll
  ffmpeg.exe               ← optional; MP3/GIF export
  platforms/
    qwindows.dll           ← required — app won't start without this
  imageformats/
    qjpeg.dll, qgif.dll, …
  multimedia/
    windowsmediaplugin.dll, ffmpegmediaplugin.dll, …
  styles/
  iconengines/
  tls/
  templates/
    blank-1080p.nova       ← project templates in sidebar
    vertical-reels.nova
    youtube-1080p.nova
```

---

## Step-by-step (manual)

### 1. Prerequisites (build machine only)

On the machine that **compiles** Nova Studio:

- Windows 10/11 x64
- Visual Studio 2022 or 2026 Build Tools (C++ workload)
- Git, CMake, Python (or run `.\scripts\setup.ps1` to install them)

You do **not** need these on PCs that only **run** the packaged ZIP.

### 2. Configure and build Release

```powershell
cmake --preset windows
cmake --build --preset windows -j
ctest --preset windows --output-on-failure
```

If you only have VS 2022:

```powershell
cmake --preset windows-vs2022
cmake --build --preset windows-vs2022 -j
```

The executable lands at:

```
build\Release\nova_studio.exe
```

### 3. Deploy Qt + FFmpeg DLLs

CMake runs `windeployqt` automatically after each build. To refresh DLLs without rebuilding:

```powershell
.\scripts\setup.ps1 -DeployOnly
```

This:

1. Runs `windeployqt` on `nova_studio.exe`
2. Copies FFmpeg DLLs from `build\vcpkg_installed\x64-windows-release\bin\`

**Smoke test** before packaging:

```powershell
.\scripts\run.ps1
```

If you see *“Could not find the Qt platform plugin windows”*, run `-DeployOnly` again.

### 4. Assemble the portable folder

Either use the packager:

```powershell
.\scripts\package.ps1
```

Or copy manually:

```powershell
$dest = "dist\NovaStudio"
New-Item -ItemType Directory -Force -Path $dest | Out-Null
Copy-Item build\Release\*.exe, build\Release\*.dll -Destination $dest
Copy-Item build\Release\platforms, build\Release\imageformats, build\Release\multimedia,
          build\Release\styles, build\Release\iconengines, build\Release\tls,
          build\Release\generic, build\Release\networkinformation -Destination $dest -Recurse
Copy-Item templates -Destination $dest\templates -Recurse
```

Remove build artifacts you don't need to ship:

```powershell
Remove-Item $dest\*.lib -ErrorAction SilentlyContinue
```

### 5. (Optional) Bundle `ffmpeg.exe` for MP3/GIF export

Video export uses built-in FFmpeg DLLs. **MP3** and **GIF** export spawn `ffmpeg.exe` as a subprocess.

The app looks for `ffmpeg.exe` in this order:

1. Next to `nova_studio.exe`
2. On your system `PATH`

To include it in the package:

```powershell
# If you have ffmpeg on PATH:
Copy-Item (Get-Command ffmpeg).Source "$dest\ffmpeg.exe"

# Or download a static build from https://www.gyan.dev/ffmpeg/builds/
# and copy ffmpeg.exe into dist\NovaStudio\
```

Without `ffmpeg.exe`, the editor still runs; only **Export MP3** and **Export GIF** will show an error.

### 6. Zip for distribution

```powershell
Compress-Archive -Path dist\NovaStudio -DestinationPath dist\NovaStudio-0.1.0-win64.zip -Force
```

Share the ZIP. Recipients extract and run `NovaStudio\nova_studio.exe`.

---

## Running on another PC

### Minimum requirements (target machine)

| Requirement | Notes |
|-------------|--------|
| Windows 10/11 **64-bit** | 32-bit Windows is not supported |
| GPU with OpenGL 3.3+ | Most machines from the last ~10 years |
| **Nothing else** | Runtime DLLs are inside the ZIP |

The package script copies **MSVC runtime DLLs** next to the exe and includes **`vc_redist.x64.exe`** plus **`Install-VC-Runtime.bat`** as a fallback. Most users never need the installer.

No admin rights required if you extract to a user folder (e.g. Desktop).

### First launch checklist

1. Extract the ZIP to a normal folder (not inside `Program Files` unless you know UAC implications).
2. Double-click `nova_studio.exe`.
3. **File → Import Media…** to load a clip.
4. If preview is black, check the file codec (H.264 in MP4 is the safest test).

### Antivirus / SmartScreen

Unsigned executables may trigger Windows SmartScreen (“Windows protected your PC”). Click **More info → Run anyway**, or code-sign the binary for production releases.

---

## Rebuild after code changes

```powershell
cmake --build build --config Release
.\scripts\setup.ps1 -DeployOnly
.\scripts\package.ps1
```

Or the all-in-one:

```powershell
.\scripts\setup.ps1
.\scripts\package.ps1
```

---

## Linux and macOS (brief)

### Linux

```bash
./scripts/setup.sh
cmake --build --preset linux -j"$(nproc)"
./build/nova_studio
```

For distribution, bundle Qt with `linuxdeployqt` (not yet scripted in this repo) or ship as a Flatpak/AppImage. See [`BUILDING.md`](BUILDING.md).

### macOS

```bash
./scripts/setup.sh
cmake --build --preset macos -j"$(sysctl -n hw.ncpu)"
open build/nova_studio.app
```

CMake sets `MACOSX_BUNDLE` on the target. Use `macdeployqt build/nova_studio.app` before zipping the `.app` bundle.

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `Could not find the Qt platform plugin "windows"` | Run `.\scripts\setup.ps1 -DeployOnly` — `platforms\qwindows.dll` is missing |
| App starts then crashes on import | Ensure FFmpeg DLLs (`avcodec-62.dll`, etc.) sit next to the `.exe` |
| MP3/GIF export fails | Copy `ffmpeg.exe` next to `nova_studio.exe` or install FFmpeg on PATH |
| Templates missing in sidebar | Ensure `templates\` folder is next to `nova_studio.exe` |
| `VCRUNTIME140.dll` missing | Run `Install-VC-Runtime.bat` in the app folder (included in ZIP) |
| Blank preview | Try a different video (H.264 MP4); check GPU/OpenGL drivers |

---

## Production checklist (before sharing widely)

- [ ] Build **Release** (not Debug)
- [ ] Run `ctest --preset windows`
- [ ] Run `.\scripts\package.ps1` (bundles MSVC DLLs + VC installer fallback)
- [ ] Test `dist\NovaStudio\nova_studio.exe` on a **clean** VM or another PC
- [ ] Upload `dist\NovaStudio-*-win64.zip` to **GitHub Releases** for end users
- [ ] Include `ffmpeg.exe` if you advertise MP3/GIF export
- [ ] Document VC++ Redistributable requirement in your release notes
- [ ] (Optional) Create an installer with [Inno Setup](https://jrsoftware.org/isinfo.php) or [WiX](https://wixtoolset.org/) pointing at `dist\NovaStudio`

---

## Related docs

- [`INSTALL-WINDOWS.md`](INSTALL-WINDOWS.md) — non-developer install from source
- [`BUILDING.md`](BUILDING.md) — manual dependency setup
- [`THIRD_PARTY_LICENSES.md`](THIRD_PARTY_LICENSES.md) — Qt (LGPL), FFmpeg (LGPL/GPL), redistribution notes
