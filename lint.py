# Copyright 2023 Leo James Peckham (padril)

# Lints `toki.c`

import shlex, subprocess

# If running this doesn't work, make sure you have `cppcheck` installed
# and in your PATH environment variable

if __name__ == "__main__":
    command = "cppcheck "\
              "--enable=all "\
              "--force "\
              "--suppress=missingIncludeSystem "\
              "toki.c"
    split = shlex.split(command)
    subprocess.run(split)