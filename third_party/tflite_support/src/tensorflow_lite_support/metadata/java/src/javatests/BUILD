# Description:
# Test targets for TFLite MetadataExtractor.
load("@build_bazel_rules_android//android:rules.bzl", "android_library", "android_local_test")
load("@org_tensorflow//tensorflow/java:build_defs.bzl", "JAVACOPTS")

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

android_library(
    name = "test_lib",
    testonly = 1,
    assets = glob(["org/tensorflow/lite/support/assets/**"]),
    assets_dir = "org/tensorflow/lite/support/assets",
    manifest = "AndroidManifest.xml",
)

android_local_test(
    name = "MetadataExtractorTest",
    srcs = ["org/tensorflow/lite/support/metadata/MetadataExtractorTest.java"],
    manifest = "AndroidManifest.xml",
    nocompress_extensions = [
        "tflite",
        "txt",
    ],
    tags = ["no_oss"],
    test_class = "org.tensorflow.lite.support.metadata.MetadataExtractorTest",
    deps = [
        ":test_lib",  # unuseddeps: keep
        "//tensorflow_lite_support/metadata:metadata_schema_fbs_android",
        "//tensorflow_lite_support/metadata:schema_fbs_android",
        "//tensorflow_lite_support/metadata/java:tensorflowlite_support_metadata",
        "@flatbuffers//:runtime_android",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:commons_io_commons_io",
        "@maven//:org_robolectric_robolectric",
        "@org_checkerframework_qual",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ByteBufferChannelTest",
    srcs = [
        "org/tensorflow/lite/support/metadata/ByteBufferChannelTest.java",
    ],
    manifest = "AndroidManifest.xml",
    tags = ["no_oss"],
    test_class = "org.tensorflow.lite.support.metadata.ByteBufferChannelTest",
    deps = [
        "//tensorflow_lite_support/metadata/java:tensorflowlite_support_metadata",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ZipFileTest",
    srcs = ["org/tensorflow/lite/support/metadata/ZipFileTest.java"],
    manifest = "AndroidManifest.xml",
    nocompress_extensions = [
        "tflite",
        "txt",
    ],
    tags = ["no_oss"],
    test_class = "org.tensorflow.lite.support.metadata.ZipFileTest",
    deps = [
        ":test_lib",
        "//tensorflow_lite_support/metadata/java:tensorflowlite_support_metadata",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:commons_io_commons_io",
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "BoundedInputStreamTest",
    srcs = [
        "org/tensorflow/lite/support/metadata/BoundedInputStreamTest.java",
    ],
    manifest = "AndroidManifest.xml",
    tags = ["no_oss"],
    test_class = "org.tensorflow.lite.support.metadata.BoundedInputStreamTest",
    deps = [
        "//tensorflow_lite_support/metadata/java:tensorflowlite_support_metadata",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

java_test(
    name = "MetadataParserTest",
    size = "small",
    srcs = ["org/tensorflow/lite/support/metadata/MetadataParserTest.java"],
    javacopts = JAVACOPTS,
    tags = ["no_oss"],
    test_class = "org.tensorflow.lite.support.metadata.MetadataParserTest",
    deps = [
        "//tensorflow_lite_support/metadata/java:tensorflowlite_support_metadata_lib",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
    ],
)
