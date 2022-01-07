load("@build_bazel_rules_android//android:rules.bzl", "android_library")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

android_library(
    name = "base-task-api",
    srcs = glob(["**/*.java"]),
    javacopts = ["-source 7 -target 7"],
    visibility = ["//visibility:public"],
    # LINT.IfChange(dep)
    deps = [
        "@com_google_auto_value",
    ],
    # LINT.ThenChange(<INTERNAL>/release/build_task_base_pom.sh:dep)
)

alias(
    name = "base_task_api",
    actual = ":base-task-api",
)
