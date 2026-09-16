#!/bin/bash

# test that /iso14229.c/.h matches the generated version in src/iso14229.c/.h
# This test does not run under bazel 

bazel build //src:iso14229.c //src:iso14229.h 
BAZEL_BIN="$(bazel info bazel-bin)"
diff $BAZEL_BIN/src/iso14229.c iso14229.c
if [ $? != 0 ]; then
    echo "fail: iso14229.c mismatch. run make update_srcs"
    exit 1
fi
diff $BAZEL_BIN/src/iso14229.h iso14229.h
if [ $? != 0 ]; then
    echo "fail: iso14229.h mismatch. run make update_srcs"
    exit 1
fi

exit 0
