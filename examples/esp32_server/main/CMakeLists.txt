idf_component_register(
    SRCS "main.c"
    "iso14229.c"
    "iso14229.h"
    INCLUDE_DIRS "."
)

# workaround for https://github.com/SimonCahill/isotp-c/issues/83
set_source_files_properties(
    "iso14229.c"
    PROPERTIES COMPILE_FLAGS
    -Wno-error=stringop-overflow
)
