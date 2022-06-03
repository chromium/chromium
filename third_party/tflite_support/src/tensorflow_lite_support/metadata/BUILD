load("@flatbuffers//:build_defs.bzl", "flatbuffer_android_library", "flatbuffer_cc_library", "flatbuffer_java_library", "flatbuffer_py_library")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["metadata_schema.fbs"])

flatbuffer_py_library(
    name = "schema_py",
    srcs = ["@org_tensorflow//tensorflow/lite/schema:schema.fbs"],
)

# Generic schema for inference on device.
flatbuffer_android_library(
    name = "schema_fbs_android",
    srcs = ["@org_tensorflow//tensorflow/lite/schema:schema.fbs"],
    custom_package = "org.tensorflow.lite.schema",
)

flatbuffer_java_library(
    name = "schema_fbs_java",
    srcs = ["@org_tensorflow//tensorflow/lite/schema:schema.fbs"],
    custom_package = "org.tensorflow.lite.schema",
)

# Generic schema for model metadata.
flatbuffer_cc_library(
    name = "metadata_schema_cc",
    srcs = ["metadata_schema.fbs"],
)

flatbuffer_py_library(
    name = "metadata_schema_py",
    srcs = ["metadata_schema.fbs"],
)

flatbuffer_java_library(
    name = "metadata_schema_java",
    srcs = ["metadata_schema.fbs"],
    custom_package = "org.tensorflow.lite.support.metadata.schema",
)

flatbuffer_android_library(
    name = "metadata_schema_fbs_android",
    srcs = ["metadata_schema.fbs"],
    custom_package = "org.tensorflow.lite.support.metadata.schema",
)
