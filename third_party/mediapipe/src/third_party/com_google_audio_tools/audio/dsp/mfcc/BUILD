package(default_visibility = ["//visibility:public"])

cc_library(
    name = "mfcc",
    srcs = [
        "dct.cc",
        "mel_filterbank.cc",
        "mfcc.cc",
    ],
    hdrs = [
        "dct.h",
        "mel_filterbank.h",
        "mfcc.h",
    ],
    deps = [
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "mel_filterbank_test",
    size = "small",
    srcs = ["mel_filterbank_test.cc"],
    deps = [
        ":mfcc",
        "//audio/dsp:porting",
        "//audio/dsp:signal_vector_util",
        "@com_google_absl//absl/random",
        "@gtest//:gtest_main",

    ],
)

cc_test(
    name = "dct_test",
    size = "small",
    srcs = ["dct_test.cc"],
    deps = [
        ":mfcc",
        "//audio/dsp:porting",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "mfcc_test",
    size = "small",
    srcs = ["mfcc_test.cc"],
    deps = [
        ":mfcc",
        "//audio/dsp:porting",
        "//audio/dsp:signal_vector_util",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)
