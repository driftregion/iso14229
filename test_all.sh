#!/bin/bash

# Run all the tests that will run on the host machine
# Note: qemu must be installed, vcan must be set up
# See test/README.md for more information

bazel test //test:all

##
## QEMU tests
##

# ISO-TP linux sockets fail on qemu-arm with the following error:
# ENOPROTOOPT 92 Protocol not available
# however, socketcan linux sockets work fine.
bazel test --config=arm_linux //test:all --test_tag_filters=-isotp_sock

# It seems socketcan is not supported at all on qemu-ppc
bazel test --config=ppc //test:all --test_tag_filters=-vcan
bazel test --config=ppc64 //test:all --test_tag_filters=-vcan
bazel test --config=ppc64le //test:all --test_tag_filters=-vcan

# Test that the ultra strict build works
bazel build -s --config=x86_64_clang //test:ultra_strict