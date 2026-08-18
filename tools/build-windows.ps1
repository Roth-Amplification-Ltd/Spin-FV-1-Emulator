param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug",

    [string]$QtDir = "",
    [string]$VcpkgRoot = "",
    [string]$BuildDir = "",
    [switch]$Clean
)

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot
$null = Assert-FV1Command "cmake.exe"
$null = Assert-FV1VisualStudio2022

$QtDir = Resolve-FV1QtDir $QtDir
$qtVersion = Get-FV1QtVersion $QtDir

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root "build-windows"
}

if ($Clean -and (Test-Path $BuildDir)) {
    Remove-Item -Recurse -Force $BuildDir
}

$Miniaudio = Join-Path $Root "third_party\miniaudio\miniaudio.h"
if (-not (Test-Path $Miniaudio)) {
    throw "Pinned miniaudio header missing. Run .\tools\bootstrap-windows.ps1 first."
}

$VcpkgRoot = Resolve-FV1VcpkgRoot $VcpkgRoot
$speexArgs = Get-FV1SpeexCMakeArgs $VcpkgRoot

$cmakeArgs = @(
    "-S", $Root,
    "-B", $BuildDir,
    "-G", "Visual Studio 17 2022",
    "-A", "x64",
    "-DCMAKE_PREFIX_PATH=$QtDir",
    "-DFV1_BUILD_GUI=ON",
    "-DFV1_BUILD_WINDOWS_FRONTEND=OFF",
    "-DFV1_ENABLE_LIVE_AUDIO=ON",
    "-DFV1_BUILD_TESTS=ON",
    "-DFV1_SDK_BUILD_SHARED=ON"
) + $speexArgs

Write-Host ""
Write-Host "=== Configure FV-1 Lab for Windows ==="
Write-Host "Qt:     $QtDir ($qtVersion)"
Write-Host "Build:  $BuildDir"
Write-Host "Config: $Config"
if ($speexArgs.Count -gt 0) {
    Write-Host "SRC:    SpeexDSP"
} else {
    Write-Host "SRC:    deterministic linear fallback"
}

& cmake.exe @cmakeArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "=== Build FV-1 Lab / tests ==="
& cmake.exe --build $BuildDir --config $Config --parallel
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$exe = Find-FV1BuiltExe -BuildDir $BuildDir -Config $Config

Write-Host ""
Write-Host "Windows Qt build complete."
Write-Host "Executable: $exe"
Write-Host ""
Write-Host "Launch:"
Write-Host "  & '$exe'"
