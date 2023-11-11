package(default_visibility = ["//visibility:public"])

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
    name = "arm",
    constraint_values = [
        "@platforms//cpu:arm",
        "@platforms//os:linux",
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