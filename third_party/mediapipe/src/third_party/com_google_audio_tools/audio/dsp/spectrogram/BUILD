package(default_visibility = ["//visibility:public"])


cc_library(
    name = "inverse_spectrogram",
    srcs = ["inverse_spectrogram.cc"],
    hdrs = ["inverse_spectrogram.h"],
    deps = [
        "//audio/dsp:number_util",
        "//third_party/fft2d",
        "@com_github_glog_glog//:glog",
    ],
)

cc_library(
    name = "spectrogram",
    srcs = ["spectrogram.cc"],
    hdrs = ["spectrogram.h"],
    deps = [
        "//audio/dsp:number_util",
        "//audio/dsp:porting",
        "//audio/dsp:window_functions",
        "//third_party/fft2d:fft2d",
        "@com_github_glog_glog//:glog",
    ],
)
