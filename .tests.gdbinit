# For handy x86 breakpoint setting, drop an `__asm__("int3");` in the source
exec-file python3
run ./test_iso14229 -s
