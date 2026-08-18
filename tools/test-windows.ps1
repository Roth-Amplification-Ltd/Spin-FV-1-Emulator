param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug",

    [string]$QtDir = "",
    [string]$VcpkgRoot = "",
    [string]$BuildDir = "",
    [switch]$Clean
)

. (Join-Path $PSScriptRoot "windows-qt-common.ps1")

Assert-FV1Windows
$Root = Get-FV1RepoRoot $PSScriptRoot
$null = Assert-FV1Command "ctest.exe"

if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $Root "build-windows"
}

$buildArgs = @{
    Config = $Config
    QtDir = $QtDir
    BuildDir = $BuildDir
}
if ($VcpkgRoot) {
    $buildArgs["VcpkgRoot"] = $VcpkgRoot
}
if ($Clean) {
    $buildArgs["Clean"] = $true
}

& (Join-Path $PSScriptRoot "build-windows.ps1") @buildArgs
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$QtDir = Resolve-FV1QtDir $QtDir

# Build-tree executables intentionally do not carry deployed Qt DLLs. Put the
# selected developer kit first for CTest and direct smoke execution.
$env:PATH = "$(Join-Path $QtDir 'bin');$env:PATH"
$env:QT_PLUGIN_PATH = Join-Path $QtDir "plugins"

Write-Host ""
Write-Host "=== Windows CTest suite ==="
& ctest.exe --test-dir $BuildDir -C $Config --output-on-failure
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$exe = Find-FV1BuiltExe -BuildDir $BuildDir -Config $Config

Write-Host ""
Write-Host "=== Direct Qt desktop smoke ==="
foreach ($arg in @("--smoke", "--smoke-splash", "--smoke-about")) {
    Write-Host "  $arg"
    & $exe $arg
    if ($LASTEXITCODE -ne 0) {
        throw "FV1Lab.exe $arg failed with exit code $LASTEXITCODE"
    }
}

$SmokeProgram = Join-Path $Root "examples\simple_passthrough.spn"
Write-Host "  --smoke-open $SmokeProgram"
& $exe --smoke-open $SmokeProgram
if ($LASTEXITCODE -ne 0) {
    throw "FV1Lab.exe command-line program-open smoke failed with exit code $LASTEXITCODE"
}

Write-Host ""
Write-Host "WINDOWS QT TEST GATE PASSED"
Write-Host "Executable: $exe"
