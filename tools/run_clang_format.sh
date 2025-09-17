#!/bin/bash
set -e

CHECK_FORMAT=${CHECK_FORMAT:-""}

files=`find src -type f \( -name '*.c' -o -name '*.h' \) -not -path "src/tp/isotp-c/*"`

# Create a stable launcher to the hermetic clang-format.
CLANG_FORMAT="${TMPDIR:-/tmp}/clang-format.bazel"
if [[ ! -x "$CLANG_FORMAT" ]]; then
  bazel run --script_path="$CLANG_FORMAT" @llvm_toolchain//:clang-format > /dev/null
fi

for file in $files ; do
    full_path="$(realpath $file)"
    if [ -z "$CHECK_FORMAT" ] ; then
        bash -c "$CLANG_FORMAT -i $full_path"
    else
        bash -c "$CLANG_FORMAT -Werror --dry-run $full_path"
    fi
done