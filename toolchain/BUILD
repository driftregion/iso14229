load(":gcc_family.bzl", "gcc_toolchain")
load(":clang.bzl", "clang_toolchain")

filegroup(
    name = "empty",
    srcs = [],
)

gcc_toolchain(
    name="arm_gcc",
    prefix="arm-linux-gnueabihf",
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:arm",
    ],
)

gcc_toolchain(
    name="ppc_gcc",
    prefix="powerpc-linux-gnu", 
    target_compatible_with = [
        "@platforms//os:linux",
        "@platforms//cpu:ppc",
    ]
)

gcc_toolchain(
    name="ppc64_gcc", 
    prefix="powerpc64-linux-gnu", 
    target_compatible_with = [
        "@platforms//os:linux",
        "//platforms/cpu:ppc64",
    ]
)

gcc_toolchain(
    name="ppc64le_gcc",
    prefix="powerpc64le-linux-gnu",
    target_compatible_with = [
        "@platforms//os:linux",
        "//platforms/cpu:ppc64le",
    ]
)

clang_toolchain(name="x86_64_clang")