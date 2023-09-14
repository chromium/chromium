package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

# AudioClassifier protos.

proto_library(
    name = "audio_classifier_options_proto",
    srcs = ["audio_classifier_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
    ],
)

cc_proto_library(
    name = "audio_classifier_options_cc_proto",
    deps = [
        ":audio_classifier_options_proto",
    ],
)

cc_library(
    name = "classifications_proto_inc",
    hdrs = ["classifications_proto_inc.h"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:classifications_cc_proto",
    ],
)

cc_library(
    name = "class_proto_inc",
    hdrs = ["class_proto_inc.h"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:class_cc_proto",
    ],
)

proto_library(
    name = "audio_embedder_options_proto",
    srcs = ["audio_embedder_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_proto",
    ],
)

cc_proto_library(
    name = "audio_embedder_options_cc_proto",
    deps = [":audio_embedder_options_proto"],
)
