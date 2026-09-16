@echo off
:: ============================================
:: Black ^& White Head Tracking - Uninstall
:: ============================================
:: Thin wrapper - uninstall body lives in cameraunlock-core/scripts/uninstall-body.cmd,
:: staged into the release ZIP's shared/ by Copy-SharedBundle. To change
:: uninstall behaviour edit the body, not this wrapper. Everything below the
:: CONFIG BLOCK is copied verbatim from
:: cameraunlock-core/scripts/templates/uninstall-wrapper.cmd.
:: ============================================

:: --- CONFIG BLOCK ---
set "GAME_ID=black-and-white"
set "MOD_DISPLAY_NAME=Black ^& White Head Tracking"
set "MOD_DLLS=HeadTracking.dll bw-headtracking-launcher.exe HeadTracking.ini HeadTracking.log HeadTracking.prev.log"
set "MOD_INTERNAL_NAME=BlackAndWhiteHeadTracking"
set "STATE_FILE=.headtracking-state.json"
set "FRAMEWORK_TYPE=None"
set "SHIM_MARKER=black and white head tracking"
set "SHIM_MARKER_ALT=runblack.exe not found next to launcher:"
set "LEGACY_DLLS=HeadTracking_debug.log HeadTracking_debug.prev.log HeadTracking_debug.previous.log"
set "PLUGIN_SUBFOLDER="
set "MANAGED_SUBFOLDER="
set "ASSEMBLY_DLL="
set "PATCH_MARKER="
set "MANAGED_EXTRAS="
set "ASI_LOADER_NAME=winmm.dll"
:: --- END CONFIG BLOCK ---

:: Pin delayed expansion off before `%*` is expanded on the `call` below.
:: Under `cmd /V:ON`, or with DelayedExpansion=1 in
:: HKCU\Software\Microsoft\Command Processor, cmd.exe eats a `!` out of the
:: expanded line, and a real game path like C:\Games\Oh! My Game reaches the
:: body already mangled. The body pins expansion off at its own outer scope
:: too, but that is one `call` too late to save the argument it was handed.
setlocal disabledelayedexpansion

set "WRAPPER_DIR=%~dp0"
set "_BODY=%WRAPPER_DIR%shared\uninstall-body.cmd"
if not exist "%_BODY%" set "_BODY=%WRAPPER_DIR%..\cameraunlock-core\scripts\uninstall-body.cmd"
if not exist "%_BODY%" (
    echo ERROR: uninstall-body.cmd not found in shared\ or ..\cameraunlock-core\scripts\.
    echo If this is a release ZIP, re-download it from GitHub ^(corrupt installer^).
    echo If this is the dev tree, run: git submodule update --init --recursive
    exit /b 1
)
call "%_BODY%" %*
exit /b %errorlevel%
