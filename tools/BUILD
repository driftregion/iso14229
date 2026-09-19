package(default_visibility = ["//visibility:public"])
load("@rules_python//python:py_binary.bzl", "py_binary")

py_binary(
    name="amalgamate",
    srcs=["amalgamate.py"],
    legacy_create_init = False,
)

py_binary(
    name = "gen_version",
    srcs = ["gen_version.py"],
    legacy_create_init = False,
)
