load("@bazel_tools//tools/build_defs/cc:action_names.bzl", "ACTION_NAMES")
load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "tool_path",
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
    ]

    features = [
    ]

    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        features = features, 
        cxx_builtin_include_directories = [
            "/usr/{}/include".format(ctx.attr.prefix),
            "/usr/lib/gcc-cross/{}/11/include".format(ctx.attr.prefix),
            "/usr/include",
        ],
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
    },
    provides = [CcToolchainConfigInfo],
)

def gcc_toolchain(name, prefix, target_compatible_with):
    """Create a gcc toolchain

    Args:
        name: Name of the toolchain
        prefix: Prefix of the compiler suite
        target_compatible_with: List of targets that this toolchain is compatible with
    """
    CONFIG_NAME = name + "_config"
    CC_TOOLCHAIN_NAME = name + "_cc_toolchain"

    gcc_toolchain_config(
        name = CONFIG_NAME,
        prefix = prefix,
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