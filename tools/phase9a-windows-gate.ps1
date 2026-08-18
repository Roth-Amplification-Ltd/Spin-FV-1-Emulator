param(
    [string]$QtDir = "",
    [string]$VcpkgRoot = "",
    [switch]$Clean
)

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot

Write-Host ""
Write-Host "FV-1 Lab Phase 9A Windows Qt mega-gate"
Write-Host "======================================"

$bootstrapArgs = @{
    QtDir = $QtDir
    CheckOnly = $true
}
if ($VcpkgRoot) {
    $bootstrapArgs["VcpkgRoot"] = $VcpkgRoot
}
& (Join-Path $PSScriptRoot "bootstrap-windows.ps1") @bootstrapArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$testArgs = @{
    QtDir = $QtDir
}
if ($VcpkgRoot) {
    $testArgs["VcpkgRoot"] = $VcpkgRoot
}
if ($Clean) {
    $testArgs["Clean"] = $true
}

& (Join-Path $PSScriptRoot "test-windows.ps1") @testArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

& (Join-Path $PSScriptRoot "package-windows.ps1") `
    -QtDir $QtDir `
    -VcpkgRoot $VcpkgRoot
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

Write-Host ""
Write-Host "=== PHASE 9A WINDOWS QT MEGA-GATE PASSED ==="
Write-Host "Next: launch the packaged Qt app and test real WASAPI hardware interactively."
