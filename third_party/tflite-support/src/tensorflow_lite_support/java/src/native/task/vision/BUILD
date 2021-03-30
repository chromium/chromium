load("@org_tensorflow//tensorflow/lite:build_def.bzl", "tflite_jni_binary")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "jni_utils",
    srcs = [
        "jni_utils.cc",
    ],
    hdrs = [
        "jni_utils.h",
    ],
    deps = [
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:class_proto_inc",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/jni",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "task_vision_native",
    srcs = [
        ":libtask_vision_jni.so",
    ],
)

tflite_jni_binary(
    name = "libtask_vision_jni.so",
    srcs = [
        "//tensorflow_lite_support/java/src/native/task/vision/classifier:image_classifier_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/vision/detector:object_detector_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/vision/segmenter:image_segmenter_jni.cc",
    ],
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision:image_classifier",
        "//tensorflow_lite_support/cc/task/vision:image_segmenter",
        "//tensorflow_lite_support/cc/task/vision:object_detector",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:classifications_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:detections_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_classifier_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_segmenter_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:object_detector_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:segmentations_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/jni",
        "//tensorflow_lite_support/java/src/native/task/vision:jni_utils",
        "@com_google_absl//absl/strings",
    ],
)
