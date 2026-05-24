# Run a Jython script against the B&W Ghidra project, headless.
#
# Usage: pixi run ghidra-script <script-name>
#        (script-name relative to scripts/ghidra/, with or without .py)

param(
    [Parameter(Mandatory=$true)][string]$Script
)

$ErrorActionPreference = "Stop"

$GhidraRoot = "C:\ProgramData\chocolatey\lib\ghidra\tools\ghidra_12.0_PUBLIC"
$Headless   = Join-Path $GhidraRoot "support\analyzeHeadless.bat"
$ProjectDir = "C:\temp\bandw"
$ProjectNm  = "Black and White"
$ProgramNm  = "runblack.exe"

$RepoRoot   = Split-Path -Parent $PSScriptRoot
$ScriptDir  = Join-Path $RepoRoot "scripts\ghidra"

if (-not $Script.EndsWith(".py")) { $Script = "$Script.py" }
$ScriptPath = Join-Path $ScriptDir $Script
if (-not (Test-Path $ScriptPath)) { throw "script not found: $ScriptPath" }

$env:GHIDRA_INSTALL_DIR = $GhidraRoot
Write-Host "Running $Script against $ProgramNm via pyghidra..." -ForegroundColor Cyan
& py -3 -c "
import os, pyghidra
pyghidra.run_script(
    binary_path=None,
    script_path=r'$ScriptPath',
    project_location=r'$ProjectDir',
    project_name=r'$ProjectNm',
    program_name=r'$ProgramNm',
    analyze=False,
    nested_project_location=False,
)
"
