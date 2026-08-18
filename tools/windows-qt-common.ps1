Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Get-FV1RepoRoot {
    param([string]$StartingPath)

    if ([string]::IsNullOrWhiteSpace($StartingPath)) {
        $StartingPath = $PSScriptRoot
    }

    $candidate = Resolve-Path (Join-Path $StartingPath "..")
    if (Test-Path (Join-Path $candidate "CMakeLists.txt")) {
        return $candidate.Path
    }

    $candidate = Resolve-Path $StartingPath
    if (Test-Path (Join-Path $candidate "CMakeLists.txt")) {
        return $candidate.Path
    }

    throw "Unable to locate the Spin-FV-1-Emulator repository root."
}

function Assert-FV1Windows {
    # `$IsWindows` exists in PowerShell 7 but not in stock Windows PowerShell
    # 5.1. `$env:OS` is present in both, so use it as the compatibility test.
    if ($env:OS -ne "Windows_NT") {
        throw "This helper is intended for Windows."
    }
}

function Assert-FV1Command {
    param([Parameter(Mandatory=$true)][string]$Name)

    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if (-not $command) {
        throw "Required command '$Name' was not found in PATH."
    }
    return $command.Source
}

function Resolve-FV1QtDir {
    param([string]$Requested)

    $candidates = New-Object System.Collections.Generic.List[string]

    if (-not [string]::IsNullOrWhiteSpace($Requested)) {
        $candidates.Add($Requested)
    }

    if (-not [string]::IsNullOrWhiteSpace($env:QTDIR)) {
        $candidates.Add($env:QTDIR)
    }

    foreach ($tool in @("qmake6.exe", "qmake.exe", "qtpaths6.exe", "qtpaths.exe")) {
        $cmd = Get-Command $tool -ErrorAction SilentlyContinue
        if ($cmd) {
            try {
                if ($tool -like "qmake*") {
                    $prefix = (& $cmd.Source -query QT_INSTALL_PREFIX 2>$null | Select-Object -First 1)
                } else {
                    $prefix = (& $cmd.Source --query QT_INSTALL_PREFIX 2>$null | Select-Object -First 1)
                }
                if ($prefix) {
                    $candidates.Add($prefix.Trim())
                }
            } catch {
            }
        }
    }

    if (Test-Path "C:\Qt") {
        $versions = Get-ChildItem "C:\Qt" -Directory -ErrorAction SilentlyContinue |
            Sort-Object Name -Descending

        foreach ($version in $versions) {
            foreach ($kit in @("msvc2022_64", "msvc2019_64")) {
                $candidate = Join-Path $version.FullName $kit
                if (Test-Path $candidate) {
                    $candidates.Add($candidate)
                }
            }
        }
    }

    foreach ($candidate in $candidates) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }

        try {
            $full = (Resolve-Path $candidate).Path
        } catch {
            continue
        }

        if (Test-Path (Join-Path $full "lib\cmake\Qt6\Qt6Config.cmake")) {
            return $full
        }
    }

    throw @"
Qt 6 MSVC kit was not found.

Install the Qt 6 'MSVC 2022 64-bit' desktop component with the official Qt
installer, then either:
  - set QTDIR to the kit directory, or
  - pass -QtDir 'C:\Qt\<version>\msvc2022_64'

The Linux and Windows products intentionally use the same Qt 6 Widgets frontend.
"@
}

function Get-FV1QtVersion {
    param([Parameter(Mandatory=$true)][string]$QtDir)

    $qmake = Join-Path $QtDir "bin\qmake.exe"
    if (Test-Path $qmake) {
        try {
            return ((& $qmake -query QT_VERSION) | Select-Object -First 1).Trim()
        } catch {
        }
    }

    $versionFile = Join-Path $QtDir "lib\cmake\Qt6\Qt6ConfigVersion.cmake"
    if (Test-Path $versionFile) {
        $match = Select-String -Path $versionFile -Pattern 'PACKAGE_VERSION\s+"([^"]+)"' |
            Select-Object -First 1
        if ($match -and $match.Matches.Count -gt 0) {
            return $match.Matches[0].Groups[1].Value
        }
    }

    return "unknown"
}

function Resolve-FV1VsWhere {
    $path = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path $path) {
        return $path
    }
    return $null
}

function Assert-FV1VisualStudio2022 {
    $vswhere = Resolve-FV1VsWhere
    if (-not $vswhere) {
        throw "Visual Studio Installer/vswhere was not found. Install Visual Studio 2022 with Desktop development with C++."
    }

    $install = (& $vswhere -latest -products * -version "[17.0,18.0)" `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath) | Select-Object -First 1

    if (-not $install) {
        throw "Visual Studio 2022 C++ x64 tools were not found. Add the 'Desktop development with C++' workload."
    }

    return $install.Trim()
}

function Resolve-FV1VcpkgRoot {
    param([string]$Requested)

    foreach ($candidate in @(
        $Requested,
        $env:VCPKG_ROOT,
        (Join-Path $env:LOCALAPPDATA "FV1Lab\vcpkg"),
        "C:\vcpkg"
    )) {
        if ([string]::IsNullOrWhiteSpace($candidate)) {
            continue
        }
        if (Test-Path (Join-Path $candidate "vcpkg.exe")) {
            return (Resolve-Path $candidate).Path
        }
    }

    return $null
}

function Get-FV1SpeexCMakeArgs {
    param([string]$VcpkgRoot)

    if ([string]::IsNullOrWhiteSpace($VcpkgRoot)) {
        return @()
    }

    $triplet = "x64-windows-static-md"
    $prefix = Join-Path $VcpkgRoot "installed\$triplet"

    $include = Join-Path $prefix "include"
    $release = Join-Path $prefix "lib\speexdsp.lib"
    $debug = Join-Path $prefix "debug\lib\speexdsp.lib"

    if (-not (Test-Path (Join-Path $include "speex\speex_resampler.h"))) {
        return @()
    }

    $args = @("-DSPEEXDSP_INCLUDE_DIR=$include")

    if (Test-Path $release) {
        $args += "-DSPEEXDSP_LIBRARY_RELEASE=$release"
    }

    if (Test-Path $debug) {
        $args += "-DSPEEXDSP_LIBRARY_DEBUG=$debug"
    }

    return $args
}

function Find-FV1BuiltExe {
    param(
        [Parameter(Mandatory=$true)][string]$BuildDir,
        [Parameter(Mandatory=$true)][string]$Config
    )

    foreach ($candidate in @(
        (Join-Path $BuildDir "$Config\FV1Lab.exe"),
        (Join-Path $BuildDir "FV1Lab.exe")
    )) {
        if (Test-Path $candidate) {
            return (Resolve-Path $candidate).Path
        }
    }

    throw "FV1Lab.exe was not found in '$BuildDir' for configuration '$Config'."
}
