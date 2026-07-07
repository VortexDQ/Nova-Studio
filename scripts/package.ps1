<#
.SYNOPSIS
  Package Nova Studio into a portable dist folder and ZIP.

.DESCRIPTION
  Copies nova_studio.exe, Qt/FFmpeg runtime DLLs, plugins, and templates into
  dist\NovaStudio\, then creates dist\NovaStudio-<version>-win64.zip.

  Run after a successful Release build. Deploys DLLs first if needed.

.EXAMPLE
  .\scripts\package.ps1

.EXAMPLE
  .\scripts\package.ps1 -SkipDeploy
#>
param(
    [switch]$SkipDeploy,
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Resolve-Path (Join-Path $ScriptDir "..")
Set-Location $Root

$ReleaseDir = Join-Path $Root "build\Release"
$Exe = Join-Path $ReleaseDir "nova_studio.exe"
$DistName = "NovaStudio"
$DistDir = Join-Path $Root "dist\$DistName"
$ZipPath = Join-Path $Root "dist\$DistName-$Version-win64.zip"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Copy-MsvcRuntime {
    param([string]$Destination)

    $copied = $false
    $searchRoots = @()

    if ($env:VCToolsRedistDir) {
        $searchRoots += (Join-Path $env:VCToolsRedistDir "x64\Microsoft.VC143.CRT")
        $searchRoots += (Join-Path $env:VCToolsRedistDir "x64\Microsoft.VC142.CRT")
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $vswhere) {
        $vsPath = & $vswhere -latest -property installationPath 2>$null
        if ($vsPath) {
            $redistRoot = Join-Path $vsPath "VC\Redist\MSVC"
            if (Test-Path $redistRoot) {
                $latest = Get-ChildItem $redistRoot -Directory | Sort-Object Name -Descending | Select-Object -First 1
                if ($latest) {
                    $searchRoots += (Join-Path $latest.FullName "x64\Microsoft.VC143.CRT")
                    $searchRoots += (Join-Path $latest.FullName "x64\Microsoft.VC142.CRT")
                }
            }
        }
    }

    foreach ($dir in $searchRoots) {
        if (-not (Test-Path $dir)) { continue }
        Copy-Item (Join-Path $dir "*.dll") $Destination -Force
        $copied = $true
        Write-Host "  OK: MSVC runtime DLLs from $dir" -ForegroundColor DarkGray
        break
    }

    if (-not $copied) {
        Write-Host "  Note: MSVC runtime DLLs not found - build on a machine with Visual Studio" -ForegroundColor Yellow
    }
    return $copied
}

function Write-EndUserReadme {
    param([string]$Destination, [string]$AppVersion)

    $readme = @"
Nova Studio $AppVersion
=====================

For everyone (no Python, no Visual Studio, no setup):

  1. Extract this ZIP to any folder (Desktop is fine).
  2. Double-click nova_studio.exe
  3. File -> Import Media to load a video.

That is it. You do not need to install anything else.

Optional: MP3 and GIF export use ffmpeg.exe in this folder (included when available).

Problems?
  - Windows 10 or 11 (64-bit) required.
  - If Windows blocks the app, click More info -> Run anyway.
  - If the app will not start, run Install-VC-Runtime.bat once (included when packaged).

"@
    Set-Content -Path (Join-Path $Destination "README.txt") -Value $readme -Encoding UTF8
}

if (-not (Test-Path $Exe)) {
    throw "Release build not found at $Exe`nRun: .\scripts\setup.ps1"
}

if (-not $SkipDeploy) {
    Write-Step "Deploying Qt and FFmpeg runtime libraries..."
    & (Join-Path $ScriptDir "setup.ps1") -DeployOnly
}

$platformPlugin = Join-Path $ReleaseDir "platforms\qwindows.dll"
if (-not (Test-Path $platformPlugin)) {
    throw "Missing $platformPlugin - run .\scripts\setup.ps1 -DeployOnly"
}

Write-Step "Creating portable folder at dist\$DistName\"

if (Test-Path $DistDir) {
    Remove-Item $DistDir -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $DistDir | Out-Null

Copy-Item (Join-Path $ReleaseDir "*.exe") $DistDir -Force
Copy-Item (Join-Path $ReleaseDir "*.dll") $DistDir -Force

$pluginDirs = @(
    "platforms", "imageformats", "multimedia", "styles", "iconengines",
    "tls", "generic", "networkinformation"
)
foreach ($dir in $pluginDirs) {
    $src = Join-Path $ReleaseDir $dir
    if (Test-Path $src) {
        Copy-Item $src (Join-Path $DistDir $dir) -Recurse -Force
    }
}

Remove-Item (Join-Path $DistDir "*.lib") -ErrorAction SilentlyContinue

Copy-MsvcRuntime -Destination $DistDir | Out-Null

$vcRedistUrl = "https://aka.ms/vs/17/release/vc_redist.x64.exe"
$vcRedistExe = Join-Path $DistDir "vc_redist.x64.exe"
$vcBat = Join-Path $DistDir "Install-VC-Runtime.bat"
try {
    Write-Host "  Downloading VC++ redistributable installer (fallback)..." -ForegroundColor DarkGray
    Invoke-WebRequest -Uri $vcRedistUrl -OutFile $vcRedistExe -UseBasicParsing
    @"
@echo off
echo Installing Microsoft Visual C++ runtime (one-time, may need admin)...
"%~dp0vc_redist.x64.exe" /install /quiet /norestart
echo Done. Try nova_studio.exe again.
pause
"@ | Set-Content -Path $vcBat -Encoding ASCII
    Write-Host "  OK: vc_redist.x64.exe + Install-VC-Runtime.bat" -ForegroundColor DarkGray
} catch {
    Write-Host "  Note: could not download VC++ installer (offline build?)" -ForegroundColor Yellow
}

$templatesSrc = Join-Path $Root "templates"
if (Test-Path $templatesSrc) {
    Copy-Item $templatesSrc (Join-Path $DistDir "templates") -Recurse -Force
    Write-Host "  OK: templates\" -ForegroundColor DarkGray
}

$ffmpegCandidates = @(
    (Join-Path $ReleaseDir "ffmpeg.exe"),
    (Join-Path $Root "build\vcpkg_installed\x64-windows-release\tools\ffmpeg\ffmpeg.exe")
)
$ffmpegOnPath = $null
try {
    $ffmpegOnPath = (Get-Command ffmpeg -ErrorAction Stop).Source
} catch {}

if ($ffmpegOnPath) {
    $ffmpegCandidates += $ffmpegOnPath
}

foreach ($candidate in $ffmpegCandidates) {
    if ($candidate -and (Test-Path $candidate)) {
        Copy-Item $candidate (Join-Path $DistDir "ffmpeg.exe") -Force
        Write-Host "  OK: ffmpeg.exe (MP3/GIF export)" -ForegroundColor DarkGray
        break
    }
}

if (-not (Test-Path (Join-Path $DistDir "ffmpeg.exe"))) {
    Write-Host "  Note: ffmpeg.exe not bundled - MP3/GIF export needs ffmpeg on PATH" -ForegroundColor Yellow
}

Write-Step "Creating ZIP archive"
Write-EndUserReadme -Destination $DistDir -AppVersion $Version
if (Test-Path $ZipPath) {
    Remove-Item $ZipPath -Force
}
New-Item -ItemType Directory -Force -Path (Join-Path $Root "dist") | Out-Null
Compress-Archive -Path $DistDir -DestinationPath $ZipPath -Force

Write-Host ""
Write-Host "Package ready!" -ForegroundColor Green
Write-Host "  Folder: $DistDir"
Write-Host "  ZIP:    $ZipPath"
Write-Host ""
Write-Host "Run on this PC:" -ForegroundColor Green
Write-Host "  $DistDir\nova_studio.exe"
Write-Host ""
Write-Host "Copy the ZIP to any Windows 10/11 PC - no Python or dev tools required." -ForegroundColor DarkGray
Write-Host "MSVC runtime DLLs are bundled; Install-VC-Runtime.bat is a one-click fallback." -ForegroundColor DarkGray
Write-Host ""
