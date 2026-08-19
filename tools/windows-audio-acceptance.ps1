param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [string]$BuildDir = "",

    [string]$Program = "",

    [double]$Seconds = 3.0,

    [int]$OutputDevice = -1,
    [int]$InputDevice = -1,

    [string]$OutputId = "",
    [string]$InputId = "",

    [switch]$LiveInput
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root "build-windows"
}

if ([string]::IsNullOrWhiteSpace($Program)) {
    $Program = Join-Path $Root "examples\simple_passthrough.spn"
}

$Exe = Join-Path $BuildDir "$Config\fv1-live.exe"
if (-not (Test-Path $Exe)) {
    throw "fv1-live.exe not found: $Exe. Run tools\test-windows.ps1 first."
}

$Program = (Resolve-Path $Program).Path

Write-Host ""
Write-Host "FV-1 Lab Phase 9B.2 WASAPI hardware acceptance"
Write-Host "================================================"
Write-Host "Executable: $Exe"
Write-Host "Program:    $Program"
Write-Host ""

Write-Host "=== WASAPI endpoint inventory ==="
& $Exe devices
if ($LASTEXITCODE -ne 0) {
    throw "fv1-live devices failed with exit code $LASTEXITCODE"
}

function Add-EndpointArgs {
    param([System.Collections.Generic.List[string]]$CommandArgs)

    if (-not [string]::IsNullOrWhiteSpace($OutputId)) {
        $CommandArgs.Add("--output-id")
        $CommandArgs.Add($OutputId)
    } elseif ($OutputDevice -ge 0) {
        $CommandArgs.Add("--output-device")
        $CommandArgs.Add([string]$OutputDevice)
    }

    if (-not [string]::IsNullOrWhiteSpace($InputId)) {
        $CommandArgs.Add("--input-id")
        $CommandArgs.Add($InputId)
    } elseif ($InputDevice -ge 0) {
        $CommandArgs.Add("--input-device")
        $CommandArgs.Add([string]$InputDevice)
    }
}

$Matrix = @(
    @{ Rate = 48000; Buffer = 256; Name = "48 kHz / 256" },
    @{ Rate = 44100; Buffer = 256; Name = "44.1 kHz / 256" },
    @{ Rate = 48000; Buffer = 128; Name = "48 kHz / 128" },
    @{ Rate = 48000; Buffer = 512; Name = "48 kHz / 512" }
)

Write-Host ""
Write-Host "=== Playback/Test Generator matrix ==="
Write-Host "Each run emits a 440 Hz test tone through the selected playback endpoint."
Write-Host ""

foreach ($Case in $Matrix) {
    Write-Host "--- $($Case.Name) ---"

    $Args = [System.Collections.Generic.List[string]]::new()
    $Args.Add("run")
    $Args.Add($Program)
    $Args.Add("--sine")
    $Args.Add("440")
    $Args.Add("--seconds")
    $Args.Add([string]$Seconds)
    $Args.Add("--meter")
    $Args.Add("--host-rate")
    $Args.Add([string]$Case.Rate)
    $Args.Add("--buffer")
    $Args.Add([string]$Case.Buffer)

    Add-EndpointArgs $Args

    & $Exe @Args
    if ($LASTEXITCODE -ne 0) {
        throw "WASAPI matrix case '$($Case.Name)' failed with exit code $LASTEXITCODE"
    }
}

if ($LiveInput) {
    Write-Host ""
    Write-Host "=== Live input -> FV-1 -> playback ==="
    Write-Host "WARNING: avoid acoustic feedback when using speakers and a microphone."

    $Args = [System.Collections.Generic.List[string]]::new()
    $Args.Add("run")
    $Args.Add($Program)
    $Args.Add("--live")
    $Args.Add("--seconds")
    $Args.Add([string]$Seconds)
    $Args.Add("--meter")
    $Args.Add("--host-rate")
    $Args.Add("48000")
    $Args.Add("--buffer")
    $Args.Add("256")

    Add-EndpointArgs $Args

    & $Exe @Args
    if ($LASTEXITCODE -ne 0) {
        throw "Live WASAPI capture/playback acceptance failed with exit code $LASTEXITCODE"
    }
}

Write-Host ""
Write-Host "PHASE 9B.2 WASAPI HARDWARE SCRIPT PASSED"
Write-Host ""
Write-Host "Now manually verify in FV-1 Lab:"
Write-Host "  - endpoint selection persists across Refresh"
Write-Host "  - unplug/disable an active endpoint is reported cleanly"
Write-Host "  - default-device reroute is logged"
Write-Host "  - no sustained analyzer/recorder drops under sane settings"
