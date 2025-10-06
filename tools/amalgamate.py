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
    src = re.sub(r'#include ".*\n', "", src)
    src = re.sub(r'#pragma once\n', "", src)
    return src

isotp_c_wrapped_c = "#if defined(UDS_TP_ISOTP_C)\n" + \
    "#ifndef ISO_TP_USER_SEND_CAN_ARG\n" + \
    '#error\n' + \
    "#endif\n" + \
    strip_includes(open("src/tp/isotp-c/isotp.c").read()) + \
    "#endif\n"

isotp_c_wrapped_h = "#if defined(UDS_TP_ISOTP_C)\n" + \
    "#define ISO_TP_USER_SEND_CAN_ARG 1\n" + \
    "\n".join([strip_includes(open("src/tp/isotp-c/" + h).read()) for h in [
        "isotp_config.h",
        "isotp_defines.h",
        "isotp_user.h",
        "isotp.h",
    ]]) + \
    "#endif\n"

with open(args.out_c, "w", encoding="utf-8") as f:
    f.write("/**\n")
    f.write(" * @file iso14229.c\n")
    f.write(" * @brief ISO14229-1 (UDS) library\n")
    f.write(" * @copyright Copyright (c) Nick Kirkby\n")
    f.write(" * @see https://github.com/driftregion/iso14229\n")
    f.write(" */\n")
    f.write("\n")
    f.write("#include \"iso14229.h\"\n")
    for src in [
        "src/client.c",
        "src/server.c",
        "src/tp.c",
        "src/util.c",
        "src/log.c",
        "src/tp/isotp_c.c",
        "src/tp/isotp_c_socketcan.c",
        "src/tp/isotp_sock.c",
        "src/tp/isotp_mock.c",
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


with open(args.out_h, "w", encoding="utf-8") as f:
    f.write("#ifndef ISO14229_H\n")
    f.write("#define ISO14229_H\n")
    f.write("\n")
    f.write("/**\n")
    f.write(" * @file iso14229.h\n")
    f.write(" * @brief ISO14229-1 (UDS) library\n")
    f.write(" * @copyright Copyright (c) Nick Kirkby\n")
    f.write(" * @see https://github.com/driftregion/iso14229\n")
    f.write(" */\n")
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
        "src/tp.h",
        "src/uds.h",
        "src/util.h",
        "src/log.h",
        "src/client.h",
        "src/server.h",
    ]:
        f.write("\n")
        src_path = next((s for s in args.srcs if src in s))
        with open(src_path, "r", encoding="utf-8") as src_file:
            stripped = strip_includes(src_file.read())
            f.write(stripped)
            f.write("\n")

    f.write(isotp_c_wrapped_h)


    for src in [
        "src/tp/isotp_c.h",
        "src/tp/isotp_c_socketcan.h",
        "src/tp/isotp_sock.h",
        "src/tp/isotp_mock.h",
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
