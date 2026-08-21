param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug",

    [string]$QtDir = "",

    [string]$BuildDir = ""
)

$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root "build-windows"
}

$QtDir = Resolve-FV1QtDir $QtDir
$Exe = Find-FV1BuiltExe -BuildDir $BuildDir -Config $Config

$OldPath = $env:PATH
$OldPluginPath = $env:QT_PLUGIN_PATH
$OldScaleFactor = $env:QT_SCALE_FACTOR
$OldQpa = $env:QT_QPA_PLATFORM

try {
    $env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"
    $env:QT_PLUGIN_PATH = Join-Path $QtDir "plugins"
    $env:QT_QPA_PLATFORM = "windows"

    Write-Host ""
    Write-Host "FV-1 Lab Phase 9B.4 Windows DPI acceptance"
    Write-Host "=========================================="
    Write-Host "Executable: $Exe"

    foreach ($Scale in @("1.0", "1.25", "1.5", "2.0")) {
        Write-Host ""
        Write-Host "=== Simulated Qt scale factor $Scale ==="
        $env:QT_SCALE_FACTOR = $Scale

        $Smoke = Start-Process `
            -FilePath $Exe `
            -ArgumentList @("--smoke-desktop") `
            -Wait `
            -PassThru

        if ($Smoke.ExitCode -ne 0) {
            throw "FV1Lab.exe --smoke-desktop failed at scale $Scale with exit code $($Smoke.ExitCode)"
        }

        Write-Host "desktop smoke exit: $($Smoke.ExitCode)"
    }

    $Manifest = Get-Content (Join-Path $Root "packaging\windows\fv1-lab-qt.manifest") -Raw
    foreach ($Needle in @("PerMonitorV2", "longPathAware", "true/pm")) {
        if (-not $Manifest.Contains($Needle)) {
            throw "Windows manifest is missing required token: $Needle"
        }
    }

    $Version = (Get-Item $Exe).VersionInfo
    if ($Version.ProductName -ne "FV-1 Lab") {
        throw "Unexpected Windows ProductName: $($Version.ProductName)"
    }

    Write-Host ""
    Write-Host "PHASE 9B.4 AUTOMATED DPI ACCEPTANCE PASSED"
    Write-Host ""
    Write-Host "Automated coverage:"
    Write-Host "  - Qt fractional scale policy: 100 / 125 / 150 / 200 percent"
    Write-Host "  - main-window visibility smoke"
    Write-Host "  - adaptive splash geometry"
    Write-Host "  - Windows PerMonitorV2 manifest contract"
    Write-Host "  - Windows long-path/DPI manifest contract"
    Write-Host "  - Windows ProductName identity"
    Write-Host ""
    Write-Host "Manual mixed-monitor/UI checks remain in:"
    Write-Host "  docs\WINDOWS-PHASE9B4-CHECKLIST.md"
}
finally {
    $env:PATH = $OldPath
    $env:QT_PLUGIN_PATH = $OldPluginPath
    $env:QT_SCALE_FACTOR = $OldScaleFactor
    $env:QT_QPA_PLATFORM = $OldQpa
}
