
load(":cc_toolchain_config.bzl", "dummy_cc_toolchain_config")

package(default_visibility = ["//visibility:public"])

# The platforms we want to generate GN targets for.

platform(
    name = "linux_x64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:x86_64",
    ],
)

platform(
    name = "linux_aarch64",
    constraint_values = [
        "@platforms//os:linux",
        "@platforms//cpu:aarch64",
    ],
)

# A dummy clang toolchain for building them for any arch.

filegroup(name = "empty")

cc_toolchain(
    name = "dummy_cc_toolchain",
    toolchain_identifier = "dummy-toolchain",
    toolchain_config = ":dummy_toolchain_config",
    all_files = ":empty",
    compiler_files = ":empty",
    dwp_files = ":empty",
    linker_files = ":empty",
    objcopy_files = ":empty",
    strip_files = ":empty",
    supports_param_files = 0,
)

dummy_cc_toolchain_config(
    name = "dummy_toolchain_config",
)

toolchain(
    name = "dummy_toolchain",
    toolchain = ":dummy_cc_toolchain",
    toolchain_type = "@bazel_tools//tools/cpp:toolchain_type",
    exec_compatible_with = [],
    target_compatible_with = [],
)
