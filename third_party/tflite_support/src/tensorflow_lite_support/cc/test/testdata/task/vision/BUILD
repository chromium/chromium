package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(glob(["*.*"]))

filegroup(
    name = "test_models",
    srcs = glob([
        "*.tflite",
    ]),
)

filegroup(
    name = "test_images",
    srcs = glob([
        "*.jpg",
        "*.png",
    ]),
)

filegroup(
    name = "test_indices",
    srcs = glob([
        "*.ldb",
    ]),
)
