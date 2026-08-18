param(
    [string]$QtDir = "",
    [string]$VcpkgRoot = "",
    [string]$BuildDir = "",
    [string]$DistDir = ""
)

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root "build-windows"
}
if ([string]::IsNullOrWhiteSpace($DistDir)) {
    $DistDir = Join-Path $Root "dist\windows"
}

$QtDir = Resolve-FV1QtDir $QtDir
$qtVersion = Get-FV1QtVersion $QtDir
$VcpkgRoot = Resolve-FV1VcpkgRoot $VcpkgRoot

$env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"
$env:QT_PLUGIN_PATH = Join-Path $QtDir "plugins"

$buildArgs = @{
    Config = "Release"
    QtDir = $QtDir
    BuildDir = $BuildDir
}
if ($VcpkgRoot) {
    $buildArgs["VcpkgRoot"] = $VcpkgRoot
}

& (Join-Path $PSScriptRoot "build-windows.ps1") @buildArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$release = Find-FV1BuiltExe -BuildDir $BuildDir -Config "Release"

$version = "1.0.0-rc1"
try {
    $versionOut = & (Join-Path $BuildDir "Release\fv1-cli.exe") --version 2>$null
    if ($versionOut -match '([0-9]+\.[0-9]+\.[0-9]+(?:-[A-Za-z0-9._-]+)?)') {
        $version = $Matches[1]
    }
} catch {
}

$stage = Join-Path $DistDir "FV1Lab-$version"
$zip = Join-Path $DistDir "FV1Lab-$version-windows-x64.zip"

if (Test-Path $stage) {
    Remove-Item -Recurse -Force $stage
}
if (Test-Path $zip) {
    Remove-Item -Force $zip
}
New-Item -ItemType Directory -Force -Path $stage | Out-Null

Write-Host ""
Write-Host "=== Install product tree ==="
& cmake.exe --install $BuildDir --config Release --prefix $stage
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$installedExe = Join-Path $stage "bin\FV1Lab.exe"
if (-not (Test-Path $installedExe)) {
    throw "Installed Qt product executable missing: $installedExe"
}

$windeployqt = Join-Path $QtDir "bin\windeployqt.exe"
if (-not (Test-Path $windeployqt)) {
    throw "Qt deployment tool not found: $windeployqt"
}

Write-Host ""
Write-Host "=== Deploy Qt runtime/plugins ==="
& $windeployqt `
    --release `
    --compiler-runtime `
    --no-translations `
    --dir (Join-Path $stage "bin") `
    $installedExe
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Copy-Item (Join-Path $Root "LICENSE") (Join-Path $stage "LICENSE.txt") -Force
Copy-Item (Join-Path $Root "README.md") (Join-Path $stage "README.md") -Force

$commit = (& git.exe -C $Root rev-parse HEAD).Trim()
$manifest = @"
FV-1 Lab Windows portable package
Version: $version
Commit: $commit
Qt: $qtVersion
Architecture: x86_64
Compiler: MSVC 2022
Frontend: shared Qt 6 Widgets (same source as Linux)
Audio: miniaudio / WASAPI
"@
Set-Content -Path (Join-Path $stage "BUILD-INFO.txt") -Value $manifest -Encoding UTF8

Write-Host ""
Write-Host "=== Smoke deployed package ==="
& $installedExe --smoke
if ($LASTEXITCODE -ne 0) {
    throw "Deployed FV1Lab.exe smoke test failed."
}
& $installedExe --smoke-splash
if ($LASTEXITCODE -ne 0) {
    throw "Deployed splash smoke test failed."
}

Write-Host ""
Write-Host "=== Portable ZIP ==="
Compress-Archive -Path (Join-Path $stage "*") -DestinationPath $zip -CompressionLevel Optimal

$hash = Get-FileHash -Algorithm SHA256 $zip
Set-Content -Path "$zip.sha256" -Value "$($hash.Hash.ToLower())  $([IO.Path]::GetFileName($zip))"

Write-Host ""
Write-Host "WINDOWS PORTABLE PACKAGE COMPLETE"
Write-Host "Stage: $stage"
Write-Host "ZIP:   $zip"
Write-Host "SHA:   $zip.sha256"
