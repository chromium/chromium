"""
Partial workspace defintion for the TFLite Support Library. See WORKSPACE for usage.
"""

load("@robolectric//bazel:robolectric.bzl", "robolectric_repositories")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("//third_party/flatbuffers:workspace.bzl", flatbuffers = "repo")

def tflite_support_workspace7():
    """Partial workspace definition for the TFLite Support Library. See WORKSPACE for usage."""
    robolectric_repositories()

    flatbuffers()

    RULES_JVM_EXTERNAL_TAG = "4.2"

    http_archive(
        name = "rules_jvm_external",
        strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
        sha256 = "cd1a77b7b02e8e008439ca76fd34f5b07aecb8c752961f9640dea15e9e5ba1ca",
        url = "https://github.com/bazelbuild/rules_jvm_external/archive/refs/tags/%s.zip" % RULES_JVM_EXTERNAL_TAG,
    )
