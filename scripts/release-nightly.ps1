[CmdletBinding()]
param([switch]$AllowDirty)

$ErrorActionPreference = 'Stop'

$ProjectRoot = Resolve-Path (Join-Path $PSScriptRoot '..')

Import-Module (Join-Path $ProjectRoot 'cameraunlock-core\powershell\NightlyRelease.psm1') -Force

$versionHeader = Join-Path $ProjectRoot 'src\version.h'
$match = (Select-String -Path $versionHeader -Pattern 'HEADTRACKING_VERSION_STRING\s+"([^"]+)"').Matches
if (-not $match) { throw "Could not parse HEADTRACKING_VERSION_STRING from $versionHeader" }
$version = $match[0].Groups[1].Value

Publish-NightlyBuild `
    -ModId 'black-and-white' `
    -ModName 'BlackAndWhiteHeadTracking' `
    -Version $version `
    -ProjectRoot $ProjectRoot `
    -AllowDirty:$AllowDirty
