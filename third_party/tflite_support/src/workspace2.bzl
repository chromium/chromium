"""
Partial workspace defintion for the TFLite Support Library. See WORKSPACE for usage.
"""

load("//third_party/tensorflow:tf_configure.bzl", "tf_configure")
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
load("@build_bazel_rules_apple//apple:repositories.bzl", "apple_rules_dependencies")
load("@build_bazel_apple_support//lib:repositories.bzl", "apple_support_dependencies")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party/py:python_configure.bzl", "python_configure")
load("@upb//bazel:repository_defs.bzl", "bazel_version_repository")

def tflite_support_workspace2():
    """Partial workspace definition for the TFLite Support Library. See WORKSPACE for usage."""
    tf_configure(name = "local_config_tf")
    grpc_deps()
    apple_rules_dependencies()
    apple_support_dependencies()

    bazel_version_repository(name = "bazel_version")

    python_configure(name = "local_config_python")

    ATS_TAG = "androidx-test-1.3.0"
    http_archive(
        name = "android_test_support",
        strip_prefix = "android-test-%s" % ATS_TAG,
        urls = ["https://github.com/android/android-test/archive/%s.tar.gz" % ATS_TAG],
    )
