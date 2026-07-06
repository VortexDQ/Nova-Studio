<#
.SYNOPSIS
  One-command setup and build for Nova Studio on Windows.

.DESCRIPTION
  Installs missing build tools (Git, CMake, MSVC, Python) via winget when possible,
  downloads prebuilt Qt binaries, uses vcpkg for FFmpeg only, then builds the app.

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

$QtVersion = "6.8.3"
$QtArch = "win64_msvc2022_64"
$QtModules = @("qtopenglwidgets")

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

function Ensure-Python {
    if (Test-Command python) {
        return
    }

    Write-Step "Installing Python..."
    Install-WingetPackage "Python.Python.3.12"

    if (-not (Test-Command python)) {
        throw "Python is still unavailable. Close this terminal, open a new one, and re-run setup.ps1."
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

function Get-QtInstallPath {
    $candidates = @(
        (Join-Path $Root ".qt\$QtVersion\msvc2022_64")
        (Join-Path $Root ".qt\Qt\$QtVersion\msvc2022_64")
    )

    foreach ($candidate in $candidates) {
        $config = Join-Path $candidate "lib\cmake\Qt6\Qt6Config.cmake"
        if (Test-Path $config) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Ensure-Qt {
    $existing = Get-QtInstallPath
    if ($existing) {
        Write-Step "Using prebuilt Qt at $existing"
        return $existing
    }

    Write-Step "Downloading prebuilt Qt $QtVersion (much faster than compiling from source)..."
    Ensure-Python

    python -m pip install --upgrade aqtinstall --quiet
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to install aqtinstall."
    }

    $qtOutput = Join-Path $Root ".qt"
    $moduleArgs = @()
    foreach ($module in $QtModules) {
        $moduleArgs += "-m"
        $moduleArgs += $module
    }

    & python -m aqt install-qt windows desktop $QtVersion $QtArch --outputdir $qtOutput @moduleArgs
    if ($LASTEXITCODE -ne 0) {
        throw "Qt download failed. Check your internet connection and re-run setup.ps1."
    }

    $installed = Get-QtInstallPath
    if (-not $installed) {
        throw "Qt install finished but Qt6Config.cmake was not found under .qt/."
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

function Get-CMakeVisualStudioGenerator {
    $help = cmake --help | Out-String

    if ($help -match "Visual Studio 18 2026") {
        return "Visual Studio 18 2026"
    }

    if ($help -match "Visual Studio 17 2022") {
        return "Visual Studio 17 2022"
    }

    throw "No supported Visual Studio CMake generator was found. Install Visual Studio 2022/2026 Build Tools with the C++ workload, then re-run setup.ps1."
}

function Remove-StaleBuildCache {
    param(
        [string]$ExpectedGenerator,
        [string]$ExpectedTriplet,
        [string]$ExpectedQtPath
    )

    $buildDir = Join-Path $Root "build"
    $cacheFile = Join-Path $buildDir "CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return
    }

    $cache = Get-Content $cacheFile -Raw
    $shouldRemove = $false

    if ($cache -match 'CMAKE_GENERATOR:INTERNAL=([^\r\n]+)') {
        $existingGenerator = $Matches[1].Trim()
        if ($existingGenerator -ne $ExpectedGenerator) {
            Write-Step "Removing stale build cache (was '$existingGenerator', need '$ExpectedGenerator')..."
            $shouldRemove = $true
        }
    }

    if ($cache -match 'VCPKG_TARGET_TRIPLET:STRING=([^\r\n]+)') {
        $existingTriplet = $Matches[1].Trim()
        if ($existingTriplet -ne $ExpectedTriplet) {
            Write-Step "Removing stale vcpkg cache (was '$existingTriplet', need '$ExpectedTriplet')..."
            $shouldRemove = $true
        }
    }

    if ($cache -match 'CMAKE_PREFIX_PATH:STRING=([^\r\n]*)') {
        $existingQtPath = $Matches[1].Trim()
        if ($existingQtPath -ne $ExpectedQtPath) {
            Write-Step "Removing stale Qt cache (was '$existingQtPath', need '$ExpectedQtPath')..."
            $shouldRemove = $true
        }
    } else {
        Write-Step "Removing stale build cache (missing prebuilt Qt path)..."
        $shouldRemove = $true
    }

    if ($shouldRemove) {
        Remove-Item -Recurse -Force $buildDir
    }
}

function Invoke-Configure {
    param([string]$QtPath)

    Write-Step "Configuring CMake..."
    Write-Host "First configure builds FFmpeg via vcpkg. Qt is already downloaded as prebuilt binaries." -ForegroundColor Yellow

    $generator = Get-CMakeVisualStudioGenerator
    $triplet = "x64-windows-release"
    $buildDir = Join-Path $Root "build"
    $toolchainFile = Join-Path $Root ".vcpkg\scripts\buildsystems\vcpkg.cmake"

    Remove-StaleBuildCache -ExpectedGenerator $generator -ExpectedTriplet $triplet -ExpectedQtPath $QtPath

    cmake -S $Root -B $buildDir `
        -G $generator `
        -A x64 `
        -DCMAKE_TOOLCHAIN_FILE="$toolchainFile" `
        -DVCPKG_TARGET_TRIPLET="$triplet" `
        -DVCPKG_HOST_TRIPLET="$triplet" `
        -DCMAKE_PREFIX_PATH="$QtPath"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }
}

function Invoke-Build {
    Write-Step "Building Nova Studio..."
    cmake --build (Join-Path $Root "build") --config Release -j
    if ($LASTEXITCODE -ne 0) {
        throw "Build failed."
    }
}

function Invoke-Tests {
    Write-Step "Running tests..."
    ctest --test-dir (Join-Path $Root "build") -C Release --output-on-failure
    if ($LASTEXITCODE -ne 0) {
        throw "Tests failed."
    }
}

Write-Host @"

 Nova Studio - Windows setup
 -----------------------------
 One script does the heavy lifting:
   1. Git, CMake, Python, and MSVC (if missing)
   2. Prebuilt Qt download + FFmpeg via vcpkg
   3. Configure, build, and optional tests

"@ -ForegroundColor Green

Ensure-Winget
Ensure-Git
Ensure-CMake
Ensure-MSVC | Out-Null
$qtPath = Ensure-Qt
Ensure-Vcpkg | Out-Null
Invoke-Configure -QtPath $qtPath

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
