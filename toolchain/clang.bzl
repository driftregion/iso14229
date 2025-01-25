load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "feature",
    "flag_group",
    "flag_set",
    "tool_path",
)

all_link_actions = [ 
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

def _impl(ctx):
    CLANG = "clang-15"
    tool_paths = [
        tool_path(
            name = "gcc",
            path = "/usr/bin/" + CLANG,
        ),
        tool_path(
            name = "ld",
            path = "/usr/bin/ld",
        ),
        tool_path(
            name = "ar",
            path = "/usr/bin/ar",
        ),
        tool_path(
            name = "cpp",
            path = "/bin/false",
        ),
        tool_path(
            name = "gcov",
            path = "/usr/bin/gcov",
        ),
        tool_path(
            name = "nm",
            path = "/bin/false",
        ),
        tool_path(
            name = "objdump",
            path = "/bin/false",
        ),
        tool_path(
            name = "strip",
            path = "/bin/false",
        ),
    ]

    features = [
        feature(
            name = "default_linker_flags",
            enabled = True,
            flag_sets = [
                flag_set(
                    actions = all_link_actions,
                    flag_groups = ([
                        flag_group(
                            flags = [
                                "-stdlib=libstdc++",
                            ],
                        ),
                    ]),
                ),
            ],
        ),
        # feature(
        #     name = "default_compiler_flags",
        #     enabled = True,
        #     flag_sets = [
        #         flag_set(
        #             actions = [ACTION_NAMES.cpp_compile],
        #             flag_groups = ([
        #                 flag_group(
        #                     flags = [
        #                         "-stdlib=libstdc++",
        #                     ],
        #                 ),
        #             ]),
        #         ),
        #     ],
        # ),
    ]

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        features = features, 
        cxx_builtin_include_directories = [
            "/usr/lib/llvm-15/lib/clang/15.0.7/include",
            "/usr/lib/llvm-15/lib/clang/15.0.7/share",
            "/usr/include",
            "/usr/lib/x86_64-linux-gnu/",
        ],
        toolchain_identifier = "local",
        host_system_name = "local",
        target_system_name = "local",
        target_cpu = "k8",
        target_libc = "unknown",
        compiler = CLANG,
        abi_version = "unknown",
        abi_libc_version = "unknown",
        tool_paths = tool_paths,
    )

clang_toolchain_config = rule(
    implementation = _impl,
    provides = [CcToolchainConfigInfo],
)

def clang_toolchain(name):
    CONFIG_NAME = name + "_config"
    CC_TOOLCHAIN_NAME = name + "_cc_toolchain"

    clang_toolchain_config(
        name = CONFIG_NAME,
    )

    native.cc_toolchain(
        name = CC_TOOLCHAIN_NAME,
        toolchain_identifier = "k8-toolchain",
        toolchain_config = CONFIG_NAME,
        all_files = ":empty",
        compiler_files = ":empty",
        dwp_files = ":empty",
        linker_files = ":empty",
        objcopy_files = ":empty",
        strip_files = ":empty",
        supports_param_files = 0,
    )

    native.toolchain(
        name = name,
        exec_compatible_with = [
            "@platforms//os:linux",
            "@platforms//cpu:x86_64",
        ],
        target_compatible_with = [
            "@platforms//os:linux",
            "@platforms//cpu:x86_64",
        ],
        toolchain = CC_TOOLCHAIN_NAME,
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    )
