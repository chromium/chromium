package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])  # Apache 2, BSD, MIT

exports_files(["LICENSE"])

cc_library(
    name = "basic_types",
    hdrs = ["basic_types.h"],
)

cc_library(
    name = "threadpool",
    hdrs = ["threadpool.h"],
)

cc_library(
    name = "status",
    srcs = ["status.cc"],
    hdrs = [
        "canonical_errors.h",
        "status.h",
        "status_builder.h",
        "status_macros.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)
