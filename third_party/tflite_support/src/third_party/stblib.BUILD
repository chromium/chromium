# Description:
#   Single-file C++ image decoding and encoding libraries

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])  # MIT license

exports_files(["LICENSE"])

cc_library(
    name = "stb_image",
    hdrs = ["stb_image.h"],
    copts = [
        "-Wno-unused-function",
        "$(STACK_FRAME_UNLIMITED)",
    ],
    includes = ["."],
)

cc_library(
    name = "stb_image_write",
    hdrs = ["stb_image_write.h"],
    includes = ["."],
)
