param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Release",

    [string]$BuildDir = "",

    [int]$Cycles = 100,

    [double]$CycleSeconds = 0.20,

    [double]$ProgramSeconds = 1.0,

    [double]$SoakSeconds = 1800.0,

    [int]$HostRate = 48000,

    [int]$Buffer = 256,

    [string]$OutputId = "",

    [string]$InputId = "",

    [switch]$RunLiveInput,

    [switch]$Quick
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($env:OS -ne "Windows_NT") {
    throw "This Phase 9C RC torture gate must run on Windows."
}

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root "build-windows"
}

if ($Quick) {
    $Cycles = 10
    $CycleSeconds = 0.10
    $ProgramSeconds = 0.25
    $SoakSeconds = 10.0
}

if ($Cycles -lt 1) {
    throw "Cycles must be at least 1."
}
if ($CycleSeconds -le 0.0) {
    throw "CycleSeconds must be greater than zero."
}
if ($ProgramSeconds -le 0.0) {
    throw "ProgramSeconds must be greater than zero."
}
if ($SoakSeconds -le 0.0) {
    throw "SoakSeconds must be greater than zero."
}

$Exe = Join-Path $BuildDir "$Config\fv1-live.exe"
if (-not (Test-Path $Exe)) {
    throw "fv1-live.exe not found: $Exe. Run the Phase 9C release gate first."
}

$Programs = Get-ChildItem (Join-Path $Root "examples") `
    -Recurse `
    -File `
    -Filter "*.spn" |
    Sort-Object FullName

if (-not $Programs) {
    throw "No bundled SpinASM programs found under examples."
}

$ReportDir = Join-Path $Root "build-phase9c-windows"
New-Item -ItemType Directory -Force -Path $ReportDir | Out-Null

$Stamp = Get-Date -Format "yyyyMMdd-HHmmss"
$Report = Join-Path $ReportDir "phase9c-rc-torture-$Stamp.txt"

function Log {
    param([string]$Text = "")
    $Text | Tee-Object -FilePath $Report -Append | Write-Host
}

function Add-EndpointArgs {
    param([System.Collections.Generic.List[string]]$CommandArgs)

    if (-not [string]::IsNullOrWhiteSpace($OutputId)) {
        $CommandArgs.Add("--output-id")
        $CommandArgs.Add($OutputId)
    }

    if (-not [string]::IsNullOrWhiteSpace($InputId)) {
        $CommandArgs.Add("--input-id")
        $CommandArgs.Add($InputId)
    }
}

function Invoke-FV1Live {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Program,

        [Parameter(Mandatory=$true)]
        [double]$Seconds,

        [Parameter(Mandatory=$true)]
        [string]$Label,

        [switch]$Live
    )

    $CommandArgs = [System.Collections.Generic.List[string]]::new()
    $CommandArgs.Add("run")
    $CommandArgs.Add($Program)

    if ($Live) {
        $CommandArgs.Add("--live")
    } else {
        $CommandArgs.Add("--sine")
        $CommandArgs.Add("440")
    }

    $CommandArgs.Add("--seconds")
    $CommandArgs.Add([string]$Seconds)
    $CommandArgs.Add("--meter")
    $CommandArgs.Add("--host-rate")
    $CommandArgs.Add([string]$HostRate)
    $CommandArgs.Add("--buffer")
    $CommandArgs.Add([string]$Buffer)

    Add-EndpointArgs $CommandArgs

    Log ""
    Log "=== $Label ==="
    Log ("$Exe " + ($CommandArgs -join " "))

    $Lines = [System.Collections.Generic.List[string]]::new()

    & $Exe @CommandArgs 2>&1 | ForEach-Object {
        $Line = [string]$_
        $Lines.Add($Line)
        Log $Line
    }
    $ExitCode = $LASTEXITCODE

    if ($ExitCode -ne 0) {
        throw "$Label failed with exit code $ExitCode"
    }

    return ,$Lines.ToArray()
}

function Assert-CleanTelemetry {
    param(
        [Parameter(Mandatory=$true)]
        [object[]]$Lines,

        [Parameter(Mandatory=$true)]
        [string]$Label
    )

    $Text = ($Lines | ForEach-Object { [string]$_ }) -join "`n"

    foreach ($Pattern in @(
        "output underruns:\s+0 frames",
        "analyzer drops:\s+0 frames",
        "device-lost=no"
    )) {
        if ($Text -notmatch $Pattern) {
            throw "$Label did not report clean telemetry for pattern: $Pattern"
        }
    }
}

$Head = (& git.exe -C $Root rev-parse HEAD).Trim()
$Branch = (& git.exe -C $Root branch --show-current).Trim()

Log "FV-1 Lab Phase 9C.1 Windows RC torture"
Log "======================================="
Log "Date:          $((Get-Date).ToString('o'))"
Log "Commit:        $Head"
Log "Branch:        $Branch"
Log "Config:        $Config"
Log "Programs:      $($Programs.Count)"
Log "Cycles:        $Cycles"
Log "Cycle seconds: $CycleSeconds"
Log "Program sec:   $ProgramSeconds"
Log "Soak seconds:  $SoakSeconds"
Log "Host rate:     $HostRate"
Log "Buffer:        $Buffer"
Log "Live input:    $RunLiveInput"
Log ""

Log "=== Device inventory ==="
$DeviceLines = @(& $Exe devices 2>&1)
$DeviceExit = $LASTEXITCODE
foreach ($Line in $DeviceLines) {
    Log ([string]$Line)
}
if ($DeviceExit -ne 0) {
    throw "fv1-live devices failed with exit code $DeviceExit"
}

Log ""
Log "=== Bundled-program realtime sweep ==="
foreach ($Program in $Programs) {
    $Lines = Invoke-FV1Live `
        -Program $Program.FullName `
        -Seconds $ProgramSeconds `
        -Label ("Program sweep: " + $Program.Name)

    Assert-CleanTelemetry `
        -Lines $Lines `
        -Label ("Program sweep: " + $Program.Name)
}

Log ""
Log "=== Repeated host/runtime lifecycle torture ==="
for ($Cycle = 1; $Cycle -le $Cycles; ++$Cycle) {
    $Program = $Programs[($Cycle - 1) % $Programs.Count]

    $Lines = Invoke-FV1Live `
        -Program $Program.FullName `
        -Seconds $CycleSeconds `
        -Label ("Lifecycle cycle {0}/{1}: {2}" -f $Cycle, $Cycles, $Program.Name)

    Assert-CleanTelemetry `
        -Lines $Lines `
        -Label ("Lifecycle cycle {0}/{1}" -f $Cycle, $Cycles)
}

$Representative = $Programs |
    Where-Object { $_.Name -eq "02_ghost_spring.spn" } |
    Select-Object -First 1

if (-not $Representative) {
    $Representative = $Programs | Select-Object -First 1
}

$SoakLines = Invoke-FV1Live `
    -Program $Representative.FullName `
    -Seconds $SoakSeconds `
    -Label ("Long realtime soak: " + $Representative.Name)

Assert-CleanTelemetry `
    -Lines $SoakLines `
    -Label "Long realtime soak"

if ($RunLiveInput) {
    Log ""
    Log "=== Live capture -> FV-1 -> playback acceptance ==="
    Log "WARNING: prevent acoustic feedback before continuing."

    $LiveLines = Invoke-FV1Live `
        -Program $Representative.FullName `
        -Seconds ([Math]::Max(10.0, [Math]::Min($SoakSeconds, 60.0))) `
        -Label "Live-input hardware pass" `
        -Live

    Assert-CleanTelemetry `
        -Lines $LiveLines `
        -Label "Live-input hardware pass"
}

Log ""
Log "=== PHASE 9C.1 WINDOWS RC TORTURE AUTOMATION PASSED ==="
Log ""
Log "Automated evidence:"
Log "  - every bundled SpinASM program executed through realtime host"
Log "  - $Cycles repeated host/runtime construction/start/stop/destruction cycles"
Log "  - $SoakSeconds second continuous realtime soak"
Log "  - zero output underruns required"
Log "  - zero analyzer drops required"
Log "  - device-lost=no required (normal timed shutdown may increment device stops)"
if ($RunLiveInput) {
    Log "  - real live-input full-duplex pass"
}
Log ""
Log "Important:"
Log "  This automates backend/session torture."
Log "  The final GUI/manual acceptance items still remain."
Log ""
Log "Report: $Report"
