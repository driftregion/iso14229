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


def transform(filename):
    """ makes the source file suitable for amalgamation by stripping includes and inserting 
    preprocessor directives. 
    """
    with open(filename, "r", encoding='utf-8') as f:
        buf = f.read()
        buf = re.sub(r'#include ".*\n', "\n", buf)
        buf = re.sub(r'#pragma once\n', "\n", buf)

    # this hack is needed to make the #line directive resolve correctly
    first_line, buf = buf.split("\n", 1)
    buf = first_line + f"""
#ifdef UDS_LINES
#line 1 "{filename}"
#endif
""" + buf

    return buf


with open(args.out_c, "w", encoding="utf-8") as f:
    f.write("""/**
 * @file iso14229.c
 * @brief ISO14229-1 (UDS) library
 * @copyright Copyright (c) Nick Kirkby
 * @see https://github.com/driftregion/iso14229
 */

#include "iso14229.h"
""")
    for src in [
        "src/util_private.h",
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
        f.write(transform(src))
        f.write("\n")

    f.write("""#if defined(UDS_TP_ISOTP_C)
/// \cond DOXYGEN_SHOULD_SKIP_THIS

#ifndef ISO_TP_USER_SEND_CAN_ARG
#error "need this"
#endif

#ifndef ISO_TP_NO_FORMATTED_ERRORS
#error "need this too"
#endif

""" + \
transform("src/tp/isotp-c/isotp.c") + \
"""
/// \endcond
#endif // if defined(UDS_TP_ISOTP_C)

""")


with open(args.out_h, "w", encoding="utf-8") as f:
    f.write("""#ifndef ISO14229_H
#define ISO14229_H

/**
 * @file iso14229.h
 * @brief ISO14229-1 (UDS) library
 * @copyright Copyright (c) Nick Kirkby
 * @see https://github.com/driftregion/iso14229
 */

#ifdef __cplusplus
extern "C" {
#endif

""")
    for src in [
        "src/version.h",
        "src/sys.h",
        "src/config.h",
        "src/uds.h",
        "src/tp.h",
        "src/util.h",
        "src/log.h",
        "src/client.h",
        "src/server.h",
    ]:
        f.write(transform(src))
        f.write("\n")

    f.write("""#if defined(UDS_TP_ISOTP_C)
/// \cond DOXYGEN_SHOULD_SKIP_THIS

#define ISO_TP_USER_SEND_CAN_ARG 1
#define ISO_TP_NO_FORMATTED_ERRORS 1

""" + "\n".join(
    [
        transform(f"src/tp/isotp-c/{h}") for h in [
            "isotp_config.h",
            "isotp_defines.h",
            "isotp_user.h",
            "isotp.h",
        ]
    ]) + \
"""
/// \endcond
#endif // if defined(UDS_TP_ISOTP_C)
""")

    for src in [
        "src/tp/isotp_c.h",
        "src/tp/isotp_c_socketcan.h",
        "src/tp/isotp_sock.h",
        "src/tp/isotp_mock.h",
    ]:
        f.write(transform(src))
        f.write("\n")

    f.write("""
#ifdef __cplusplus
}
#endif

#endif
""")

# os.chmod(iso14229_h, 0o444)
# os.chmod(iso14229_c, 0o444)

if __name__ == "__main__":
    print(f"amalgamated source files written to {args.out_c} and {args.out_h}")
