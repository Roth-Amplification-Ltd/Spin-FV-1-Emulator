param(
    [Parameter(Mandatory=$true)]
    [string]$ZipPath,

    [string]$ShaPath = "",

    [string]$ManifestPath = "",

    [switch]$KeepExtracted
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($env:OS -ne "Windows_NT") {
    throw "This clean-machine verifier must run on Windows."
}

$ZipPath = (Resolve-Path $ZipPath).Path

if ([string]::IsNullOrWhiteSpace($ShaPath)) {
    $ShaPath = "$ZipPath.sha256"
}
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = "$ZipPath.manifest.json"
}

$ShaPath = (Resolve-Path $ShaPath).Path
$ManifestPath = (Resolve-Path $ManifestPath).Path

$ExpectedSha = ((Get-Content $ShaPath -Raw).Trim() -split '\s+')[0].ToLowerInvariant()
$ActualSha = (Get-FileHash -Algorithm SHA256 $ZipPath).Hash.ToLowerInvariant()

if ($ExpectedSha -ne $ActualSha) {
    throw "SHA-256 mismatch: expected $ExpectedSha got $ActualSha"
}

$Manifest = Get-Content $ManifestPath -Raw | ConvertFrom-Json

if ($Manifest.schema -ne "fv1lab-windows-release-v1") {
    throw "Unexpected release manifest schema: $($Manifest.schema)"
}
if ($Manifest.product -ne "FV-1 Lab") {
    throw "Unexpected release product: $($Manifest.product)"
}
if ($Manifest.sha256.ToLowerInvariant() -ne $ActualSha) {
    throw "Manifest SHA does not match ZIP SHA."
}
if ($Manifest.zip -ne [IO.Path]::GetFileName($ZipPath)) {
    throw "Manifest ZIP filename does not match artifact."
}

$Scratch = Join-Path $env:TEMP (
    "fv1lab-clean-machine-" + [Guid]::NewGuid().ToString("N")
)
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null

$OldPath = $env:PATH
$OldQtPluginPath = [Environment]::GetEnvironmentVariable("QT_PLUGIN_PATH", "Process")
$OldQtDir = [Environment]::GetEnvironmentVariable("QTDIR", "Process")

function Invoke-PackagedSmoke {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Exe,

        [Parameter(Mandatory=$true)]
        [string[]]$Arguments
    )

    $QuotedArguments = foreach ($Argument in $Arguments) {
        if ($Argument -match '[\s"]') {
            '"' + ($Argument -replace '"', '\"') + '"'
        } else {
            $Argument
        }
    }

    $ArgumentLine = $QuotedArguments -join " "

    $Process = Start-Process `
        -FilePath $Exe `
        -ArgumentList $ArgumentLine `
        -WorkingDirectory ($env:SystemDrive + "\") `
        -Wait `
        -PassThru

    if ($Process.ExitCode -ne 0) {
        throw "FV1Lab packaged smoke failed: $($Arguments -join ' ') -> $($Process.ExitCode)"
    }

    Write-Host "  $($Arguments -join ' ') -> exit 0"
}

try {
    Write-Host ""
    Write-Host "FV-1 Lab Windows clean-machine RC verification"
    Write-Host "==============================================="
    Write-Host "ZIP:      $ZipPath"
    Write-Host "Version:  $($Manifest.version)"
    Write-Host "Commit:   $($Manifest.commit)"
    Write-Host "SHA256:   $ActualSha"
    Write-Host ""

    Expand-Archive -LiteralPath $ZipPath -DestinationPath $Scratch -Force

    $Exe = Join-Path $Scratch "bin\FV1Lab.exe"

    foreach ($Required in @(
        $Exe,
        (Join-Path $Scratch "bin\Qt6Core.dll"),
        (Join-Path $Scratch "bin\Qt6Gui.dll"),
        (Join-Path $Scratch "bin\Qt6Widgets.dll"),
        (Join-Path $Scratch "bin\platforms\qwindows.dll")
    )) {
        if (-not (Test-Path $Required)) {
            throw "Portable package missing required runtime file: $Required"
        }
    }

    $env:PATH = @(
        (Join-Path $env:SystemRoot "System32"),
        $env:SystemRoot,
        (Join-Path $env:SystemRoot "System32\Wbem"),
        (Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0")
    ) -join ";"

    Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:QTDIR -ErrorAction SilentlyContinue

    Write-Host "=== Portable GUI smoke ==="
    Invoke-PackagedSmoke -Exe $Exe -Arguments @("--smoke")
    Invoke-PackagedSmoke -Exe $Exe -Arguments @("--smoke-desktop")
    Invoke-PackagedSmoke -Exe $Exe -Arguments @("--smoke-splash")
    Invoke-PackagedSmoke -Exe $Exe -Arguments @("--smoke-about")

    Write-Host ""
    Write-Host "=== PHASE 9C CLEAN-MACHINE RC VERIFICATION PASSED ==="
    Write-Host "Qt development environment: NOT USED"
    Write-Host "Version: $($Manifest.version)"
    Write-Host "Commit:  $($Manifest.commit)"
    Write-Host "SHA256:  $ActualSha"
    Write-Host ""
    Write-Host "Now launch the packaged application interactively:"
    Write-Host "  $Exe"
}
finally {
    $env:PATH = $OldPath

    if ($null -eq $OldQtPluginPath) {
        Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
    } else {
        $env:QT_PLUGIN_PATH = $OldQtPluginPath
    }

    if ($null -eq $OldQtDir) {
        Remove-Item Env:QTDIR -ErrorAction SilentlyContinue
    } else {
        $env:QTDIR = $OldQtDir
    }

    if ($KeepExtracted) {
        Write-Host "Kept extracted RC at: $Scratch"
    } else {
        Remove-Item -Recurse -Force $Scratch -ErrorAction SilentlyContinue
    }
}
