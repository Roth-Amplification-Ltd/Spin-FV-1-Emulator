param(
    [string]$ZipPath = "",
    [string]$ProgramRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

if ($env:OS -ne "Windows_NT") {
    throw "This verifier is intended for Windows."
}

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

if ([string]::IsNullOrWhiteSpace($ProgramRoot)) {
    $ProgramRoot = Join-Path $Root "examples"
}
$ProgramRoot = (Resolve-Path $ProgramRoot).Path

if ([string]::IsNullOrWhiteSpace($ZipPath)) {
    $Candidate = Get-ChildItem (Join-Path $Root "dist\windows") `
        -Filter "FV1Lab-*-windows-x64.zip" -File |
        Sort-Object LastWriteTime -Descending |
        Select-Object -First 1
    if (-not $Candidate) {
        throw "No Windows portable ZIP found under dist\windows."
    }
    $ZipPath = $Candidate.FullName
}

$ZipPath = (Resolve-Path $ZipPath).Path
$ShaPath = "$ZipPath.sha256"
$ManifestPath = "$ZipPath.manifest.json"

foreach ($Required in @($ShaPath, $ManifestPath)) {
    if (-not (Test-Path $Required)) {
        throw "Missing release sidecar: $Required"
    }
}

$ExpectedSha = ((Get-Content $ShaPath -Raw).Trim() -split '\s+')[0].ToLowerInvariant()
$ActualSha = (Get-FileHash -Algorithm SHA256 $ZipPath).Hash.ToLowerInvariant()
if ($ExpectedSha -ne $ActualSha) {
    throw "SHA-256 mismatch: expected $ExpectedSha got $ActualSha"
}

$Manifest = Get-Content $ManifestPath -Raw | ConvertFrom-Json
$Head = (& git.exe -C $Root rev-parse HEAD).Trim()

if ($Manifest.schema -ne "fv1lab-windows-release-v1") {
    throw "Unexpected manifest schema: $($Manifest.schema)"
}
if ($Manifest.product -ne "FV-1 Lab") {
    throw "Unexpected product: $($Manifest.product)"
}
if ($Manifest.commit -ne $Head) {
    throw "Manifest commit $($Manifest.commit) does not match HEAD $Head"
}
if ($Manifest.sha256.ToLowerInvariant() -ne $ActualSha) {
    throw "Manifest SHA does not match ZIP SHA"
}

$Scratch = Join-Path $env:TEMP ("fv1lab-9c-" + [Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $Scratch | Out-Null

$OldPath = $env:PATH
$OldQtPluginPath = [Environment]::GetEnvironmentVariable("QT_PLUGIN_PATH", "Process")
$OldQtDir = [Environment]::GetEnvironmentVariable("QTDIR", "Process")

function Invoke-PackagedGui {
    param(
        [Parameter(Mandatory=$true)]
        [string]$Exe,

        [Parameter(Mandatory=$true)]
        [string[]]$Arguments
    )

    if ($Arguments.Count -eq 0) {
        throw "Invoke-PackagedGui received no arguments."
    }

    $QuotedArguments = foreach ($Argument in $Arguments) {
        if ($Argument -match '[\s"]') {
            '"' + ($Argument -replace '"', '\"') + '"'
        } else {
            $Argument
        }
    }

    $ArgumentLine = $QuotedArguments -join " "
    if ([string]::IsNullOrWhiteSpace($ArgumentLine)) {
        throw "Invoke-PackagedGui produced an empty ArgumentList."
    }

    $P = Start-Process `
        -FilePath $Exe `
        -ArgumentList $ArgumentLine `
        -WorkingDirectory ($env:SystemDrive + "\") `
        -Wait `
        -PassThru

    if ($P.ExitCode -ne 0) {
        throw "Packaged FV1Lab.exe failed: $($Arguments -join ' ') => $($P.ExitCode)"
    }

    Write-Host "    exit: $($P.ExitCode)"
}

try {
    Expand-Archive -LiteralPath $ZipPath -DestinationPath $Scratch -Force

    $Exe = Join-Path $Scratch "bin\FV1Lab.exe"
    foreach ($Required in @(
        $Exe,
        (Join-Path $Scratch "bin\Qt6Core.dll"),
        (Join-Path $Scratch "bin\Qt6Gui.dll"),
        (Join-Path $Scratch "bin\Qt6Widgets.dll"),
        (Join-Path $Scratch "bin\platforms\qwindows.dll"),
        (Join-Path $Scratch "share\spin-fv1-emulator\splash\FV1LabSplashImagebase.png"),
        (Join-Path $Scratch "BUILD-INFO.txt"),
        (Join-Path $Scratch "LICENSE.txt"),
        (Join-Path $Scratch "README.md")
    )) {
        if (-not (Test-Path $Required)) {
            throw "Release ZIP missing: $Required"
        }
    }

    $Debris = Get-ChildItem $Scratch -Recurse -File |
        Where-Object { $_.Extension -in @(".pdb",".ilk",".obj",".pch",".idb",".ipdb",".iobj") }
    if ($Debris) {
        throw "Developer build debris leaked into release ZIP."
    }

    $BuildInfo = Get-Content (Join-Path $Scratch "BUILD-INFO.txt") -Raw
    if (-not $BuildInfo.Contains("Commit: $Head")) {
        throw "BUILD-INFO.txt does not match current HEAD"
    }
    if (-not $BuildInfo.Contains("Version: $($Manifest.version)")) {
        throw "BUILD-INFO.txt version does not match release manifest"
    }

    $Version = (Get-Item $Exe).VersionInfo
    if ($Version.ProductName -ne "FV-1 Lab") {
        throw "Unexpected ProductName: $($Version.ProductName)"
    }
    if ([string]$Version.ProductVersion -ne [string]$Manifest.version) {
        throw "ProductVersion $($Version.ProductVersion) does not match manifest $($Manifest.version)"
    }
    if ([string]$Version.FileVersion -ne [string]$Manifest.version) {
        throw "FileVersion $($Version.FileVersion) does not match manifest $($Manifest.version)"
    }

    $env:PATH = @(
        (Join-Path $env:SystemRoot "System32"),
        $env:SystemRoot,
        (Join-Path $env:SystemRoot "System32\Wbem"),
        (Join-Path $env:SystemRoot "System32\WindowsPowerShell\v1.0")
    ) -join ";"
    Remove-Item Env:QT_PLUGIN_PATH -ErrorAction SilentlyContinue
    Remove-Item Env:QTDIR -ErrorAction SilentlyContinue

    Write-Host ""
    Write-Host "=== Neutral packaged GUI smoke ==="
    $SmokeCases = @(
        @("--smoke"),
        @("--smoke-desktop"),
        @("--smoke-splash"),
        @("--smoke-about")
    )

    foreach ($SmokeArgs in $SmokeCases) {
        $SmokeArgs = @($SmokeArgs)
        Write-Host "  $($SmokeArgs -join ' ')"
        Invoke-PackagedGui -Exe $Exe -Arguments $SmokeArgs
    }

    $Programs = Get-ChildItem $ProgramRoot -Recurse -File -Filter "*.spn" |
        Sort-Object FullName
    if (-not $Programs) {
        throw "No SpinASM programs found under $ProgramRoot"
    }

    Write-Host ""
    Write-Host "=== Packaged open of every bundled SpinASM program ==="
    $Count = 0
    foreach ($Program in $Programs) {
        Write-Host "  $($Program.FullName)"
        Invoke-PackagedGui -Exe $Exe -Arguments @("--smoke-open", $Program.FullName)
        $Count++
    }

    Write-Host ""
    Write-Host "PHASE 9C WINDOWS RELEASE ARTIFACT VERIFIED"
    Write-Host "  Version:      $($Manifest.version)"
    Write-Host "  Commit:       $($Manifest.commit)"
    Write-Host "  Qt:           $($Manifest.qt)"
    Write-Host "  SHA256:       $ActualSha"
    Write-Host "  Programs:     $Count"
    Write-Host "  Qt dev env:   NOT USED"
    Write-Host "  Build debris: NONE"
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
    Remove-Item -Recurse -Force $Scratch -ErrorAction SilentlyContinue
}
