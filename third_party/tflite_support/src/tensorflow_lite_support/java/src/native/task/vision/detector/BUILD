load("@org_tensorflow//tensorflow/lite:build_def.bzl", "tflite_jni_binary")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["object_detector_jni.cc"])

cc_library(
    name = "object_detector_native",
    srcs = [
        ":libtask_vision_jni.so",
    ],
)

tflite_jni_binary(
    name = "libtask_vision_jni.so",
    srcs = [
        "object_detector_jni.cc",
    ],
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision:object_detector",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:detections_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:object_detector_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/jni",
        "//tensorflow_lite_support/java/src/native/task/vision:jni_utils",
        "@com_google_absl//absl/strings",
    ],
)
