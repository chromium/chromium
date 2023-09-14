load("@build_bazel_rules_android//android:rules.bzl", "android_library")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

android_library(
    name = "base-task-api",
    srcs = glob(["**/*.java"]) + [
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/processor:task_processor_src",
    ],
    proguard_specs = ["proguard.flags"],
    visibility = ["//visibility:public"],
    # LINT.IfChange(dep)
    deps = [
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
    ],
    # LINT.ThenChange(<INTERNAL>/release/build_task_base_pom.sh:dep)
)

alias(
    name = "base_task_api",
    actual = ":base-task-api",
)
