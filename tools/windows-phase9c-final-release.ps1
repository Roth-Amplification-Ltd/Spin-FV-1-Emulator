param(
    [string]$QtDir = "",
    [string]$VcpkgRoot = "",
    [switch]$PreflightOnly,
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
$ExpectedVersion = "1.0.0"
$ExpectedTag = "v1.0.0"

function Assert-FinalVersionSource {
    $CMake = Get-Content (Join-Path $Root "CMakeLists.txt") -Raw
    if (-not $CMake.Contains('set(FV1_RELEASE_CHANNEL "" CACHE STRING')) {
        throw "CMake default release channel is not final/empty."
    }

    foreach ($Rel in @(
        "src\cli\main.cpp",
        "src\live\main.cpp",
        "src\gui\main.cpp"
    )) {
        $Text = Get-Content (Join-Path $Root $Rel) -Raw
        if ($Text.Contains("1.0.0-rc1")) {
            throw "$Rel still contains the rc1 shipping-version fallback."
        }
    }
}

function Assert-ReleaseBinaryVersions {
    $ReleaseDir = Join-Path $Root "build-windows\Release"
    $Cli = Join-Path $ReleaseDir "fv1-cli.exe"
    $Live = Join-Path $ReleaseDir "fv1-live.exe"
    $Gui = Join-Path $ReleaseDir "FV1Lab.exe"

    foreach ($Required in @($Cli, $Live, $Gui)) {
        if (-not (Test-Path $Required)) {
            throw "Missing Release binary: $Required"
        }
    }

    $CliVersion = (& $Cli --version).Trim()
    $LiveVersion = (& $Live --version).Trim()
    if ($CliVersion -ne "Spin FV-1 Emulator $ExpectedVersion") {
        throw "Unexpected fv1-cli version: $CliVersion"
    }
    if ($LiveVersion -ne "Spin FV-1 Emulator $ExpectedVersion") {
        throw "Unexpected fv1-live version: $LiveVersion"
    }

    $GuiInfo = (Get-Item $Gui).VersionInfo
    if ([string]$GuiInfo.ProductVersion -ne $ExpectedVersion) {
        throw "FV1Lab ProductVersion is '$($GuiInfo.ProductVersion)', expected '$ExpectedVersion'."
    }
    if ([string]$GuiInfo.FileVersion -ne $ExpectedVersion) {
        throw "FV1Lab FileVersion is '$($GuiInfo.FileVersion)', expected '$ExpectedVersion'."
    }

    Write-Host "Version source/binaries: $ExpectedVersion"
}

Write-Host ""
Write-Host "FV-1 Lab Phase 9C.2 Final Windows 1.0.0 Release"
Write-Host "================================================"
Write-Host ""

Assert-FinalVersionSource

& git.exe -C $Root diff --check
if ($LASTEXITCODE -ne 0) {
    throw "git diff --check failed."
}

$QtDir = Resolve-FV1QtDir $QtDir

if ($PreflightOnly) {
    Write-Host "=== Final-version preflight (dirty tree permitted) ==="

    $TestArgs = @{
        Config = "Release"
        QtDir = $QtDir
        Clean = $true
    }
    if ($VcpkgRoot) {
        $TestArgs["VcpkgRoot"] = $VcpkgRoot
    }

    $global:LASTEXITCODE = 0
    & (Join-Path $PSScriptRoot "test-windows.ps1") @TestArgs
    if (-not $?) {
        throw "Windows Release regression failed during final-version preflight."
    }

    Assert-ReleaseBinaryVersions

    Write-Host ""
    Write-Host "=== PHASE 9C.2 FINAL VERSION PREFLIGHT PASSED ==="
    Write-Host ""
    Write-Host "Next:"
    Write-Host "  inspect/stage/commit/push the 1.0.0 promotion"
    Write-Host "  then rerun this script WITHOUT -PreflightOnly"
    exit 0
}

Write-Host "=== Clean/taggable source preflight ==="
$Branch = (& git.exe -C $Root branch --show-current).Trim()
if ($Branch -ne "main") {
    throw "Final release must run from main; current branch is '$Branch'."
}

$Dirty = @(& git.exe -C $Root status --porcelain --untracked-files=normal)
if ($Dirty.Count -ne 0) {
    throw "Final release requires a clean working tree. Commit/stash/remove changes first."
}

$Head = (& git.exe -C $Root rev-parse HEAD).Trim()
$OriginMain = ""
try {
    $OriginMain = (& git.exe -C $Root rev-parse origin/main 2>$null).Trim()
} catch {
}
if ($OriginMain -and $OriginMain -ne $Head) {
    throw "HEAD $Head does not match origin/main $OriginMain. Push the promotion commit first."
}

$ExistingTag = @(& git.exe -C $Root tag --list $ExpectedTag)
if ($ExistingTag) {
    throw "Tag $ExpectedTag already exists. Refusing to manufacture a second final release."
}

Write-Host "HEAD:    $Head"
Write-Host "Version: $ExpectedVersion"
Write-Host ""

Write-Host "=== Full Phase 9C final release gate ==="
$GateArgs = @{ QtDir = $QtDir }
if ($VcpkgRoot) { $GateArgs["VcpkgRoot"] = $VcpkgRoot }
if ($RunHardware) { $GateArgs["RunHardware"] = $true }
if ($RunLiveInput) { $GateArgs["RunLiveInput"] = $true }
if ($OutputId) { $GateArgs["OutputId"] = $OutputId }
if ($InputId) { $GateArgs["InputId"] = $InputId }

$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "phase9c-windows-release-gate.ps1") @GateArgs
if (-not $?) {
    throw "Phase 9C final release gate failed."
}

Assert-ReleaseBinaryVersions

$Zip = Join-Path $Root "dist\windows\FV1Lab-$ExpectedVersion-windows-x64.zip"
$Sha = "$Zip.sha256"
$ManifestPath = "$Zip.manifest.json"

foreach ($Required in @($Zip, $Sha, $ManifestPath)) {
    if (-not (Test-Path $Required)) {
        throw "Final release artifact missing: $Required"
    }
}

Write-Host ""
Write-Host "=== Exact final artifact verification ==="
$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "windows-release-artifact-check.ps1") -ZipPath $Zip
if (-not $?) {
    throw "Final artifact verifier failed."
}

$Manifest = Get-Content $ManifestPath -Raw | ConvertFrom-Json
if ([string]$Manifest.version -ne $ExpectedVersion) {
    throw "Manifest version '$($Manifest.version)' is not '$ExpectedVersion'."
}
if ([string]$Manifest.commit -ne $Head) {
    throw "Manifest commit '$($Manifest.commit)' does not match release HEAD '$Head'."
}
if ([string]$Manifest.zip -ne [IO.Path]::GetFileName($Zip)) {
    throw "Manifest ZIP filename does not match final artifact name."
}

Write-Host ""
Write-Host "=== Local neutral clean-machine verification ==="
$global:LASTEXITCODE = 0
& (Join-Path $PSScriptRoot "windows-phase9c-clean-machine.ps1") -ZipPath $Zip
if (-not $?) {
    throw "Local clean-machine verification failed."
}

$EvidenceDir = Join-Path $Root "build-phase9c-windows"
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$Evidence = Join-Path $EvidenceDir (
    "phase9c2-final-release-" + (Get-Date -Format "yyyyMMdd-HHmmss") + ".txt"
)
$ActualSha = (Get-FileHash -Algorithm SHA256 $Zip).Hash.ToLowerInvariant()

$EvidenceText = @(
    "FV-1 Lab Windows 1.0.0 final release evidence",
    "Version: $ExpectedVersion",
    "Commit: $Head",
    "Tag to create: $ExpectedTag",
    "ZIP: $Zip",
    "SHA256: $ActualSha",
    "Manifest: $ManifestPath",
    "Qt: $($Manifest.qt)",
    "Compiler: $($Manifest.compiler)",
    "Architecture: $($Manifest.architecture)",
    "Frontend: $($Manifest.frontend)",
    "Audio: $($Manifest.audio)",
    "",
    "Verified:",
    "- clean main branch",
    "- HEAD matches origin/main when available",
    "- final release channel is empty",
    "- fv1-cli/fv1-live/FV1Lab report 1.0.0",
    "- full Phase 9C automated Windows release gate",
    "- exact final ZIP/SHA/manifest",
    "- artifact commit equals release HEAD",
    "- neutral packaged GUI smoke",
    "- all bundled SpinASM packaged-open coverage",
    "- local clean-machine verifier"
)
$EvidenceText | Set-Content -Path $Evidence -Encoding UTF8

Write-Host ""
Write-Host "=== PHASE 9C.2 WINDOWS 1.0.0 FINAL RELEASE GATE PASSED ==="
Write-Host "Version:  $ExpectedVersion"
Write-Host "Commit:   $Head"
Write-Host "ZIP:      $Zip"
Write-Host "SHA256:   $ActualSha"
Write-Host "Evidence: $Evidence"
Write-Host ""
Write-Host "Tag only after the remaining human launch/visual check:"
Write-Host "  git tag -a $ExpectedTag -m `"FV-1 Lab $ExpectedVersion`""
Write-Host "  git push origin $ExpectedTag"
