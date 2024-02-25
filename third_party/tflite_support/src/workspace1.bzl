"""
Partial workspace defintion for the TFLite Support Library. See WORKSPACE for usage.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@android_test_support//:repo.bzl", "android_test_repositories")

def tflite_support_workspace1():
    """Partial workspace definition for the TFLite Support Library. See WORKSPACE for usage."""
    android_test_repositories()

    http_archive(
        name = "tf_toolchains",
        sha256 = "d550e6260b7bd8a7f2c2e3a20ba3c4b6c519985f5a164a1bf3601dd96852a05b",
        strip_prefix = "toolchains-1.4.6",
        urls = [
            "http://mirror.tensorflow.org/github.com/tensorflow/toolchains/archive/v1.4.6.tar.gz",
            "https://github.com/tensorflow/toolchains/archive/v1.4.6.tar.gz",
        ],
    )
