param(
    [string]$QtDir = "",
    [string]$VcpkgRoot = "",
    [switch]$RunHardware,
    [switch]$RunLiveInput,
    [string]$OutputId = "",
    [string]$InputId = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot
$QtDir = Resolve-FV1QtDir $QtDir

Write-Host ""
Write-Host "FV-1 Lab Phase 9C Windows Completion / Release gate"
Write-Host "==================================================="

Write-Host ""
Write-Host "=== Source hygiene ==="
& git.exe -C $Root diff --check
if ($LASTEXITCODE -ne 0) { throw "git diff --check failed" }

Write-Host ""
Write-Host "=== Clean MSVC Release regression ==="
$TestArgs = @{
    Config = "Release"
    QtDir = $QtDir
    Clean = $true
}
if ($VcpkgRoot) { $TestArgs["VcpkgRoot"] = $VcpkgRoot }
$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "test-windows.ps1") @TestArgs
if (-not $?) { throw "Phase 9C child PowerShell gate failed." }

Write-Host ""
Write-Host "=== Unicode / long-path filesystem acceptance ==="
$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "windows-filesystem-acceptance.ps1") `
    -Config Release -QtDir $QtDir
if (-not $?) { throw "Phase 9C child PowerShell gate failed." }

Write-Host ""
Write-Host "=== DPI / desktop acceptance ==="
$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "windows-dpi-acceptance.ps1") `
    -Config Release -QtDir $QtDir
if (-not $?) { throw "Phase 9C child PowerShell gate failed." }

if ($RunHardware -or $RunLiveInput) {
    Write-Host ""
    Write-Host "=== WASAPI hardware acceptance ==="
    $AudioArgs = @{ Config = "Release" }
    if ($RunLiveInput) { $AudioArgs["LiveInput"] = $true }
    if ($OutputId) { $AudioArgs["OutputId"] = $OutputId }
    if ($InputId) { $AudioArgs["InputId"] = $InputId }

    $global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "windows-audio-acceptance.ps1") @AudioArgs
    if (-not $?) { throw "Phase 9C child PowerShell gate failed." }
} else {
    Write-Host ""
    Write-Host "=== WASAPI hardware acceptance ==="
    Write-Host "SKIPPED. Use -RunHardware or -RunLiveInput."
}

Write-Host ""
Write-Host "=== Release packaging ==="
$PackageArgs = @{ QtDir = $QtDir }
if ($VcpkgRoot) { $PackageArgs["VcpkgRoot"] = $VcpkgRoot }
$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "package-windows.ps1") @PackageArgs
if (-not $?) { throw "Phase 9C child PowerShell gate failed." }

Write-Host ""
Write-Host "=== Release artifact verification ==="
$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "windows-release-artifact-check.ps1")
if (-not $?) { throw "Phase 9C child PowerShell gate failed." }

Write-Host ""
Write-Host "=== PHASE 9C AUTOMATED WINDOWS RELEASE GATE PASSED ==="
Write-Host ""
Write-Host "Manual closure: docs\WINDOWS-PHASE9C-CHECKLIST.md"
