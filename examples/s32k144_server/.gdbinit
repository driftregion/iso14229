target remote localhost:3333
set architecture armv7e-m
# file bazel-bin/examples/s32k144/main

define dumpflash
    dump binary memory dump.bin 0x0 0x80000
end