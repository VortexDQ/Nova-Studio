<#
.SYNOPSIS
  One-command setup and build for Nova Studio on Windows.

.DESCRIPTION
  Installs missing build tools (Git, CMake, MSVC) via winget when possible,
  bootstraps a local vcpkg checkout, pulls Qt + FFmpeg from vcpkg.json,
  then configures and builds the app.

  First run can take 30-60 minutes while vcpkg compiles Qt.

.EXAMPLE
  .\scripts\setup.ps1

.EXAMPLE
  .\scripts\setup.ps1 -Run
#>
param(
    [switch]$SkipBuild,
    [switch]$Run,
    [switch]$SkipTests
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Resolve-Path (Join-Path $ScriptDir "..")
Set-Location $Root

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Refresh-Path {
    $machine = [System.Environment]::GetEnvironmentVariable("Path", "Machine")
    $user = [System.Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = "$machine;$user"
}

function Test-Command {
    param([string]$Name)
    return $null -ne (Get-Command $Name -ErrorAction SilentlyContinue)
}

function Ensure-Winget {
    if (-not (Test-Command winget)) {
        throw "winget is required but was not found. Install 'App Installer' from the Microsoft Store, then re-run this script."
    }
}

function Install-WingetPackage {
    param(
        [string]$Id,
        [string[]]$ExtraArgs = @()
    )

    $wingetArgs = @(
        "install", "--id", $Id, "-e",
        "--accept-source-agreements", "--accept-package-agreements"
    ) + $ExtraArgs

    & winget @wingetArgs
    if ($LASTEXITCODE -ne 0) {
        throw "winget install $Id failed (exit code $LASTEXITCODE)."
    }

    Refresh-Path
}

function Ensure-Git {
    if (Test-Command git) {
        return
    }

    Write-Step "Installing Git..."
    Install-WingetPackage "Git.Git"

    if (-not (Test-Command git)) {
        throw "Git is still unavailable. Close this terminal, open a new one, and re-run setup.ps1."
    }
}

function Ensure-CMake {
    if (Test-Command cmake) {
        return
    }

    Write-Step "Installing CMake..."
    Install-WingetPackage "Kitware.CMake"

    if (-not (Test-Command cmake)) {
        throw "CMake is still unavailable. Close this terminal, open a new one, and re-run setup.ps1."
    }
}

function Get-VsInstallPath {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) {
        return $null
    }

    return & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null
}

function Ensure-MSVC {
    $existing = Get-VsInstallPath
    if ($existing) {
        return $existing
    }

    Write-Step "Installing Visual Studio 2022 Build Tools (C++ workload)..."
    Write-Host "This is a one-time install and can take several minutes." -ForegroundColor Yellow

    Install-WingetPackage "Microsoft.VisualStudio.2022.BuildTools" @(
        "--override", "--wait --passive --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    )

    $installed = Get-VsInstallPath
    if (-not $installed) {
        throw @"
MSVC was not detected after installation.
Open 'x64 Native Tools Command Prompt for VS 2022' or restart your terminal, then re-run:
  .\scripts\setup.ps1
"@
    }

    return $installed
}

function Ensure-Vcpkg {
    $vcpkgDir = Join-Path $Root ".vcpkg"

    if (-not (Test-Path $vcpkgDir)) {
        Write-Step "Cloning vcpkg into .vcpkg (one-time setup)..."
        git clone --depth 1 https://github.com/microsoft/vcpkg.git $vcpkgDir
    }

    $vcpkgExe = Join-Path $vcpkgDir "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        Write-Step "Bootstrapping vcpkg..."
        $bootstrap = Join-Path $vcpkgDir "bootstrap-vcpkg.bat"
        & $bootstrap -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg bootstrap failed."
        }
    }

    return $vcpkgDir
}

function Invoke-Configure {
    Write-Step "Configuring CMake..."
    Write-Host "First configure downloads and builds Qt + FFmpeg via vcpkg." -ForegroundColor Yellow
    Write-Host "That can take 30-60 minutes on a fresh machine - only happens once." -ForegroundColor Yellow

    cmake --preset windows
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }
}

function Invoke-Build {
    Write-Step "Building Nova Studio..."
    cmake --build --preset windows -j
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed."
    }
}

function Invoke-Tests {
    Write-Step "Running tests..."
    ctest --preset windows
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed."
    }
}

Write-Host @"

 Nova Studio - Windows setup
 -----------------------------
 One script does the heavy lifting:
   1. Git, CMake, and MSVC (if missing)
   2. Local vcpkg + Qt/FFmpeg from vcpkg.json
   3. Configure, build, and optional tests

"@ -ForegroundColor Green

Ensure-Winget
Ensure-Git
Ensure-CMake
Ensure-MSVC | Out-Null
Ensure-Vcpkg | Out-Null
Invoke-Configure

if (-not $SkipBuild) {
    Invoke-Build

    if (-not $SkipTests) {
        Invoke-Tests
    }

    $exe = Join-Path $Root "build\Release\nova_studio.exe"
    Write-Host ""
    Write-Host "Done! Run Nova Studio with:" -ForegroundColor Green
    Write-Host "  .\build\Release\nova_studio.exe"
    Write-Host ""

    if ($Run -and (Test-Path $exe)) {
        & $exe
    }
}
