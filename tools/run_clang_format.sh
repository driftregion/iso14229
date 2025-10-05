#!/bin/bash
set -e

CHECK_FORMAT=${CHECK_FORMAT:-""}

files=`find src -type f \( -name '*.c' -o -name '*.h' \) -not -path "src/tp/isotp-c/*"`
files="$files `find test -type f -name '*.c'`"
files="$files `find examples -type f -name '*.c' -not -path '*/build/*'`"


if [ -n "$CLANG_FORMAT" ] ; then
    echo "Using CLANG_FORMAT from environment: $CLANG_FORMAT"
    $CLANG_FORMAT --version
else
    CLANG_FORMAT="${TMPDIR:-/tmp}/clang-format.bazel"
    if [[ ! -x "$CLANG_FORMAT" ]]; then
        bazel run --script_path="$CLANG_FORMAT" @llvm_toolchain//:clang-format > /dev/null
    fi
    bash -c "$CLANG_FORMAT --version"
fi

for file in $files ; do
    full_path="$(realpath $file)"
    if [ -z "$CHECK_FORMAT" ] ; then
        bash -c "$CLANG_FORMAT -i $full_path"
    else
        bash -c "$CLANG_FORMAT -Werror --dry-run $full_path"
    fi
done
