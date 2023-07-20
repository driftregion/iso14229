#!/usr/bin/env python

import subprocess

architectures = [
    "CC=arm-linux-gnueabihf-gcc RUN=qemu-arm LDFLAGS=-L/usr/arm-linux-gnueabihf/ CFLAGS=-static",
    "CC=powerpc-linux-gnu-gcc RUN=qemu-ppc LDFLAGS=-L/usr/powerpc-linux-gnu/ CFLAGS=-static",
    "CC=powerpc64-linux-gnu-gcc RUN=qemu-ppc64 LDFLAGS=-L/usr/powerpc64-linux-gnu/ CFLAGS=-static",
]

for arch in architectures:
    subprocess.check_call("make clean".split(" "), shell=True)
    args = f"{arch} make unit_test".split(" ")
    ret = subprocess.call(args, shell=True)
    if ret:
        exit(ret)
