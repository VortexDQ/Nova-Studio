<#
.SYNOPSIS
  One-command setup and build for Nova Studio on Windows.

.DESCRIPTION
  Installs missing build tools via winget, downloads prebuilt Qt, builds FFmpeg
  with vcpkg, compiles Nova Studio, deploys runtime DLLs, and optionally runs the app.

  Safe to re-run: already-installed tools and downloads are skipped automatically.

.EXAMPLE
  .\scripts\setup.ps1

.EXAMPLE
  .\scripts\setup.ps1 -Run

.EXAMPLE
  .\scripts\setup.ps1 -DeployOnly -Run
#>
param(
    [switch]$SkipBuild,
    [switch]$Run,
    [switch]$SkipTests,
    [switch]$Force,
    [switch]$DeployOnly
)

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$Root = Resolve-Path (Join-Path $ScriptDir "..")
Set-Location $Root

$QtVersion = "6.8.3"
$QtArch = "win64_msvc2022_64"
$QtModules = @("qtopenglwidgets", "qtmultimedia")
$VcpkgTriplet = "x64-windows-release"

function Write-Step {
    param([string]$Message)
    Write-Host ""
    Write-Host "==> $Message" -ForegroundColor Cyan
}

function Write-Ok {
    param([string]$Message)
    Write-Host "  OK: $Message" -ForegroundColor DarkGray
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

function Get-AppExe {
    return Join-Path $Root "build\Release\nova_studio.exe"
}

function Get-ReleaseDir {
    return Join-Path $Root "build\Release"
}

function Ensure-Winget {
    if (-not (Test-Command winget)) {
        throw "winget is required but was not found. Open the Microsoft Store, install 'App Installer', then re-run this script."
    }
    Write-Ok "winget is available"
}

function Install-WingetPackage {
    param(
        [string]$Id,
        [string[]]$ExtraArgs = @()
    )

    $listArgs = @("list", "--id", $Id, "-e")
    & winget @listArgs 2>$null | Out-Null
    if ($LASTEXITCODE -eq 0) {
        Write-Ok "$Id already installed"
        return
    }

    $wingetArgs = @(
        "install", "--id", $Id, "-e",
        "--accept-source-agreements", "--accept-package-agreements"
    ) + $ExtraArgs

    & winget @wingetArgs
    # 0 = installed, -1978335189 = already up to date / no upgrade available
    if ($LASTEXITCODE -ne 0 -and $LASTEXITCODE -ne -1978335189) {
        throw "winget install $Id failed (exit code $LASTEXITCODE)."
    }

    Refresh-Path
}

function Ensure-Git {
    if (Test-Command git) {
        Write-Ok "Git found"
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
        Write-Ok "CMake found"
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
        Write-Ok "Python found"
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
        Write-Ok "MSVC found at $existing"
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
        $multimediaConfig = Join-Path $candidate "lib\cmake\Qt6Multimedia\Qt6MultimediaConfig.cmake"
        if ((Test-Path $config) -and (Test-Path $multimediaConfig)) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Ensure-Qt {
    $existing = Get-QtInstallPath
    if ($existing) {
        Write-Ok "Qt $QtVersion already downloaded"
        return $existing
    }

    Write-Step "Downloading prebuilt Qt $QtVersion (one-time download)..."
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
    } else {
        Write-Ok "vcpkg checkout present"
    }

    $vcpkgExe = Join-Path $vcpkgDir "vcpkg.exe"
    if (-not (Test-Path $vcpkgExe)) {
        Write-Step "Bootstrapping vcpkg..."
        $bootstrap = Join-Path $vcpkgDir "bootstrap-vcpkg.bat"
        & $bootstrap -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            throw "vcpkg bootstrap failed."
        }
    } else {
        Write-Ok "vcpkg bootstrapped"
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
            Write-Step "Removing stale build cache (generator changed)..."
            $shouldRemove = $true
        }
    }

    if ($cache -match 'VCPKG_TARGET_TRIPLET:STRING=([^\r\n]+)') {
        $existingTriplet = $Matches[1].Trim()
        if ($existingTriplet -ne $ExpectedTriplet) {
            Write-Step "Removing stale build cache (vcpkg triplet changed)..."
            $shouldRemove = $true
        }
    }

    if ($cache -match 'CMAKE_PREFIX_PATH:STRING=([^\r\n]*)') {
        $existingQtPath = $Matches[1].Trim()
        if ($existingQtPath -ne $ExpectedQtPath) {
            Write-Step "Removing stale build cache (Qt path changed)..."
            $shouldRemove = $true
        }
    } else {
        Write-Step "Removing stale build cache (missing Qt path)..."
        $shouldRemove = $true
    }

    if ($shouldRemove) {
        Remove-Item -Recurse -Force $buildDir
    }
}

function Test-ConfigureUpToDate {
    param(
        [string]$ExpectedGenerator,
        [string]$ExpectedTriplet,
        [string]$ExpectedQtPath
    )

    if ($Force) {
        return $false
    }

    $cacheFile = Join-Path $Root "build\CMakeCache.txt"
    if (-not (Test-Path $cacheFile)) {
        return $false
    }

    $cache = Get-Content $cacheFile -Raw
    return ($cache -match "CMAKE_GENERATOR:INTERNAL=$([regex]::Escape($ExpectedGenerator))") `
        -and ($cache -match "VCPKG_TARGET_TRIPLET:STRING=$([regex]::Escape($ExpectedTriplet))") `
        -and ($cache -match "CMAKE_PREFIX_PATH:STRING=$([regex]::Escape($ExpectedQtPath))")
}

function Invoke-Configure {
    param([string]$QtPath)

    $generator = Get-CMakeVisualStudioGenerator
    $buildDir = Join-Path $Root "build"
    $toolchainFile = Join-Path $Root ".vcpkg\scripts\buildsystems\vcpkg.cmake"

    Remove-StaleBuildCache -ExpectedGenerator $generator -ExpectedTriplet $VcpkgTriplet -ExpectedQtPath $QtPath

    if (Test-ConfigureUpToDate -ExpectedGenerator $generator -ExpectedTriplet $VcpkgTriplet -ExpectedQtPath $QtPath) {
        Write-Ok "CMake already configured"
        return
    }

    Write-Step "Configuring CMake..."
    Write-Host "First configure builds FFmpeg via vcpkg. Qt is already downloaded as prebuilt binaries." -ForegroundColor Yellow

    cmake -S $Root -B $buildDir `
        -G $generator `
        -A x64 `
        -DCMAKE_TOOLCHAIN_FILE="$toolchainFile" `
        -DVCPKG_TARGET_TRIPLET="$VcpkgTriplet" `
        -DVCPKG_HOST_TRIPLET="$VcpkgTriplet" `
        -DCMAKE_PREFIX_PATH="$QtPath"
    if ($LASTEXITCODE -ne 0) {
        throw "CMake configure failed."
    }
}

function Invoke-Build {
    if ($Force) {
        Write-Step "Building Nova Studio (forced rebuild)..."
    } else {
        Write-Step "Building Nova Studio..."
    }

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

function Get-QtPluginsSource {
    $candidates = @(
        (Join-Path $Root "build\vcpkg_installed\$VcpkgTriplet\Qt6\plugins")
        (Join-Path $Root ".qt\$QtVersion\msvc2022_64\plugins")
        (Join-Path $Root ".qt\Qt\$QtVersion\msvc2022_64\plugins")
    )

    foreach ($candidate in $candidates) {
        if (Test-Path (Join-Path $candidate "platforms\qwindows.dll")) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Copy-QtPluginsFallback {
    param([string]$ReleaseDir)

    $pluginSource = Get-QtPluginsSource
    if (-not $pluginSource) {
        return $false
    }

    foreach ($pluginDir in Get-ChildItem $pluginSource -Directory) {
        $dest = Join-Path $ReleaseDir $pluginDir.Name
        New-Item -ItemType Directory -Force -Path $dest | Out-Null
        Copy-Item (Join-Path $pluginDir.FullName "*") $dest -Force
    }

    Write-Ok "Copied Qt plugins from $pluginSource"
    return $true
}

function Invoke-Deploy {
    param([string]$QtPath)

    $exe = Get-AppExe
    $releaseDir = Get-ReleaseDir

    if (-not (Test-Path $exe)) {
        throw "Build output not found: $exe`nRun .\scripts\setup.ps1 without -DeployOnly first."
    }

    $platformPlugin = Join-Path $releaseDir "platforms\qwindows.dll"
    $exeTime = (Get-Item $exe).LastWriteTimeUtc
    $needsDeploy = $Force -or -not (Test-Path $platformPlugin)

    if (-not $needsDeploy -and (Test-Path $platformPlugin)) {
        $pluginTime = (Get-Item $platformPlugin).LastWriteTimeUtc
        if ($exeTime -gt $pluginTime) {
            $needsDeploy = $true
        }
    }

    if (-not $needsDeploy) {
        Write-Ok "Runtime libraries already deployed"
        return
    }

    Write-Step "Deploying Qt and FFmpeg runtime libraries..."

    $deployed = $false
    if ($QtPath) {
        $windeployqt = Join-Path $QtPath "bin\windeployqt.exe"
        if (Test-Path $windeployqt) {
            & $windeployqt --release --no-translations --no-system-d3d-compiler $exe
            if ($LASTEXITCODE -eq 0) {
                $deployed = $true
                Write-Ok "windeployqt completed"
            }
        }
    }

    if (-not (Test-Path $platformPlugin)) {
        $deployed = Copy-QtPluginsFallback -ReleaseDir $releaseDir
    }

    if (-not (Test-Path $platformPlugin)) {
        throw "Failed to deploy Qt platform plugins. Run: git pull, then .\scripts\setup.ps1 -DeployOnly"
    }

    $vcpkgBin = Join-Path $Root "build\vcpkg_installed\$VcpkgTriplet\bin"
    if (Test-Path $vcpkgBin) {
        Copy-Item (Join-Path $vcpkgBin "*.dll") $releaseDir -Force
        Write-Ok "Copied FFmpeg runtime DLLs"
    }
}

function Invoke-Run {
    $exe = Get-AppExe
    if (-not (Test-Path $exe)) {
        throw "Cannot run app; executable not found at $exe"
    }

    & $exe
}

Write-Host @"

 Nova Studio - Windows setup
 -----------------------------
 First run installs tools and downloads dependencies automatically.
 Re-runs skip work that is already done.

"@ -ForegroundColor Green

if ($DeployOnly) {
    $qtPath = Get-QtInstallPath
    Invoke-Deploy -QtPath $qtPath

    if ($Run) {
        Invoke-Run
    } else {
        Write-Host ""
        Write-Host "Done! Run Nova Studio with:" -ForegroundColor Green
        Write-Host "  .\build\Release\nova_studio.exe"
        Write-Host "  .\scripts\run.ps1"
        Write-Host ""
    }
    return
}

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

    Invoke-Deploy -QtPath $qtPath

    Write-Host ""
    Write-Host "Done! Run Nova Studio with:" -ForegroundColor Green
    Write-Host "  .\build\Release\nova_studio.exe"
    Write-Host "  .\scripts\run.ps1"
    Write-Host ""

    if ($Run) {
        Invoke-Run
    }
}
