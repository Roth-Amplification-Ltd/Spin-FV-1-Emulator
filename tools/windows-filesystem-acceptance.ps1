param(
    [ValidateSet("Debug", "Release")]
    [string]$Config = "Debug",

    [string]$QtDir = "C:\Qt\6.11.1\msvc2022_64",

    [string]$BuildDir = "",

    [switch]$KeepArtifacts
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root "build-windows"
}

$GuiExe = Join-Path $BuildDir "$Config\FV1Lab.exe"
if (-not (Test-Path $GuiExe)) {
    throw "FV1Lab.exe not found: $GuiExe. Run tools\test-windows.ps1 first."
}

if (Test-Path (Join-Path $QtDir "bin")) {
    $env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"
}

function Convert-ToExtendedPath {
    param([Parameter(Mandatory = $true)][string]$Path)

    $Full = [System.IO.Path]::GetFullPath($Path)
    if ($Full.StartsWith("\\?\")) {
        return $Full
    }
    if ($Full.StartsWith("\\")) {
        return "\\?\UNC\" + $Full.Substring(2)
    }
    return "\\?\" + $Full
}

function New-ExtendedDirectory {
    param([Parameter(Mandatory = $true)][string]$Path)

    [void][System.IO.Directory]::CreateDirectory(
        (Convert-ToExtendedPath $Path)
    )
}

function Copy-ExtendedFile {
    param(
        [Parameter(Mandatory = $true)][string]$Source,
        [Parameter(Mandatory = $true)][string]$Destination
    )

    [System.IO.File]::Copy(
        (Convert-ToExtendedPath $Source),
        (Convert-ToExtendedPath $Destination),
        $true
    )
}

function New-Pcm16StereoWav {
    param([Parameter(Mandatory = $true)][string]$Path)

    $Rate = 48000
    $Frames = 1024
    $Channels = 2
    $Bits = 16
    $BlockAlign = $Channels * ($Bits / 8)
    $DataBytes = $Frames * $BlockAlign

    $Extended = Convert-ToExtendedPath $Path
    $Stream = [System.IO.File]::Open(
        $Extended,
        [System.IO.FileMode]::Create,
        [System.IO.FileAccess]::Write,
        [System.IO.FileShare]::None
    )

    try {
        $Writer = [System.IO.BinaryWriter]::new(
            $Stream,
            [System.Text.Encoding]::ASCII,
            $true
        )
        try {
            $Ascii = [System.Text.Encoding]::ASCII

            $Writer.Write([byte[]]$Ascii.GetBytes("RIFF"))
            $Writer.Write([uint32](36 + $DataBytes))
            $Writer.Write([byte[]]$Ascii.GetBytes("WAVE"))
            $Writer.Write([byte[]]$Ascii.GetBytes("fmt "))
            $Writer.Write([uint32]16)
            $Writer.Write([uint16]1)
            $Writer.Write([uint16]$Channels)
            $Writer.Write([uint32]$Rate)
            $Writer.Write([uint32]($Rate * $BlockAlign))
            $Writer.Write([uint16]$BlockAlign)
            $Writer.Write([uint16]$Bits)
            $Writer.Write([byte[]]$Ascii.GetBytes("data"))
            $Writer.Write([uint32]$DataBytes)

            for ($i = 0; $i -lt $Frames; $i++) {
                $Sample = [int16](
                    [Math]::Round(
                        [Math]::Sin(
                            2.0 * [Math]::PI * 440.0 *
                            $i / $Rate
                        ) * 5000.0
                    )
                )
                $Writer.Write($Sample)
                $Writer.Write($Sample)
            }
        }
        finally {
            $Writer.Dispose()
        }
    }
    finally {
        $Stream.Dispose()
    }
}

function Invoke-SmokeOpen {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$Label
    )

    Write-Host "--- $Label ---"

    $QuotedPath =
        '"' + $Path.Replace('"', '\"') + '"'

    $Process = Start-Process `
        -FilePath $GuiExe `
        -ArgumentList @("--smoke-open", $QuotedPath) `
        -Wait `
        -PassThru

    if ($Process.ExitCode -ne 0) {
        throw (
            "$Label failed with FV1Lab exit code " +
            $Process.ExitCode
        )
    }
}

function Assert-NoPartialArtifacts {
    param([Parameter(Mandatory = $true)][string]$Path)

    $Extended = Convert-ToExtendedPath $Path
    $Items = [System.IO.Directory]::EnumerateFiles(
        $Extended,
        "*",
        [System.IO.SearchOption]::AllDirectories
    )

    foreach ($Item in $Items) {
        if ([System.IO.Path]::GetFileName($Item).Contains(".partial-")) {
            throw "Found leftover transactional partial file: $Item"
        }
    }
}

$UnicodeComponent =
    "FV1-" +
    [char]0x00DC +
    "nicode-" +
    [char]0x6E2C +
    [char]0x8A66

$Token = [Guid]::NewGuid().ToString("N")
$Scratch = Join-Path $env:TEMP "fv1-phase9b3-$Token"
$UnicodeRoot = Join-Path $Scratch $UnicodeComponent

New-ExtendedDirectory $UnicodeRoot

$ProgramSource = Join-Path $Root "examples\simple_passthrough.spn"
$UnicodeProgram = Join-Path $UnicodeRoot (
    "P" +
    [char]0x00E1 +
    "ss-" +
    [char]0x6E2C +
    [char]0x8A66 +
    ".spn"
)
$UnicodeWav = Join-Path $UnicodeRoot (
    "L" +
    [char]0x00F6 +
    "op-" +
    [char]0x65E5 +
    [char]0x672C +
    ".wav"
)

Copy-ExtendedFile $ProgramSource $UnicodeProgram
New-Pcm16StereoWav $UnicodeWav

$LongRoot = Join-Path $Scratch "long-path"
$Segment = "segment-" + ("x" * 36)
while ($LongRoot.Length -lt 300) {
    $LongRoot = Join-Path $LongRoot $Segment
}
New-ExtendedDirectory $LongRoot

$LongProgram = Join-Path $LongRoot "long-unicode-program.spn"
$LongWav = Join-Path $LongRoot "long-unicode-loop.wav"
Copy-ExtendedFile $ProgramSource $LongProgram
New-Pcm16StereoWav $LongWav

Write-Host ""
Write-Host "FV-1 Lab Phase 9B.3 filesystem acceptance"
Write-Host "=========================================="
Write-Host "GUI:          $GuiExe"
Write-Host "Unicode root: $UnicodeRoot"
Write-Host "Long root:    $LongRoot"
Write-Host "Long length:  $($LongProgram.Length)"
Write-Host ""

try {
    Invoke-SmokeOpen $UnicodeProgram "Unicode SpinASM open"
    Invoke-SmokeOpen $UnicodeWav "Unicode WAV-loop open"
    Invoke-SmokeOpen $LongProgram "Long-path SpinASM open"
    Invoke-SmokeOpen $LongWav "Long-path WAV-loop open"

    Write-Host ""
    Write-Host "=== Underlying recorder/validation tests ==="

    $RecorderTest = Join-Path $BuildDir "$Config\fv1-recorder-tests.exe"
    $ValidationTest = Join-Path $BuildDir "$Config\fv1-validation-tests.exe"

    if (-not (Test-Path $RecorderTest)) {
        throw "Recorder test executable not found: $RecorderTest"
    }
    if (-not (Test-Path $ValidationTest)) {
        throw "Validation test executable not found: $ValidationTest"
    }

    & $RecorderTest
    if ($LASTEXITCODE -ne 0) {
        throw "fv1-recorder-tests failed with exit code $LASTEXITCODE"
    }

    & $ValidationTest
    if ($LASTEXITCODE -ne 0) {
        throw "fv1-validation-tests failed with exit code $LASTEXITCODE"
    }

    Assert-NoPartialArtifacts $Scratch

    Write-Host ""
    Write-Host "PHASE 9B.3 FILESYSTEM AUTOMATION PASSED"
    Write-Host ""
    Write-Host "Automated coverage:"
    Write-Host "  - Unicode .spn open"
    Write-Host "  - Unicode .wav open"
    Write-Host "  - >260-character .spn open"
    Write-Host "  - >260-character .wav open"
    Write-Host "  - Unicode recorder finalization"
    Write-Host "  - Unicode validation WAV/report/pack output"
    Write-Host "  - transactional .partial cleanup"
    Write-Host ""
    Write-Host "Next: complete docs\WINDOWS-PHASE9B3-CHECKLIST.md in the GUI."
}
finally {
    if ($KeepArtifacts) {
        Write-Host "Keeping acceptance artifacts: $Scratch"
    }
    else {
        try {
            [System.IO.Directory]::Delete(
                (Convert-ToExtendedPath $Scratch),
                $true
            )
        }
        catch {
            Write-Warning "Could not delete acceptance scratch tree: $Scratch"
        }
    }
}
