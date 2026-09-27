#!/bin/bash

set -euo pipefail

# symlink to clang-tidy executable in the hermetic llvm toolchain
CLANG_TIDY="${TMPDIR:-/tmp}/clang-tidy.bazel"
# sometimes the symlink exists already but points to an invalid location
if [[ -x "$CLANG_TIDY" ]]; then
    runfiles_dir="$(grep -m1 '^cd ' "$CLANG_TIDY" | sed -e 's/^cd //' -e "s/^['\"]//" -e "s/['\"]$//")"

    # if the location doesn't exist, remove it
    if [[ -z "$runfiles_dir" || ! -d "$runfiles_dir" ]]; then
        rm -f "$CLANG_TIDY"
    fi
fi
# this step takes 1-2 seconds, so only do it if the symlink doesn't exist already
if [[ ! -x "$CLANG_TIDY" ]]; then
    bazel run --script_path="$CLANG_TIDY" @llvm_toolchain//:clang-tidy > /dev/null
fi

bash -c "$CLANG_TIDY --version"

bash -c "$CLANG_TIDY $(realpath iso14229.c) -- -I$(realpath .)"

