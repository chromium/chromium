load("@build_bazel_rules_android//android:rules.bzl", "android_library", "android_local_test")
load("//tensorflow_lite_support/tools/build_rules/android_test:android_library_instrumentation_tests.bzl", "android_library_instrumentation_tests")

package(default_testonly = 1)

licenses(["notice"])

INSTRUMENTED_TESTS = glob(["**/*InstrumentedTest.java"])

# Focus testing on the oldest and newest officially supported APIs.
DEFAULT_INSTRUMENTED_DEVICES = [
]

exports_files(["AndroidManifest.xml"])

android_library(
    name = "test_lib",
    testonly = 1,
    assets = glob(["assets/**"]),
    assets_dir = "assets",
    manifest = "AndroidManifest.xml",
)

android_local_test(
    name = "GpuDelegateProxyTest",
    srcs = ["model/GpuDelegateProxyTest.java"],
    manifest = "AndroidManifest.xml",
    nocompress_extensions = ["tflite"],
    tags = [
        "noasan",
        "nomsan",
        "notsan",
    ],
    test_class = "org.tensorflow.lite.support.model.GpuDelegateProxyTest",
    deps = [
        ":test_lib",
        "//tensorflow_lite_support/java:tensorflow-lite-support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

# TODO(138904571): add a bzl file to declare tests automatically for every java test file.
# Note: This test is not able to be run with --test_strategy=local.
android_local_test(
    name = "ModelTest",
    srcs = ["model/ModelTest.java"],
    manifest = "AndroidManifest.xml",
    nocompress_extensions = ["tflite"],
    tags = [
        "noasan",
        "nomsan",
        "notsan",
    ],
    test_class = "org.tensorflow.lite.support.model.ModelTest",
    deps = [
        ":test_lib",
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "TensorBufferTest",
    srcs = [
        "tensorbuffer/TensorBufferTest.java",
    ],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.tensorbuffer.TensorBufferTest",
    deps = [
        ":test_lib",
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "TensorBufferFloatTest",
    srcs = [
        "tensorbuffer/TensorBufferFloatTest.java",
    ],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.tensorbuffer.TensorBufferFloatTest",
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "TensorBufferUint8Test",
    srcs = [
        "tensorbuffer/TensorBufferUint8Test.java",
    ],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.tensorbuffer.TensorBufferUint8Test",
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "TensorImageTest",
    size = "small",
    srcs = [
        "image/TensorImageTest.java",
        "image/TestImageCreator.java",
    ],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.image.TensorImageTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_mockito_mockito_core",
        "@maven//:org_mockito_mockito_inline",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "BoundingBoxUtilTest",
    size = "small",
    srcs = ["image/BoundingBoxUtilTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.image.BoundingBoxUtilTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlitelib_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ImageConversionsTest",
    size = "small",
    srcs = [
        "image/ImageConversionsTest.java",
        "image/TestImageCreator.java",
    ],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.image.ImageConversionsTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlitelib_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ImageProcessorTest",
    srcs = ["image/ImageProcessorTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.image.ImageProcessorTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlitelib_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "NormalizeOpTest",
    srcs = ["common/ops/NormalizeOpTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.common.ops.NormalizeOpTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlitelib_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "CastOpTest",
    srcs = ["common/ops/CastOpTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.common.ops.CastOpTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlitelib_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "DequantizeOpTest",
    srcs = ["common/ops/DequantizeOpTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.common.ops.DequantizeOpTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlitelib_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "QuantizeOpTest",
    srcs = ["common/ops/QuantizeOpTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.common.ops.QuantizeOpTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlitelib_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "LabelAxisOpTest",
    srcs = ["label/ops/LabelAxisOpTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.label.ops.LabelAxisOpTest",
    visibility = ["//visibility:private"],
    deps = [
        ":test_lib",
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "CategoryTest",
    srcs = ["label/CategoryTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.label.CategoryTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "LabelUtilTest",
    srcs = ["label/LabelUtilTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.label.LabelUtilTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "TensorLabelTest",
    srcs = ["label/TensorLabelTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.label.TensorLabelTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "FileUtilTest",
    srcs = ["common/FileUtilTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.common.FileUtilTest",
    visibility = ["//visibility:private"],
    deps = [
        ":test_lib",
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "TensorProcessorTest",
    srcs = ["common/TensorProcessorTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.common.TensorProcessorTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ColorSpaceTypeTest",
    size = "small",
    srcs = [
        "image/ColorSpaceTypeTest.java",
        "image/TestImageCreator.java",
    ],
    manifest = "AndroidManifest.xml",
    test_class = "org.tensorflow.lite.support.image.ColorSpaceTypeTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:junit_junit",
        "@maven//:org_robolectric_robolectric",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "TensorAudioTest",
    size = "small",
    srcs = [
        "audio/TensorAudioTest.java",
    ],
    manifest = "AndroidManifest.xml",
    manifest_values = {
        "minSdkVersion": "23",
    },
    test_class = "org.tensorflow.lite.support.audio.TensorAudioTest",
    visibility = ["//visibility:private"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",
        "@maven//:org_mockito_mockito_core",
        "@maven//:org_mockito_mockito_inline",
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

test_suite(
    name = "instrumentation_tests",
    tags = [
        "no_oss",
        "tflite_emulator_test_android",
    ],
)

android_library(
    name = "test_image_creator",
    testonly = 1,
    srcs = ["image/TestImageCreator.java"],
    manifest = "AndroidManifest.xml",
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
    ],
)

# This rule specifies a bundle of tests, in which each test source file converts
# to a test target.
android_library_instrumentation_tests(
    name = "instrumented_unittests",
    srcs = INSTRUMENTED_TESTS,
    binary_args = {
        "multidex": "legacy",
    },
    device_list = DEFAULT_INSTRUMENTED_DEVICES,
    tags = [
        "no_oss",
        "noasan",  # Avoid build breakage
        "nomsan",  # Avoid build breakage
        "notsan",  # Avoid build breakage
        "nozapfhahn",  # Avoid coverage test breakage
        "tflite_emulator_test_android",
    ],
    test_java_package = "org.tensorflow.lite.support",
    deps = [
        ":test_image_creator",
        ":test_lib",
        "//tensorflow_lite_support/java:tensorflowlite_support",
        "@maven//:androidx_multidex_multidex",
        "@maven//:androidx_test_core",
        "@maven//:androidx_test_ext_junit",
        "@maven//:androidx_test_runner",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:junit_junit",  # android
        "@maven//:org_mockito_mockito_core",
        "@maven//:org_mockito_mockito_inline",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_api",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_gpu",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
    ],
)
