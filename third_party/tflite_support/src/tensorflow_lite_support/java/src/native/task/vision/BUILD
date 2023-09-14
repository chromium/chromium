load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite", "jni_binary_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "jni_utils",
    srcs = [
        "jni_utils.cc",
    ],
    hdrs = [
        "jni_utils.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/utils:jni_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:class_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/java/jni",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_with_tflite(
    name = "task_vision_native",
    tflite_jni_binaries = [
        ":libtask_vision_jni.so",
    ],
)

jni_binary_with_tflite(
    name = "libtask_vision_jni.so",
    srcs = [
        "//tensorflow_lite_support/java/src/native/task/core:task_jni_utils.cc",
        "//tensorflow_lite_support/java/src/native/task/vision/classifier:image_classifier_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/vision/core:base_vision_task_api_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/vision/detector:object_detector_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/vision/searcher:image_searcher_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/vision/segmenter:image_segmenter_jni.cc",
    ],
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    tflite_deps = [
        "//tensorflow_lite_support/java/src/native/task/core:builtin_op_resolver",
        "//tensorflow_lite_support/cc/task/vision:image_classifier",
        "//tensorflow_lite_support/cc/task/vision:image_segmenter",
        "//tensorflow_lite_support/cc/task/vision:object_detector",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/src/native/task/vision:jni_utils",
        "//tensorflow_lite_support/cc/task/vision:image_searcher",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:classifications_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:detections_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_classifier_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_searcher_options_cc_proto",
        "//tensorflow_lite_support/cc/task/vision/proto:image_segmenter_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:object_detector_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:segmentations_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/java/jni",
        "@com_google_absl//absl/strings",
    ],
)
