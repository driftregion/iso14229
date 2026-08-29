target remote localhost:3333
set architecture auto
file build/iso14229_server.elf
set mem inaccessible-by-default off
svd_load core/STM32G474.svd

define mreset
    monitor reset halt
    maintenance flush register-cache
end

document mreset
    Reset the MCU via pyOCD and synchronize GDB's register cache.
end
