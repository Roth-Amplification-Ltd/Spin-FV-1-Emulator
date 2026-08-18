param(
    [string]$QtDir = "",
    [string]$VcpkgRoot = "",
    [switch]$CheckOnly,
    [switch]$SkipSpeexDSP,
    [switch]$Build,
    [switch]$Test
)

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot

Write-Host ""
Write-Host "=== FV-1 Lab Windows Qt development bootstrap ==="
Write-Host "Repository: $Root"

$git = Assert-FV1Command "git.exe"
$cmake = Assert-FV1Command "cmake.exe"
$python = Assert-FV1Command "python.exe"
$vs = Assert-FV1VisualStudio2022
$QtDir = Resolve-FV1QtDir $QtDir
$qtVersion = Get-FV1QtVersion $QtDir

Write-Host "Git:          $git"
Write-Host "CMake:        $cmake"
Write-Host "Python:       $python"
Write-Host "Visual Studio $vs"
Write-Host "Qt kit:       $QtDir"
Write-Host "Qt version:   $qtVersion"

$Miniaudio = Join-Path $Root "third_party\miniaudio\miniaudio.h"
if (-not (Test-Path $Miniaudio)) {
    if ($CheckOnly) {
        throw "Pinned miniaudio header is missing: $Miniaudio"
    }

    Write-Host ""
    Write-Host "Fetching pinned miniaudio 0.11.21 header..."
    New-Item -ItemType Directory -Force -Path (Split-Path $Miniaudio) | Out-Null
    Invoke-WebRequest `
        -Uri "https://raw.githubusercontent.com/mackron/miniaudio/0.11.21/miniaudio.h" `
        -OutFile $Miniaudio `
        -UseBasicParsing
}
Write-Host "miniaudio:    $Miniaudio"

if (-not $SkipSpeexDSP) {
    $VcpkgRoot = Resolve-FV1VcpkgRoot $VcpkgRoot

    if (-not $VcpkgRoot -and -not $CheckOnly) {
        $VcpkgRoot = Join-Path $env:LOCALAPPDATA "FV1Lab\vcpkg"
        Write-Host ""
        Write-Host "Bootstrapping vcpkg for SpeexDSP parity: $VcpkgRoot"

        New-Item -ItemType Directory -Force -Path (Split-Path $VcpkgRoot) | Out-Null
        & $git clone --depth 1 https://github.com/microsoft/vcpkg.git $VcpkgRoot
        if ($LASTEXITCODE -ne 0) {
            throw "Unable to clone vcpkg."
        }

        & (Join-Path $VcpkgRoot "bootstrap-vcpkg.bat") -disableMetrics
        if ($LASTEXITCODE -ne 0) {
            throw "Unable to bootstrap vcpkg."
        }
    }

    if ($VcpkgRoot) {
        $vcpkg = Join-Path $VcpkgRoot "vcpkg.exe"
        $speexHeader = Join-Path $VcpkgRoot "installed\x64-windows-static-md\include\speex\speex_resampler.h"

        if (-not (Test-Path $speexHeader)) {
            if ($CheckOnly) {
                Write-Warning "SpeexDSP is not installed; the deterministic linear SRC fallback would be used."
            } else {
                Write-Host ""
                Write-Host "Installing SpeexDSP via vcpkg..."
                & $vcpkg install "speexdsp:x64-windows-static-md" --disable-metrics
                if ($LASTEXITCODE -ne 0) {
                    throw "vcpkg failed to install SpeexDSP."
                }
            }
        }

        if (Test-Path $speexHeader) {
            Write-Host "SpeexDSP:     installed via $VcpkgRoot"
        }
    } elseif ($CheckOnly) {
        Write-Warning "vcpkg not found; SpeexDSP parity is not currently available."
    }
} else {
    Write-Host "SpeexDSP:     intentionally skipped; deterministic linear SRC fallback will be used"
}

Write-Host ""
Write-Host "Windows Qt environment: READY"
Write-Host "The product frontend is the same Qt 6 Widgets code used on Linux."

if ($Build) {
    $buildArgs = @{
        QtDir = $QtDir
    }
    if ($VcpkgRoot) {
        $buildArgs["VcpkgRoot"] = $VcpkgRoot
    }

    & (Join-Path $PSScriptRoot "build-windows.ps1") @buildArgs
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

if ($Test) {
    $testArgs = @{
        QtDir = $QtDir
    }
    if ($VcpkgRoot) {
        $testArgs["VcpkgRoot"] = $VcpkgRoot
    }

    & (Join-Path $PSScriptRoot "test-windows.ps1") @testArgs
    exit $LASTEXITCODE
}
