load("@build_bazel_rules_android//android:rules.bzl", "android_library", "android_local_test")

licenses(["notice"])

INSTRUMENTED_TESTS = glob(["*InstrumentedTest.java"])

android_library(
    name = "odml-image-test",
    srcs = [
        "TestImageCreator.java",
    ],
    custom_package = "com.google.android.odml.image",
    # TODO(b/163039980): Use JAVACOPTS in TF. "-Xep:RemoveUnusedImports:ERROR" wierdly break the build.
    manifest = "AndroidManifest.xml",
    deps = [
        "@maven//:androidx_annotation_annotation",
    ],
)

android_local_test(
    name = "BitmapMlImageBuilderTest",
    srcs = ["BitmapMlImageBuilderTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "com.google.android.odml.image.BitmapMlImageBuilderTest",
    deps = [
        "//tensorflow_lite_support/odml/java/image",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "BitmapExtractorTest",
    srcs = ["BitmapExtractorTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "com.google.android.odml.image.BitmapExtractorTest",
    deps = [
        ":odml-image-test",
        "//tensorflow_lite_support/odml/java/image",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ByteBufferMlImageBuilderTest",
    srcs = ["ByteBufferMlImageBuilderTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "com.google.android.odml.image.ByteBufferMlImageBuilderTest",
    deps = [
        "//tensorflow_lite_support/odml/java/image",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "ByteBufferExtractorTest",
    srcs = ["ByteBufferExtractorTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "com.google.android.odml.image.ByteBufferExtractorTest",
    deps = [
        ":odml-image-test",
        "//tensorflow_lite_support/odml/java/image",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "MediaMlImageTest",
    srcs = [
        "MediaImageExtractorTest.java",
        "MediaMlImageBuilderTest.java",
    ],
    manifest = "AndroidManifest.xml",
    test_class = "com.google.android.odml.image.MediaMlImageBuilderTest",
    deps = [
        "//tensorflow_lite_support/odml/java/image",
        "//third_party/java/mockito:mockito-android",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)

android_local_test(
    name = "TextureImageExtractorTest",
    srcs = ["TextureImageExtractorTest.java"],
    manifest = "AndroidManifest.xml",
    test_class = "com.google.android.odml.image.TextureImageExtractorTest",
    deps = [
        ":odml-image-test",
        "//tensorflow_lite_support/odml/java/image",
        "//third_party/java/mockito:mockito-android",
        "@maven//:androidx_test_core",
        "@maven//:com_google_truth_truth",  # android
        "@maven//:org_robolectric_robolectric",
        "@robolectric//bazel:android-all",
    ],
)
