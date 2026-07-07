# Install Nova Studio on Windows (non-developer guide)

**You do not need Python, Visual Studio, or any developer tools.**

## Easiest way: download the ready-made ZIP

1. Go to [GitHub Releases](https://github.com/VortexDQ/Nova-Studio/releases) for this project.
2. Download **`NovaStudio-*-win64.zip`**.
3. Right-click → **Extract All**.
4. Open the `NovaStudio` folder and double-click **`nova_studio.exe`**.

If Windows SmartScreen appears: **More info** → **Run anyway**.

If the app will not start: run **`Install-VC-Runtime.bat`** once in that folder, then try again.

---

## Build from source (developers only)

Only use this if you are modifying the code or there is no release ZIP yet.

You do **not** need Python, Qt, FFmpeg, or any dev tools installed beforehand.
One script installs everything for you.

## Steps

1. **Install Git** (optional but recommended)  
   Download from [git-scm.com](https://git-scm.com/download/win), or skip this and download a ZIP from GitHub instead.

2. **Get the project**
   ```powershell
   git clone https://github.com/VortexDQ/Nova-Studio.git
   cd Nova-Studio
   ```
   Or download the ZIP from GitHub, extract it, then open PowerShell inside the extracted folder.

3. **Run the setup script**
   ```powershell
   .\scripts\setup.ps1 -Run
   ```

4. **Wait for the first run to finish**  
   The first time can take **10–20 minutes** while it downloads Qt and builds FFmpeg.
   Later runs are much faster and skip anything already installed.

5. **Open the app later**
   ```powershell
   .\scripts\run.ps1
   ```

## If PowerShell blocks the script

Run this once, then try again:
```powershell
Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
```

## What the script installs (automatically)

| Tool | Why |
|---|---|
| Git | clone vcpkg (one time) |
| CMake | build system |
| Python | download prebuilt Qt |
| Visual Studio Build Tools | C++ compiler (only if you don't already have one) |
| Qt 6.8.3 | app UI and audio playback (downloaded, not compiled) |
| FFmpeg | video decoding (built once via vcpkg) |

Re-running `setup.ps1` skips tools and downloads that are already present.

## Troubleshooting

**"Could not find the Qt platform plugin windows"**  
Run:
```powershell
.\scripts\setup.ps1 -DeployOnly -Run
```

**Something failed partway through**  
Run again - the script picks up where it left off:
```powershell
.\scripts\setup.ps1 -Run
```

**Force a full rebuild**
```powershell
.\scripts\setup.ps1 -Force -Run
```

## Package for another PC

After a successful build, create a portable folder and ZIP:

```powershell
.\scripts\package.ps1
```

See [`docs/PACKAGING.md`](PACKAGING.md) for the full guide (manual steps, troubleshooting, VC++ redistributable).
