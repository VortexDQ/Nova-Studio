<#
.SYNOPSIS
  Launch Nova Studio after setup has completed.
#>
$ErrorActionPreference = "Stop"

$Root = Resolve-Path (Join-Path $PSScriptRoot "..")
$Exe = Join-Path $Root "build\Release\nova_studio.exe"
$PlatformPlugin = Join-Path $Root "build\Release\platforms\qwindows.dll"

if (-not (Test-Path $Exe)) {
    throw "Nova Studio is not built yet. Run .\scripts\setup.ps1 first."
}

if (-not (Test-Path $PlatformPlugin)) {
    Write-Host "Runtime libraries missing; deploying Qt/FFmpeg DLLs..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "setup.ps1") -DeployOnly
}

& $Exe
