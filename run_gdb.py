#!/usr/bin/env python3

__doc__ = """
Automates bazel, gdb, and optionally qemu to accelerate debugging

the `bazel test` command explicitly prohibits interactive debugging, 
so this script exists to automate the debugging process.
"""

import subprocess
import argparse

GDB = "gdb-multiarch"
QEMU = "qemu-arm"
QEMU_LDFLAGS = "/usr/arm-linux-gnueabihf/"
GDB_PORT = 1234

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("test", help="The test to debug")
parser.add_argument("--config", help="bazel config", default="")
args = parser.parse_args()
gdb_args = [GDB, "-q", "-ex", f"file bazel-bin/test/{args.test}"]

print(f"building {args.test}...")
bazel_args = ["bazel", "build", "-c", "dbg", "--copt=-g", f"//test:{args.test}"]

subprocess.check_call(bazel_args)
procs = []


if args.config:
    qemu = subprocess.Popen(
        [QEMU, "-g", str(GDB_PORT), 
        "-L", "/usr/arm-linux-gnueabihf/", 
        f"bazel-bin/test/{args.test}"],
    )

    if qemu.returncode is not None:
        exit(qemu.returncode)

    procs.append(qemu)
    gdb_args += ["-ex", f"target remote localhost:{GDB_PORT}"]


try:
    gdb = subprocess.Popen(gdb_args)
    procs.append(gdb)
    gdb.wait()
except KeyboardInterrupt:
    pass
finally:
    [p.kill() for p in procs]
    exit(gdb.returncode)
