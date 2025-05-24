load(
    "@bazel_tools//tools/cpp:cc_toolchain_config_lib.bzl",
    "tool_path",
)

def _impl(ctx):
    return cc_common.create_cc_toolchain_config_info(
        ctx = ctx,
        features = [],
        cxx_builtin_include_directories = [],
        toolchain_identifier = "dummy",
        host_system_name = "dummy",
        target_system_name = "dummy",
        target_cpu = "dummy",
        target_libc = "dummy",
        # Changing this to clang / gcc will generate more specific flags.
        compiler = "dummy",
        abi_version = "dummy",
        abi_libc_version = "dummy",
        tool_paths = [
            tool_path(
                name = "gcc",
                path = "/bin/false",
            ),
            tool_path(
                name = "ld",
                path = "/bin/false",
            ),
            tool_path(
                name = "ar",
                path = "/bin/false",
            ),
            tool_path(
                name = "cpp",
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
        ],
    )

dummy_cc_toolchain_config = rule(
    implementation = _impl,
    attrs = {},
    provides = [CcToolchainConfigInfo],
)
