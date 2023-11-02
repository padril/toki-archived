# Copyright 2023 Leo James Peckham (padril)

"""
Tests `toki.c` against test/tests
"""


import argparse
import os
import shlex
import subprocess
from typing import Any


# DEFAULTS

DEFAULT_END_TO_END_PATH = "./test/end_to_end"
DEFAULT_UNIT_PATH = "./test/unit"

TEST_EXTENSION = "_test.toki"
EXPECTED_EXTENSION = "_expected.txt"


def get_flags() -> "dict[str, Any]":
    """
    Returns the flags necessary for program logic, retrieved from command line
    arguments.
    """
    parser = argparse.ArgumentParser(
        prog="toki.c Test Suite",
        description="Performs all tests located in a given folder and gives "\
                    "back the results.")

    parser.add_argument("-a", "--arch",
                        help="The architecture to test",
                        choices=["all", "win"],
                        default="all")
    parser.add_argument("-d", "--default",
                        help="Use default values for `--endtoend` or `--unit`",
                        choices=["all, endtoend, unit, none"],
                        nargs=1,
                        default="none")
    parser.add_argument("-e", "--endtoend",
                        help="Specify the relative path to the file/folder "\
                             "for end-to-end tests.",
                        metavar="PATH",
                        nargs='*')
    parser.add_argument("-m", "--maketoki",
                        help="Make `toki.c` before running tests if true, or "\
                             "uses exisiting `toki.exe` if false",
                        action=argparse.BooleanOptionalAction)
    parser.add_argument("-s", "--showsuccess",
                        help="Displays all test results, even successes.",
                        action=argparse.BooleanOptionalAction)
    parser.add_argument("-u", "--unit",
                        help="Specify the relative path to the file/folder "\
                             "for unit tests.",
                        metavar="PATH",
                        nargs='*')

    args = parser.parse_args().__dict__

    if args["default"] == "all":
        args["endtoend"] = [DEFAULT_END_TO_END_PATH]
        args["unit"] = [DEFAULT_UNIT_PATH]
    elif args["default"] == "endtoend":
        args["endtoend"] = [DEFAULT_END_TO_END_PATH]
    elif args["default"] == "unit":
        args["unit"] = [DEFAULT_UNIT_PATH]

    if args["endtoend"]:
        args["endtoend"] = [os.path.normpath(p) for p in args["endtoend"]]
    if args["unit"]:
        args["unit"] = [os.path.normpath(p) for p in args["unit"]]


    return args


def make_toki():
    if not os.path.exists("./toki.c"):
        raise Exception("Cannot find `toki.c`, try compiling yourself")
    if not os.path.exists("./makefile"):
        raise Exception("Cannot find `makefile`, try compiling yourself")
    
    subprocess.run(shlex.split("make toki"))


def test_end_to_end(path: str, print_success: bool):
    if not os.path.isdir(path):
        raise Exception("End-to-end test input needs to be a directory")
    
    subfiles = os.listdir(path)
    subpaths = [os.path.join(path, f) for f in subfiles]

    if all(os.path.isdir(f) for f in subpaths):
        for f in subpaths:
            test_end_to_end(f, print_success)
        return
    
    folder = os.path.basename(path)
    test_path = os.path.join(path, folder + TEST_EXTENSION)
    expected_path = os.path.join(path, folder + EXPECTED_EXTENSION)

    if len(subfiles) != 2 or not (test_path in subpaths and
                                  expected_path in subpaths):
        raise Exception("Could not find both "
                        f"`{test_path}` and "
                        f"`{expected_path}`")

    # I can't use fstrings cause I need to replace, and I need to replace
    # because subprocess is fucking broken on windows. So this looks ugly.
    subprocess.run(
        shlex.split("./toki.exe %s a" % test_path.replace('\\', '/')))

    result = subprocess.run(shlex.split("./a.exe"),
                   capture_output = True,
                   text = True)
    
    if os.path.exists("./a.asm"):
        os.remove("a.asm")
    if os.path.exists("./a.obj"):
        os.remove("a.obj")
    if os.path.exists("./a.exe"):
        os.remove("a.exe")

    with open(expected_path) as ef:
        expected = ef.read()
        if result.stdout != expected:
            print(f"FAILURE {path}\n"
                  f"  Got:\n"
                  f"{result.stdout}\n"
                  f"  Expected:\n"
                  f"{expected}")
        elif print_success:
            print(f"SUCCESS: {path}")


def main():
    flags = get_flags()

    if flags["maketoki"]:
        make_toki()
        if not os.path.exists("./toki.exe"):
            raise Exception("Running `make toki` failed "
                            "(`toki.exe` not found), try compiling yourself")
    elif not os.path.exists("./toki.exe"):
        raise Exception("Cannot find ./toki.exe, try `--maketoki")
    
    if flags["endtoend"]:
        for p in flags["endtoend"]:
            test_end_to_end(p, flags["showsuccess"])


if __name__ == "__main__":
    main()
