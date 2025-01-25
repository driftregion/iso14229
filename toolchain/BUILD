load(":gcc_family.bzl", "gcc_toolchain")
load(":clang.bzl", "clang_toolchain")

filegroup(
    name = "empty",
    srcs = [],
)

gcc_toolchain(
    name="arm_linux_gcc",
    prefix="arm-linux-gnueabihf",
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:linux",
    ],
)

gcc_toolchain(
    name="arm_none_gcc",
    prefix="arm-none-eabi",
    target_compatible_with = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
    include_dirs = [
        "/usr/lib/gcc/arm-none-eabi/10.3.1/include/",
    ],
    linkopts = [
        "-Wl,--gc-sections",
    ],
    copts = [
        "-g",
        "-gdwarf-2",
    ]
)

gcc_toolchain(
    name="ppc_gcc",
    prefix="powerpc-linux-gnu", 
    target_compatible_with = [
        "@platforms//cpu:ppc",
        "@platforms//os:linux",
    ]
)

gcc_toolchain(
    name="ppc64_gcc", 
    prefix="powerpc64-linux-gnu", 
    target_compatible_with = [
        "//platforms/cpu:ppc64",
        "@platforms//os:linux",
    ]
)

gcc_toolchain(
    name="ppc64le_gcc",
    prefix="powerpc64le-linux-gnu",
    target_compatible_with = [
        "//platforms/cpu:ppc64le",
        "@platforms//os:linux",
    ]
)

clang_toolchain(name="x86_64_clang")

gcc_toolchain(
    name="x86_64_gcc",
    prefix="",
    include_dirs = [
        "/usr/lib/gcc/x86_64-linux-gnu/11/include/",
        '/usr/lib/gcc/x86_64-linux-gnu/13/include/',
    ],
    target_compatible_with = [
        "@platforms//cpu:x86_64",
        "@platforms//os:linux",
    ],
    linkopts = [
        "-lstdc++",
    ]
)
