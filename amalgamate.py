import os
import re
import argparse
import glob

out_dir = os.getenv("BUILD_WORKSPACE_DIRECTORY", os.getcwd())
iso14229_h = os.path.join(out_dir, "iso14229.h")
iso14229_c = os.path.join(out_dir, "iso14229.c")

parser = argparse.ArgumentParser()
parser.add_argument("--out_c", help="output c file", default=iso14229_c)
parser.add_argument("--out_h", help="output h file", default=iso14229_h)
parser.add_argument("srcs", nargs="*")
args = parser.parse_args()
srcs = {os.path.basename(src): src for src in args.srcs}


def strip_includes(src):
    return re.sub(r'#include ".*', "", src, flags=re.MULTILINE)

isotp_c_wrapped_c = "#if defined(UDS_ISOTP_C)\n" + \
    strip_includes(open("src/tp/isotp-c/isotp.c", encoding="utf-8").read()) + \
    "#endif\n"

isotp_c_wrapped_h = "#if defined(UDS_ISOTP_C)\n" + \
    "\n".join([strip_includes(open("src/tp/isotp-c/" + h).read()) for h in [
        "isotp_config.h",
        "isotp_defines.h",
        "isotp.h",
    ]]) + \
    "#endif\n"

with open(args.out_c, "w", encoding="utf-8") as f:
    f.write("#include \"iso14229.h\"\n")
    for src in [
        "src/client.c",
        "src/server.c",
        "src/tp.c",
        "src/util.c",
        "src/tp/isotp_c.c",
        "src/tp/isotp_c_socketcan.c",
        "src/tp/isotp_sock.c",
        "src/tp/mock.c",
    ]:
        f.write("\n")
        f.write("#ifdef UDS_LINES\n")
        f.write(f'#line 1 "{src}"' + "\n")
        f.write("#endif\n")
        with open(src, "r", encoding="utf-8") as src_file:
            stripped = strip_includes(src_file.read())
            f.write(stripped)
            f.write("\n")

    f.write(isotp_c_wrapped_c)
    f.write("\n")


with open(args.out_h, "w") as f:
    f.write("#ifndef ISO14229_H\n")
    f.write("#define ISO14229_H\n")
    f.write("\n")
    f.write("#ifdef __cplusplus\n")
    f.write("extern \"C\" {\n")
    f.write("#endif\n")
    f.write("\n")
    for src in [
        "src/version.h",
        "src/sys.h",
        "src/sys_arduino.h",
        "src/sys_unix.h",
        "src/sys_win32.h",
        "src/sys_esp32.h",
        "src/config.h",
        "src/util.h",
        "src/tp.h",
        "src/uds.h",
        "src/client.h",
        "src/server.h",
    ]:
        f.write("\n")
        with open(src, "r", encoding="utf-8") as src_file:
            stripped = strip_includes(src_file.read())
            f.write(stripped)
            f.write("\n")

    f.write(isotp_c_wrapped_h)


    for src in [
        "src/tp/isotp_c.h",
        "src/tp/isotp_c_socketcan.h",
        "src/tp/isotp_sock.h",
        "src/tp/mock.h",
    ]:
        f.write("\n")
        with open(src) as src_file:
            f.write(strip_includes(src_file.read()))
            f.write("\n")

    f.write("\n")
    f.write("#ifdef __cplusplus\n")
    f.write("}\n")
    f.write("#endif\n")
    f.write("\n")
    f.write("#endif\n")

# os.chmod(iso14229_h, 0o444)
# os.chmod(iso14229_c, 0o444)

if __name__ == "__main__":
    print(f"amalgamated source files written to {args.out_c} and {args.out_h}")
