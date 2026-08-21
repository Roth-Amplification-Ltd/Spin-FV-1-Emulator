param(
    [Parameter(Mandatory=$true)]
    [string]$ZipPath,

    [string]$ProgramPath = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($env:OS -ne "Windows_NT") {
    throw "This verifier is intended for Windows."
}

$ZipPath = (Resolve-Path $ZipPath).Path

if ([string]::IsNullOrWhiteSpace($ProgramPath)) {
    $RepoRoot = Resolve-Path (Join-Path $PSScriptRoot "..")
    $ProgramPath = Join-Path $RepoRoot "examples\simple_passthrough.spn"
}
$ProgramPath = (Resolve-Path $ProgramPath).Path

$Scratch = Join-Path $env:TEMP (
    "fv1lab-portable-verify-" + [Guid]::NewGuid().ToString("N")
)

New-Item -ItemType Directory -Force -Path $Scratch | Out-Null

$OldPath = $env:PATH
$OldQtPluginPath = [Environment]::GetEnvironmentVariable(
    "QT_PLUGIN_PATH",
    "Process"
)
$OldQtDir = [Environment]::GetEnvironmentVariable(
    "QTDIR",
    "Process"
)
$OldLocation = Get-Location

try {
    Write-Host ""
    Write-Host "=== Extract portable FV-1 Lab package ==="
    Write-Host "ZIP: $ZipPath"
    Write-Host "TMP: $Scratch"

    Expand-Archive -Path $ZipPath -DestinationPath $Scratch -Force

    $Exe = Join-Path $Scratch "bin\FV1Lab.exe"

    foreach ($required in @(
        $Exe,
        (Join-Path $Scratch "bin\Qt6Core.dll"),
        (Join-Path $Scratch "bin\Qt6Gui.dll"),
        (Join-Path $Scratch "bin\Qt6Widgets.dll"),
        (Join-Path $Scratch "bin\platforms\qwindows.dll"),
        (Join-Path $Scratch "share\spin-fv1-emulator\splash\FV1LabSplashImagebase.png"),
        (Join-Path $Scratch "share\spin-fv1-emulator\icons\fv1-emulator-silver.png"),
        (Join-Path $Scratch "BUILD-INFO.txt")
    )) {
        if (-not (Test-Path $required)) {
            throw "Portable package is missing required file: $required"
        }
    }

    #
    # Prove the package does not accidentally borrow Qt from a development kit.
    # The executable directory still participates in normal Windows DLL search,
    # but PATH is reduced to Windows itself and all Qt override variables are
    # removed for the smoke run.
    #
    $env:PATH = @(
        (Join-Path $env:SystemRoot "System32"),
        $env:SystemRoot,
        (Join-Path $env:SystemRoot "System32\Wbem"),
        (Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0")
    ) -join ";"

    Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:QTDIR -ErrorAction SilentlyContinue

    Set-Location "$($env:SystemDrive)\"

    Write-Host ""
    Write-Host "=== Neutral-environment portable smoke ==="

    function Invoke-PortableGuiSmoke {
        param(
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

        $Process = Start-Process `
            -FilePath $Exe `
            -ArgumentList ($QuotedArguments -join " ") `
            -WorkingDirectory ($env:SystemDrive + "\") `
            -Wait `
            -PassThru

        if ($Process.ExitCode -ne 0) {
            throw "Portable FV1Lab.exe $($Arguments -join ' ') failed with exit code $($Process.ExitCode)"
        }

        Write-Host "    exit: $($Process.ExitCode)"
    }

    foreach ($arg in @("--smoke", "--smoke-splash", "--smoke-about")) {
        Write-Host "  $arg"
        Invoke-PortableGuiSmoke -Arguments @($arg)
    }

    Write-Host "  --smoke-open $ProgramPath"
    Invoke-PortableGuiSmoke -Arguments @("--smoke-open", $ProgramPath)

    $Version = (Get-Item $Exe).VersionInfo
    if ($Version.ProductName -ne "FV-1 Lab") {
        throw "Unexpected Windows ProductName: '$($Version.ProductName)'"
    }

    Write-Host ""
    Write-Host "PORTABLE WINDOWS PACKAGE VERIFIED"
    Write-Host "  Qt development environment: NOT USED"
    Write-Host "  Product: $($Version.ProductName)"
    Write-Host "  File version: $($Version.FileVersion)"
    Write-Host "  Command-line .spn open: PASS"
}
finally {
    Set-Location $OldLocation
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

    Remove-Item -Recurse -Force $Scratch -ErrorAction SilentlyContinue
}
