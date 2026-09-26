[CmdletBinding()]
param([switch]$AllowDirty)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

$versionHeader = Join-Path $ProjectRoot 'src\version.h'
$match = (Select-String -Path $versionHeader -Pattern 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"').Matches
if (-not $match) { throw "Could not parse HEADTRACKING_VERSION_STRING from $versionHeader" }
$version = $match[0].Groups[1].Value

# A nightly carries version.h's version, and validate-manifest refuses a build whose version is
# below config.canonical_since; nothing on the nightly path runs it.
$canonicalSince = (Get-Content (Join-Path $ProjectRoot 'launcher-manifest.json') -Raw | ConvertFrom-Json).config.canonical_since
if ([version]$version -lt [version]$canonicalSince) {
    throw "version.h is $version, below launcher-manifest.json's config.canonical_since ($canonicalSince). Cut the $canonicalSince release first, then nightlies."
}

Publish-NightlyBuild `
    -ModId 'black-and-white' `
    -ModName 'BlackAndWhiteHeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -AllowDirty:$AllowDirty
