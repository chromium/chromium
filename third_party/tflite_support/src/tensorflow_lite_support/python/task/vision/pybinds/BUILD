load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")
# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "image_embedder_options_pb2",
    srcs = ["image_embedder_options_pb2.py"],
    deps = ["//tensorflow_lite_support/cc/task/vision/proto:image_embedder_options_py_pb2"],
)

pybind_extension(
    name = "_pywrap_image_embedder",
    srcs = [
        "_pywrap_image_embedder.cc",
    ],
    module_name = "_pywrap_image_embedder",
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision:image_embedder",
        "//tensorflow_lite_support/examples/task/vision/desktop/utils:image_utils",
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:status_casters",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

py_library(
    name = "image_classifier_options_pb2",
    srcs = ["image_classifier_options_pb2.py"],
    deps = ["//tensorflow_lite_support/cc/task/vision/proto:image_classifier_options_py_pb2"],
)

pybind_extension(
    name = "_pywrap_image_classifier",
    srcs = [
        "_pywrap_image_classifier.cc",
    ],
    module_name = "_pywrap_image_classifier",
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision:image_classifier",
        "//tensorflow_lite_support/examples/task/vision/desktop/utils:image_utils",
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:status_casters",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

py_library(
    name = "object_detector_options_pb2",
    srcs = ["object_detector_options_pb2.py"],
    deps = ["//tensorflow_lite_support/cc/task/vision/proto:object_detector_options_py_pb2"],
)

pybind_extension(
    name = "_pywrap_object_detector",
    srcs = [
        "_pywrap_object_detector.cc",
    ],
    module_name = "_pywrap_object_detector",
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision:object_detector",
        "//tensorflow_lite_support/examples/task/vision/desktop/utils:image_utils",
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:status_casters",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)
