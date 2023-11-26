""" objcopy rule """
load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain")

def _objcopy_impl(ctx):
    cc_toolchain = find_cc_toolchain(ctx)
    outfile = ctx.outputs.out

    ctx.actions.run_shell(
        outputs = [outfile],
        inputs = depset(
            direct = [ctx.file.src],
            transitive = [
                cc_toolchain.all_files,
            ],
        ),
        command = "{objcopy} {args} {src} {dst}".format(
            objcopy = cc_toolchain.objcopy_executable,
            args = ctx.attr.args,
            src = ctx.file.src.path,
            dst = ctx.outputs.out.path,
        ),
    )

    return [
        DefaultInfo(
            files = depset([outfile]),
        ),
    ]

_objcopy = rule(
    implementation = _objcopy_impl,
    attrs = {
        # Source file
        "src": attr.label(allow_single_file = True, mandatory = True),
        # Target file
        "out": attr.output(mandatory = True),
        # Arguments
        "args": attr.string(mandatory = True),
    },
    executable = False,
    toolchains = ["@bazel_tools//tools/cpp:toolchain_type"],
    fragments = ["cpp"],
)

def objcopy(src, out, args, name = ""):
    """
    objcopy rule

    src: source file
    out: target file
    args: arguments
    name: rule name
    """
    _objcopy(
        name = name or "objcopy" + out,
        src = src,
        out = out,
        args = args,
    )