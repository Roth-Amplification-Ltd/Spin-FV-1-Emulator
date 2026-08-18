param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug",

    [string]$BuildDir = "",
    [string]$QtDir = ""
)

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root "build-windows"
}

$exe = Find-FV1BuiltExe -BuildDir $BuildDir -Config $Config
$QtDir = Resolve-FV1QtDir $QtDir
$env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"
$env:QT_PLUGIN_PATH = Join-Path $QtDir "plugins"

Write-Host "Launching same Qt 6 FV-1 Lab desktop frontend used on Linux:"
Write-Host "  $exe"
Start-Process -FilePath $exe -WorkingDirectory $Root
