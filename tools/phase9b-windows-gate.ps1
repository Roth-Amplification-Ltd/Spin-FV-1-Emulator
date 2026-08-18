param(
    [string]$QtDir = "",
    [string]$VcpkgRoot = "",
    [switch]$Clean
)

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot

Write-Host ""
Write-Host "FV-1 Lab Phase 9B Windows workflow/platform gate"
Write-Host "================================================"

$BootstrapArgs = @{
    QtDir = $QtDir
    CheckOnly = $true
    SkipSpeexDSP = $true
}
if ($VcpkgRoot) {
    $BootstrapArgs["VcpkgRoot"] = $VcpkgRoot
}

& (Join-Path $PSScriptRoot "bootstrap-windows.ps1") @BootstrapArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$TestArgs = @{
    QtDir = $QtDir
}
if ($VcpkgRoot) {
    $TestArgs["VcpkgRoot"] = $VcpkgRoot
}
if ($Clean) {
    $TestArgs["Clean"] = $true
}

& (Join-Path $PSScriptRoot "test-windows.ps1") @TestArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$PackageArgs = @{
    QtDir = $QtDir
}
if ($VcpkgRoot) {
    $PackageArgs["VcpkgRoot"] = $VcpkgRoot
}

& (Join-Path $PSScriptRoot "package-windows.ps1") @PackageArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "=== PHASE 9B AUTOMATED WINDOWS GATE PASSED ==="
Write-Host ""
Write-Host "Automated coverage now includes:"
Write-Host "  - complete Windows CTest suite"
Write-Host "  - Qt smoke / splash / About / HiDPI"
Write-Host "  - command-line FV-1 program opening"
Write-Host "  - Release build and windeployqt"
Write-Host "  - portable ZIP extraction"
Write-Host "  - portable execution with Qt developer environment removed"
Write-Host ""
Write-Host "Next: complete docs\WINDOWS-PHASE9B-CHECKLIST.md on real WASAPI hardware."
