load("@build_bazel_rules_android//android:rules.bzl", "android_library")

licenses(["notice"])

android_library(
    name = "image",
    srcs = glob(["src/**/*.java"]),
    custom_package = "com.google.android.odml.image",
    manifest = "AndroidManifest.xml",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
    ],
)
