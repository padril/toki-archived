# Copyright 2023 Leo James Peckham (padril)

"""
Lints `toki.c` according to cppcheck's standards, and in the future, will check
for consistency in variable names.

If running this doesn't work, make sure you have `cppcheck` installed and in
your PATH environment variable
"""


import shlex
import subprocess


def lint():
    """
    Runs lint process
    """
    command = "cppcheck "\
              "--enable=all "\
              "--force "\
              "--suppress=missingIncludeSystem "\
              "toki.c"
    split = shlex.split(command)
    subprocess.run(split, check=True)


if __name__ == "__main__":
    lint()
