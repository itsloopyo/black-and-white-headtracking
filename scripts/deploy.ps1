param(
    [Parameter(Position = 0)]
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$GamePath
)

$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path "$PSScriptRoot\.."
$binDir = Join-Path $repoRoot "build\bin\$Configuration"

if (-not (Test-Path $binDir)) {
    throw "Build output not found at $binDir. Run: pixi run build-release"
}

if (-not $GamePath -and $env:BLACK_AND_WHITE_PATH) {
    $GamePath = $env:BLACK_AND_WHITE_PATH
}
if (-not $GamePath) {
    $persisted = [Environment]::GetEnvironmentVariable('BLACK_AND_WHITE_PATH', 'User')
    if ($persisted) { $GamePath = $persisted }
}
if (-not $GamePath) {
    foreach ($candidate in @(
        'C:\bin\Black and White',
        'C:\Program Files (x86)\Lionhead Studios Ltd\Black & White'
    )) {
        if (Test-Path (Join-Path $candidate 'runblack.exe')) { $GamePath = $candidate; break }
    }
}
if (-not $GamePath) {
    $module = Join-Path $repoRoot 'cameraunlock-core\powershell\GamePathDetection.psm1'
    if (Test-Path $module) {
        Import-Module $module -Force
        $GamePath = Find-GamePath -GameId 'black-and-white'
    }
}

if (-not $GamePath -or -not (Test-Path (Join-Path $GamePath 'runblack.exe'))) {
    throw "Black & White install not found. Pass -GamePath or set BLACK_AND_WHITE_PATH."
}

$files = @(
    'HeadTracking.dll',
    'bw-headtracking-launcher.exe'
)
foreach ($f in $files) {
    $src = Join-Path $binDir $f
    if (-not (Test-Path $src)) { throw "Missing build output: $src" }
    Copy-Item -Force $src (Join-Path $GamePath $f)
    Write-Host "Deployed $f -> $GamePath" -ForegroundColor Green
}

$iniSrc = Join-Path $repoRoot 'config\HeadTracking.ini'
$iniDst = Join-Path $GamePath 'HeadTracking.ini'
if (-not (Test-Path $iniDst)) {
    Copy-Item -Force $iniSrc $iniDst
    Write-Host "Deployed default HeadTracking.ini" -ForegroundColor Green
} else {
    Write-Host "Preserving existing HeadTracking.ini" -ForegroundColor Yellow
}

Write-Host "`nLaunch via: $(Join-Path $GamePath 'bw-headtracking-launcher.exe')" -ForegroundColor Cyan
