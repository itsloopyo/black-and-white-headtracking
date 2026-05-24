"""Open the existing Black and White Ghidra project and run a script against
the runblack.exe program. Avoids pyghidra's deprecated run_script CLI which
imports the binary fresh without our pre-analyzed symbols.

Usage: py run_via_pyghidra.py <script.py>
"""
import os
import sys
from pathlib import Path

os.environ.setdefault(
    "GHIDRA_INSTALL_DIR",
    r"C:\ProgramData\chocolatey\lib\ghidra\tools\ghidra_12.0_PUBLIC",
)

import pyghidra
pyghidra.start()

from ghidra.base.project import GhidraProject
from java.io import File

project_dir = r"c:\temp\bandw"
project_name = "Black and White"
program_path = "/runblack.exe"

script_path = Path(sys.argv[1]).resolve()
print("[wrapper] Opening project {} / {}".format(project_dir, project_name))

project = GhidraProject.openProject(project_dir, project_name, True)
try:
    program = project.openProgram("/", "runblack.exe", True)
    print("[wrapper] Program loaded: {}".format(program.getName()))

    from pyghidra.script import PyGhidraScript
    from ghidra.app.script import GhidraState
    from ghidra.program.util import ProgramLocation
    from ghidra.util.task import ConsoleTaskMonitor

    state = GhidraState(
        None,  # tool
        project.getProject(),
        program,
        ProgramLocation(program, program.getMinAddress()),
        None,  # selection
        None,  # highlight
    )

    # Exec the script body with currentProgram et al bound.
    g = {
        "currentProgram": program,
        "currentAddress": program.getMinAddress(),
        "monitor": ConsoleTaskMonitor(),
        "__name__": "__main__",
    }
    code = compile(script_path.read_text(), str(script_path), "exec")
    exec(code, g)
finally:
    project.close()
