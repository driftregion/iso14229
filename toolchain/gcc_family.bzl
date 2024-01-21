""" gcc toolchains """

load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "tool_path",
    "flag_group",
    "flag_set",
    "feature",
)

all_link_actions = [ 
    ACTION_NAMES.cpp_link_executable,
    ACTION_NAMES.cpp_link_dynamic_library,
    ACTION_NAMES.cpp_link_nodeps_dynamic_library,
]

def _impl(ctx):
    tool_paths = [
        tool_path(
            name = "gcc",
            path = "/usr/bin/{}-gcc".format(ctx.attr.prefix),
        ),
        tool_path(
            name = "ld",
            path = "/usr/bin/{}-ld".format(ctx.attr.prefix),
        ),
        tool_path(
            name = "ar",
            path = "/usr/bin/{}-ar".format(ctx.attr.prefix),
        ),
        tool_path(
            name = "cpp",
            path = "/bin/false",
        ),
        tool_path(
            name = "gcov",
            path = "/bin/false",
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
        tool_path(
            name = "objcopy",
            path = "/usr/bin/{}-objcopy".format(ctx.attr.prefix),
        ),
    ]

    linker_flags = ctx.attr.linkopts + ctx.attr.arch_flags
    compiler_flags = ctx.attr.copts + ctx.attr.arch_flags

    features = []

    if len(linker_flags) > 0:
        features.append(
            feature(
                name = "linker_flags",
                enabled = True,
                flag_sets = [
                    flag_set(
                        actions = all_link_actions,
                        flag_groups = [
                            flag_group(
                                flags = linker_flags,
                            ),
                        ],
                    ),
                ],
            )
        )
    
    if len(compiler_flags) > 0:
        features.append(
            feature(
                name = "compiler_flags",
                enabled = True,
                flag_sets = [
                    flag_set(
                        actions = [
                            ACTION_NAMES.c_compile,
                            ACTION_NAMES.cpp_compile,
                        ],
                        flag_groups = [
                            flag_group(
                                flags = compiler_flags,
                            ),
                        ],
                    ),
                ],
            )
        )


    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        features = features, 
        cxx_builtin_include_directories = [
            "/usr/{}/include".format(ctx.attr.prefix),
            "/usr/lib/gcc-cross/{}/11/include".format(ctx.attr.prefix),
            "/usr/include",
        ] + ctx.attr.include_dirs,
        toolchain_identifier = "local",
        host_system_name = "local",
        target_system_name = "local",
        target_cpu = "k8",
        target_libc = "unknown",
        compiler = ctx.attr.prefix + "gcc",
        abi_version = "unknown",
        abi_libc_version = "unknown",
        tool_paths = tool_paths,
    )

gcc_toolchain_config = rule(
    implementation = _impl,
    attrs = {
        "prefix": attr.string(mandatory=True, doc="Prefix of compiler suite"),
        "include_dirs": attr.string_list(mandatory=False, doc="List of include dirs"),
        "arch_flags": attr.string_list(mandatory=False, doc="List of arch flags"),
        "linkopts": attr.string_list(mandatory=False, doc="List of additional linker options"),
        "copts": attr.string_list(mandatory=False, doc="List of additional compiler options"),
    },
    provides = [CcToolchainConfigInfo],
)

def gcc_toolchain(name, prefix, target_compatible_with, include_dirs= [], linkopts = [], copts = []):
    """Create a gcc toolchain

    Args:
        name: Name of the toolchain
        prefix: Prefix of the compiler suite
        target_compatible_with: List of targets that this toolchain is compatible with
        include_dirs: List of additional include directories
        linkopts: List of additional linker options
        copts: List of additional compiler options
    """
    CONFIG_NAME = name + "_config"
    CC_TOOLCHAIN_NAME = name + "_cc_toolchain"

    gcc_toolchain_config(
        name = CONFIG_NAME,
        prefix = prefix,
        include_dirs = include_dirs,
        arch_flags = select({
            "//platforms:s32k" : [
                "-mcpu=cortex-m4",
                "-mthumb",
                "-mfloat-abi=hard",
                "-mfpu=fpv4-sp-d16",
            ],
            "//conditions:default" : [
            ],
        }),
        linkopts = linkopts,
        copts = copts,
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
        target_compatible_with = target_compatible_with,
        toolchain = CC_TOOLCHAIN_NAME,
        toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    )