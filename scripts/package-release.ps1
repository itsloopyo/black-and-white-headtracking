param([string]$Configuration = 'Release')

$ErrorActionPreference = 'Stop'

$repoRoot = Resolve-Path "$PSScriptRoot\.."
$binDir   = Join-Path $repoRoot "build\bin\$Configuration"
$outDir   = Join-Path $repoRoot 'release'

if (-not (Test-Path $binDir)) {
    throw "Build output not found at $binDir. Run: pixi run build-release"
}

$versionHeader = Join-Path $repoRoot 'src\version.h'
$match = (Select-String -Path $versionHeader -Pattern 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"').Matches
if (-not $match) { throw "Could not parse HEADTRACKING_VERSION_STRING from $versionHeader" }
$version = $match[0].Groups[1].Value

$modName = 'BlackAndWhiteHeadTracking'
$modSlug = 'black-and-white-headtracking'

$modFiles = @('HeadTracking.dll', 'bw-headtracking-launcher.exe')
foreach ($f in $modFiles) {
    if (-not (Test-Path (Join-Path $binDir $f))) {
        throw "Missing build output: $(Join-Path $binDir $f)"
    }
}
$iniSrc = Join-Path $repoRoot 'config\HeadTracking.ini'
if (-not (Test-Path $iniSrc)) { throw "Missing config/HeadTracking.ini" }

if (-not (Test-Path $outDir)) { New-Item -ItemType Directory -Path $outDir | Out-Null }

# ---------- Installer ZIP ----------
$installerStage = Join-Path $outDir "$modSlug-installer-stage"
if (Test-Path $installerStage) { Remove-Item $installerStage -Recurse -Force }
New-Item -ItemType Directory -Path $installerStage | Out-Null

$pluginsDir = Join-Path $installerStage 'plugins'
New-Item -ItemType Directory -Path $pluginsDir | Out-Null
foreach ($f in $modFiles) {
    Copy-Item (Join-Path $binDir $f) $pluginsDir
}
Copy-Item $iniSrc $pluginsDir

Copy-Item (Join-Path $repoRoot 'scripts\install.cmd')   $installerStage
Copy-Item (Join-Path $repoRoot 'scripts\uninstall.cmd') $installerStage

Import-Module (Join-Path $repoRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force
Copy-SharedBundle -StagingDir $installerStage -NoRefresh

foreach ($doc in @('README.md', 'LICENSE', 'CHANGELOG.md', 'THIRD-PARTY-NOTICES.md')) {
    $p = Join-Path $repoRoot $doc
    if (Test-Path $p) { Copy-Item $p $installerStage }
}

$installerZip = Join-Path $outDir "$modName-v$version-installer.zip"
if (Test-Path $installerZip) { Remove-Item $installerZip -Force }
Compress-Archive -Path "$installerStage\*" -DestinationPath $installerZip
Remove-Item $installerStage -Recurse -Force
Write-Host "Packaged installer: $installerZip" -ForegroundColor Green

# ---------- Nexus ZIP (extract-to-game-folder) ----------
$nexusStage = Join-Path $outDir "$modSlug-nexus-stage"
if (Test-Path $nexusStage) { Remove-Item $nexusStage -Recurse -Force }
New-Item -ItemType Directory -Path $nexusStage | Out-Null

foreach ($f in $modFiles) {
    Copy-Item (Join-Path $binDir $f) $nexusStage
}
Copy-Item $iniSrc $nexusStage

$nexusZip = Join-Path $outDir "$modName-v$version-nexus.zip"
if (Test-Path $nexusZip) { Remove-Item $nexusZip -Force }
Compress-Archive -Path "$nexusStage\*" -DestinationPath $nexusZip
Remove-Item $nexusStage -Recurse -Force
Write-Host "Packaged nexus:     $nexusZip" -ForegroundColor Green
