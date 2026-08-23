# Ghidra analysis scripts

Ad-hoc Jython probes used while working out how Black & White's camera, cursor,
picking, and render passes fit together, so that this mod can hook them without
disturbing game logic.

## What these are

Each script drives Ghidra's API against a local Ghidra project containing
`runblack.exe` **from the developer's own legally purchased copy of the game**.
They ask questions of that database - which functions write a given global, what
a call graph looks like, which matrix a projection helper reads - and print the
answers to the console.

## What is and is not in this repository

- **No game binary.** `runblack.exe` is never committed, downloaded, or shipped.
  The scripts fail without a Ghidra project you built yourself from your own
  installed copy.
- **No Ghidra project.** The `.gpr` / `.rep` database is not committed either.
- **No decompiled or disassembled game code.** Nothing here reproduces the
  game's code. The scripts *generate* decompiler output at run time, on the
  analyst's machine, and that output is never committed. What is committed is
  memory addresses, symbol names we invented, and prose describing observed
  behaviour - facts about the program, not its expression.
- **No circumvention.** Nothing here touches, weakens, or works around SafeDisc
  or any other copy protection, licence check, or DRM.

## Why they exist

Black & White shipped in 2001 with no modding API. Establishing the interfaces
this mod hooks - which addresses hold the view matrix, which function builds the
HUD camera, which pass reads the cursor - is only possible by examining the
running program. This is examination for interoperability, and only for
interoperability: the result is `src/engine_addresses.h` and the hooks in
`src/camera_hook.cpp`, all original code.

## Running them

They expect a Ghidra project you created and analysed yourself:

    pixi run ghidra-script <script-name>

Paths at the top of `../ghidra-headless.ps1` and `run_via_pyghidra.py` point at
one developer's local install and project location. Edit them to match yours.
