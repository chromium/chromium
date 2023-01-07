"""Internal helper function for generating instrumentation tests ."""

load(
    "//tensorflow_lite_support/tools/build_rules/android_test:android_multidevice_instrumentation_test.bzl",
    "android_multidevice_instrumentation_test",
)

def generate_instrumentation_tests(
        name,
        srcs,
        deps,
        target_devices,
        test_java_package_name,
        test_android_package_name,
        instrumentation_target_package,
        instruments,
        binary_args = {},
        **kwargs):
    """A helper rule to generate instrumentation tests.


    This will generate:
      - a test_binary android_binary (soon to be android_application)
      - the manifest to use for the test library.
      - for each device combination:
         - an android_instrumentation_test rule)

    Args:
      name: unique prefix to use for generated rules
      srcs: the test sources to generate rules for
      deps: the build dependencies to use for the generated test binary
      target_devices: array of device targets to execute on
      test_java_package_name: the root java package name for the tests.
      test_android_package_name: the android package name to use for the android_binary test app. Typically this is the same as test_java_package_name
      instrumentation_target_package: the android package name to specify as instrumentationTargetPackage in the test_app manifest
      instruments: The android binary the tests instrument.
      binary_args: Optional additional arguments to pass to generated android_binary
      **kwargs: arguments to pass to generated android_instrumentation_test rules
    """

    _manifest_values = {
        "applicationId": test_android_package_name,
        "instrumentationTargetPackage": instrumentation_target_package,
    }
    _manifest_values.update(binary_args.pop("manifest_values", {}))
    native.android_binary(
        name = "%s_binary" % name,
        instruments = instruments,
        manifest = "//tensorflow_lite_support/tools/build_rules/android_test:AndroidManifest_instrumentation_test_template.xml",
        manifest_values = _manifest_values,
        testonly = 1,
        deps = deps + [
            "@maven//:androidx_test_runner",
        ],
        **binary_args
    )
    android_multidevice_instrumentation_test(
        name = "%s_tests" % name,
        target_devices = target_devices,
        test_app = "%s_binary" % name,
        **kwargs
    )
