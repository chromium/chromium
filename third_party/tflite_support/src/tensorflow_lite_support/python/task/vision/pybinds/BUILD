load("//tensorflow_lite_support/python/task:build_defs.bzl", "pybind_extension_may_pack_coral")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension_may_pack_coral(
    name = "_pywrap_image_embedder",
    srcs = [
        "_pywrap_image_embedder.cc",
    ],
    module_name = "_pywrap_image_embedder",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:bounding_box_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto",
        "//tensorflow_lite_support/cc/task/vision:image_embedder",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_image_classifier",
    srcs = [
        "_pywrap_image_classifier.cc",
    ],
    module_name = "_pywrap_image_classifier",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:bounding_box_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:classification_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:classifications_cc_proto",
        "//tensorflow_lite_support/cc/task/vision:image_classifier",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_image_segmenter",
    srcs = [
        "_pywrap_image_segmenter.cc",
    ],
    module_name = "_pywrap_image_segmenter",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:segmentation_options_cc_proto",
        "//tensorflow_lite_support/cc/task/vision:image_segmenter",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_image_searcher",
    srcs = [
        "_pywrap_image_searcher.cc",
    ],
    module_name = "_pywrap_image_searcher",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:bounding_box_cc_proto",
        "//tensorflow_lite_support/cc/task/vision:image_searcher",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_object_detector",
    srcs = [
        "_pywrap_object_detector.cc",
    ],
    module_name = "_pywrap_object_detector",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:detection_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:detections_cc_proto",
        "//tensorflow_lite_support/cc/task/vision:object_detector",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)
