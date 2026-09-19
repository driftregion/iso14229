#!/bin/bash
# formats all non-vendored source files in iso14229.
# 
# To maintain a single source of truth for what constitutes correct formatting, 
# the clang-format executable from the hermetic llvm-toolchain is used.

set -e

CHECK_FORMAT=${CHECK_FORMAT:-""}

files=`find src -type f \( -name '*.c' -o -name '*.h' \) -not -path "src/tp/isotp-c/*"`
files="$files `find test -type f -name '*.c'`"
files="$files `find examples -type f -name '*.c' \
    -not -path '*/build/*' \
    -not -path '*/stm32g474/core/*' \
    -not -path '*/stm32g474/vendor/*'`"

# symlink to clang-format executable in the hermetic llvm toolchain
CLANG_FORMAT="${TMPDIR:-/tmp}/clang-format.bazel"
# sometimes the symlink exists already but points to an invalid location
if [[ -x "$CLANG_FORMAT" ]]; then
    runfiles_dir="$(grep -m1 '^cd ' "$CLANG_FORMAT" | sed -e 's/^cd //' -e "s/^['\"]//" -e "s/['\"]$//")"

    # if the location doesn't exist, remove it
    if [[ -z "$runfiles_dir" || ! -d "$runfiles_dir" ]]; then
        rm -f "$CLANG_FORMAT"
    fi
fi
# this step takes 1-2 seconds, so only do it if the symlink doesn't exist already
if [[ ! -x "$CLANG_FORMAT" ]]; then
    bazel run --script_path="$CLANG_FORMAT" @llvm_toolchain//:clang-format > /dev/null
fi


bash -c "$CLANG_FORMAT --version"

for file in $files ; do
    full_path="$(realpath $file)"
    if [ -z "$CHECK_FORMAT" ] ; then
        bash -c "$CLANG_FORMAT -i $full_path"
    else
        bash -c "$CLANG_FORMAT -Werror --dry-run $full_path"
    fi
done
