"""A rule wrapper for an instrumentation test for an android library."""

load(
    "//tensorflow_lite_support/tools/build_rules/android_test:generate_instrumentation_tests.bzl",
    "generate_instrumentation_tests",
)
load(
    "//tensorflow_lite_support/tools/build_rules/android_test:infer_java_package_name.bzl",
    "infer_java_package_name",
)

def android_library_instrumentation_tests(
        name,
        srcs,
        deps,
        device_list,
        test_java_package = None,
        library_args = {},
        binary_args = {},
        **kwargs):
    """A macro for an instrumentation test whose target under test is an android_library.

    Will generate a 'self-instrumentating' test binary and other associated rules

    The intent of this wrapper is to simplify the build API for creating instrumentation test rules
    for simple cases, while still supporting build_cleaner for automatic dependency management.

    This will generate:
      - an unused stub android_binary under test, to placate bazel
      - a test_lib android_library, containing all sources and dependencies
      - a test_binary android_binary (soon to be android_application)
      - the manifest to use for the test library.
      - for each device combination:
         - an android_instrumentation_test rule)

    Args:
      name: the name to use for the generated android_library rule. This is needed for build_cleaner to
        manage dependencies
      srcs: the test sources to generate rules for
      deps: the build dependencies to use for the generated test binary
      device_list: array of device targets to execute on
      test_java_package: Optional. A custom root package name to use for the tests. If unset
          will be derived based on current path to a java source root
      library_args: additional arguments to pass to generated android_library
      binary_args: additional arguments to pass to generated android_binary
      **kwargs: arguments to pass to generated android_instrumentation_test rules
    """
    library_name = "%s_library" % name
    test_java_package_name = test_java_package if test_java_package else infer_java_package_name()

    native.android_binary(
        name = "target_stub_binary",
        manifest = "//tensorflow_lite_support/tools/build_rules/android_test:AndroidManifest_target_stub.xml",
        # use the same package name as the test package, so it gets overridden
        manifest_values = {"applicationId": test_java_package_name},
        testonly = 1,
    )

    native.android_library(
        name = library_name,
        srcs = srcs,
        testonly = 1,
        deps = deps,
        **library_args
    )

    generate_instrumentation_tests(
        name = name,
        srcs = srcs,
        deps = [library_name],
        target_devices = device_list,
        test_java_package_name = test_java_package_name,
        test_android_package_name = test_java_package_name,
        instrumentation_target_package = test_java_package_name,
        instruments = ":target_stub_binary",
        binary_args = binary_args,
        **kwargs
    )
