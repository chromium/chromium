load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "pffft",
    hdrs = [
        "include/pffft/pffft.h",
    ],
    srcs = [
        "src/pffft.c",
        "src/pffft_common.c",
        "src/pffft_priv_impl.h",
        "src/sse2neon.h",
    ] + glob([
        "src/simd/*.h",
    ]),
    includes = [
        "include",
        "include/pffft",
    ],
    copts = [
        "-O3",
    ],
    linkopts = ["-lm"],
)
