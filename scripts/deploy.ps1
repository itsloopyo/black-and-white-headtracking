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

# Every copy on this machine gets the build. games.json's detection data covers
# BLACK_AND_WHITE_PATH, the game's own registry key and the default folder.
if ($GamePath) {
    $targets = @($GamePath)
} else {
    Import-Module (Join-Path $repoRoot 'cameraunlock-core\powershell\GamePathDetection.psm1') -Force
    $targets = @(Find-AllGamePaths -GameId 'black-and-white')
}
if ($targets.Count -eq 0) {
    throw "Black & White install not found. Pass -GamePath or set BLACK_AND_WHITE_PATH."
}

$files = @(
    'HeadTracking.dll',
    'bw-headtracking-launcher.exe'
)
foreach ($f in $files) {
    if (-not (Test-Path (Join-Path $binDir $f))) { throw "Missing build output: $(Join-Path $binDir $f)" }
}

foreach ($target in $targets) {
    if (-not (Test-Path (Join-Path $target 'runblack.exe'))) {
        throw "No runblack.exe in $target"
    }
    foreach ($f in $files) {
        Copy-Item -Force (Join-Path $binDir $f) (Join-Path $target $f)
    }
    Write-Host "Deployed $($files -join ', ') -> $target" -ForegroundColor Green
    Write-Host "  Launch via: $(Join-Path $target 'bw-headtracking-launcher.exe')" -ForegroundColor Cyan
}
Write-Host "`n$($targets.Count) install(s) updated." -ForegroundColor Cyan
