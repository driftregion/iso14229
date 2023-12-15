#!/bin/bash
rm -f release/iso14229.c release/iso14229.h
cp bazel-bin/iso14229.c release/iso14229.c && chmod 644 release/iso14229.c
cp bazel-bin/iso14229.h release/iso14229.h && chmod 644 release/iso14229.h