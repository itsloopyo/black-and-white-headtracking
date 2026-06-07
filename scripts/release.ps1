param(
    [Parameter(Mandatory = $true, Position = 0)]
    [string]$Version,
    # Ship a release even when there are no user-facing commits since the
    # last tag (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

$ErrorActionPreference = 'Stop'

if (-not $Version) {
    Write-Error "Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>"
    exit 1
}

if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

$repoRoot = Resolve-Path "$PSScriptRoot\.."
Set-Location $repoRoot

Import-Module (Join-Path $repoRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force

# Mirrors New-ChangelogFromCommits' insertion so a -Force maintenance entry
# lands in the same place with the same shape.
function Add-MaintenanceChangelogEntry {
    param([string]$Path, [string]$NewVersion)
    $date = Get-Date -Format 'yyyy-MM-dd'
    $entry = "## [$NewVersion] - $date`n`n### Changed`n`n- Maintenance release (no user-facing changes).`n`n"
    $changelog = Get-Content $Path -Raw
    if ($changelog -match '(?s)(# Changelog.*?)(## \[)') {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n\n)', "`$1$entry"
    } else {
        $changelog = $changelog -replace '(?s)(# Changelog.*?\n)', "`$1$entry"
    }
    $changelog = $changelog.TrimEnd() + "`n"
    Set-Content $Path $changelog -NoNewline
}

$versionHeader = Join-Path $repoRoot 'src\version.h'
if (-not (Test-Path $versionHeader)) { throw "Missing $versionHeader" }
$currentMatch = (Select-String -Path $versionHeader -Pattern 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"').Matches
if (-not $currentMatch) { throw "Could not parse current version from $versionHeader" }
$currentVersion = $currentMatch[0].Groups[1].Value

$newVersion = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $currentVersion
if (-not (Test-SemanticVersion -Version $newVersion)) {
    throw "Resolved version '$newVersion' is not semver X.Y.Z"
}
Write-Host "Releasing v$newVersion (current: v$currentVersion)" -ForegroundColor Cyan

$branch = (& git rev-parse --abbrev-ref HEAD).Trim()
if ($branch -ne 'main') { throw "Must release from 'main' branch (currently on '$branch')." }

if (-not $Force -and -not (Test-CleanGitStatus)) { throw "Working tree is not clean. Commit or stash before releasing." }

$tag = "v$newVersion"
if (Test-GitTagExists -Tag $tag) { throw "Tag $tag already exists." }

$changelogPath = Join-Path $repoRoot 'CHANGELOG.md'

# Changelog - generate it BEFORE mutating any version files so an abort here
# (no user-facing commits since the last tag) leaves the working tree clean
# instead of stranding a half-applied version bump with no tag.
Write-Host "Generating CHANGELOG..." -ForegroundColor Cyan
$hasExistingTags = git tag -l 2>$null
if (-not $hasExistingTags) {
    # First release - ensure a baseline CHANGELOG exists
    if (-not (Test-Path $changelogPath)) {
        $date = Get-Date -Format 'yyyy-MM-dd'
        "# Changelog`n`n## [$newVersion] - $date`n`nFirst release.`n" | Set-Content $changelogPath
        Write-Host "  Wrote initial CHANGELOG.md" -ForegroundColor Gray
    }
} else {
    try {
        New-ChangelogFromCommits -ChangelogPath $changelogPath -Version $newVersion | Out-Null
    } catch {
        if (-not $Force) {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host "No user-facing changes to release. Re-run with -Force for a maintenance release." -ForegroundColor Yellow
            exit 1
        }
        Write-Host "No user-facing commits since last tag - writing maintenance entry (-Force)." -ForegroundColor Yellow
        Add-MaintenanceChangelogEntry -Path $changelogPath -NewVersion $newVersion
    }
}

# Update version.h
if (-not ($newVersion -match '^(\d+)\.(\d+)\.(\d+)$')) {
    throw "Resolved version '$newVersion' is not X.Y.Z"
}
$maj = [int]$matches[1]; $min = [int]$matches[2]; $pat = [int]$matches[3]
$header = Get-Content $versionHeader -Raw
$header = $header -replace '(#define\s+HEADTRACKING_VERSION_MAJOR\s+)\d+',   "`${1}$maj"
$header = $header -replace '(#define\s+HEADTRACKING_VERSION_MINOR\s+)\d+',   "`${1}$min"
$header = $header -replace '(#define\s+HEADTRACKING_VERSION_PATCH\s+)\d+',   "`${1}$pat"
$header = $header -replace '(#define\s+HEADTRACKING_VERSION_STRING\s+)"[^"]+"', "`${1}`"$newVersion`""
Set-Content -Path $versionHeader -Value $header -NoNewline

# Keep pixi.toml version in sync (canonical: version.h; pixi mirrors it).
$pixiPath = Join-Path $repoRoot 'pixi.toml'
$pixi = Get-Content $pixiPath -Raw
$pixi = $pixi -replace '(?m)^(version\s*=\s*)"[^"]+"', "`${1}`"$newVersion`""
Set-Content -Path $pixiPath -Value $pixi -NoNewline

# Keep CMakeLists.txt project version in sync (canonical: version.h; CMake mirrors it).
$cmakePath = Join-Path $repoRoot 'CMakeLists.txt'
$cmake = Get-Content $cmakePath -Raw
$cmake = $cmake -replace '(project\([^)]*?VERSION\s+)\d+\.\d+\.\d+', "`${1}$newVersion"
Set-Content -Path $cmakePath -Value $cmake -NoNewline

# Keep launcher-manifest.json version in sync (canonical: version.h; the
# manifest lopari reads mirrors it). Only mod_info.version moves.
$manifestPath = Join-Path $repoRoot 'launcher-manifest.json'
$manifest = Get-Content $manifestPath -Raw | ConvertFrom-Json
$manifest.mod_info.version = $newVersion
$manifestJson = ($manifest | ConvertTo-Json -Depth 10) -replace "`r`n", "`n"
$utf8NoBom = New-Object System.Text.UTF8Encoding($false)
[System.IO.File]::WriteAllText($manifestPath, $manifestJson, $utf8NoBom)

# Build
Write-Host "Building release..." -ForegroundColor Cyan
& pixi run build-release
if ($LASTEXITCODE -ne 0) { throw "pixi run build-release failed (exit $LASTEXITCODE)" }

# Commit version + changelog
$committed = Invoke-VersionCommit -Version $newVersion -Files @($versionHeader, $pixiPath, $cmakePath, $manifestPath, $changelogPath)
if (-not $committed) { throw "No changes were staged for the release commit." }

# Tag + push
New-ReleaseTag -Version $newVersion -Message "Release v$newVersion" -Branch 'main'

Write-Host "Release v$newVersion pushed. CI release workflow triggered on tag." -ForegroundColor Green
