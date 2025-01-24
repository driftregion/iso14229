package(default_visibility = ["//visibility:public"])

constraint_setting(name = "mcu_series")

constraint_value(
    name = "s32k",
    constraint_setting = ":mcu_series",
)

platform(
    name = "x86_64",
    constraint_values = [
        "@platforms//cpu:x86_64",
        "@platforms//os:linux",
    ],
)

platform(
    name = "x86_64_clang",
    parents = [":x86_64"],
    constraint_values = [
        "//platforms/compiler:clang",
    ],
)

platform(
    name = "x86_64_gcc",
    parents = [":x86_64"],
    constraint_values = [
        "//platforms/compiler:gcc",
    ],
)

platform(
    name = "arm_linux",
    constraint_values = [
        "@platforms//cpu:arm",
        "@platforms//os:linux",
    ],
)

platform(
    name = "arm_none",
    constraint_values = [
        "@platforms//cpu:arm",
        "@platforms//os:none",
    ],
)

platform(
    name = "s32k_evb",
    parents = [":arm_none"],
    constraint_values = [
        ":s32k",
    ],
)

platform(
    name = "ppc",
    constraint_values = [
        "@platforms//cpu:ppc",
        "@platforms//os:linux",
    ],
)

platform(
    name = "ppc64",
    constraint_values = [
        "//platforms/cpu:ppc64",
        "@platforms//os:linux",
    ],
)

platform(
    name = "ppc64le",
    constraint_values = [
        "//platforms/cpu:ppc64le",
        "@platforms//os:linux",
    ],
)