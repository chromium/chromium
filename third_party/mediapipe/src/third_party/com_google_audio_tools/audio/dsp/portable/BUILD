load(":c.bzl", "c_library", "c_test")

package(default_visibility = ["//visibility:public"])

c_library(
    name = "auto_gain_control",
    srcs = ["auto_gain_control.c"],
    hdrs = ["auto_gain_control.h"],
    deps = [
        ":fast_fun",
    ],
)

c_test(
    name = "auto_gain_control_test",
    srcs = ["auto_gain_control_test.c"],
    deps = [
        ":auto_gain_control",
        ":logging",
    ],
)

c_library(
    name = "biquad_filter",
    srcs = ["biquad_filter.c"],
    hdrs = ["biquad_filter.h"],
)

c_test(
    name = "biquad_filter_test",
    srcs = ["biquad_filter_test.c"],
    deps = [
        ":biquad_filter",
        ":logging",
        ":math_constants",
    ],
)

c_library(
    name = "butterworth",
    srcs = ["butterworth.c"],
    hdrs = ["butterworth.h"],
    deps = [
        ":biquad_filter",
        ":complex",
        ":math_constants",
    ],
)

c_test(
    name = "butterworth_test",
    srcs = ["butterworth_test.c"],
    deps = [
        ":butterworth",
        ":logging",
        ":math_constants",
    ],
)

c_library(
    name = "complex",
    srcs = ["complex.c"],
    hdrs = ["complex.h"],
)

c_test(
    name = "complex_test",
    srcs = ["complex_test.c"],
    deps = [
        ":complex",
        ":logging",
        ":math_constants",
    ],
)

c_library(
    name = "elliptic_fun",
    srcs = ["elliptic_fun.c"],
    hdrs = ["elliptic_fun.h"],
    deps = [
        ":complex",
        ":math_constants",
    ],
)

c_test(
    name = "elliptic_fun_test",
    srcs = ["elliptic_fun_test.c"],
    deps = [
        ":elliptic_fun",
        ":logging",
        ":math_constants",
    ],
)

c_library(
    name = "fast_fun",
    srcs = ["fast_fun.c"],
    hdrs = ["fast_fun.h"],
    deps = [
        ":math_constants",
    ],
)

c_library(
    name = "fast_fun_compute_tables",
    srcs = ["fast_fun_compute_tables.c"],
    hdrs = ["fast_fun_compute_tables.h"],
)

c_test(
    name = "fast_fun_test",
    srcs = ["fast_fun_test.c"],
    deps = [
        ":fast_fun",
        ":fast_fun_compute_tables",
        ":logging",
    ],
)

c_library(
    name = "iir_design",
    srcs = ["iir_design.c"],
    hdrs = ["iir_design.h"],
    deps = [
        ":biquad_filter",
        ":complex",
        ":elliptic_fun",
        ":math_constants",
    ],
)

c_test(
    name = "iir_design_test",
    srcs = ["iir_design_test.c"],
    deps = [
        ":complex",
        ":iir_design",
        ":logging",
        ":math_constants",
    ],
)

c_library(
    name = "logging",
    hdrs = ["logging.h"],
)

c_test(
    name = "logging_c_test",
    srcs = ["logging_test.c"],
    deps = [":logging"],
)

c_test(
    name = "logging_cpp_test",
    srcs = ["logging_test.cc"],
    deps = [":logging"],
)

c_library(
    name = "math_constants",
    hdrs = ["math_constants.h"],
)

c_test(
    name = "math_constants_test",
    srcs = ["math_constants_test.c"],
    deps = [
        ":logging",
        ":math_constants",
    ],
)

c_library(
    name = "number_util",
    srcs = ["number_util.c"],
    hdrs = ["number_util.h"],
)

c_test(
    name = "number_util_test",
    srcs = ["number_util_test.c"],
    deps = [
        ":logging",
        ":math_constants",
        ":number_util",
    ],
)

c_library(
    name = "phasor_rotator",
    srcs = ["phasor_rotator.c"],
    hdrs = ["phasor_rotator.h"],
    deps = [
        ":math_constants",
    ],
)

c_test(
    name = "phasor_rotator_test",
    srcs = ["phasor_rotator_test.c"],
    deps = [
        ":complex",
        ":logging",
        ":math_constants",
        ":phasor_rotator",
    ],
)

c_library(
    name = "read_wav_file",
    srcs = ["read_wav_file.c"],
    hdrs = ["read_wav_file.h"],
    deps = [
        ":logging",
        ":read_wav_file_generic",
        ":read_wav_info",
    ],
)

c_library(
    name = "read_wav_file_generic",
    srcs = ["read_wav_file_generic.c"],
    hdrs = ["read_wav_file_generic.h"],
    deps = [
        ":logging",
        ":read_wav_info",
        ":serialize",
    ],
)

c_test(
    name = "read_wav_file_generic_test",
    srcs = ["read_wav_file_generic_test.c"],
    deps = [
        ":logging",
        ":read_wav_file_generic",
    ],
)

c_library(
    name = "read_wav_info",
    hdrs = ["read_wav_info.h"],
)

c_test(
    name = "read_wav_file_test",
    srcs = ["read_wav_file_test.c"],
    deps = [
        ":logging",
        ":read_wav_file",
        ":write_wav_file",
    ],
)

c_library(
    name = "rational_factor_resampler",
    srcs = ["rational_factor_resampler.c"],
    hdrs = ["rational_factor_resampler.h"],
    deps = [
        ":number_util",
        ":rational_factor_resampler_kernel",
    ],
)

c_test(
    name = "rational_factor_resampler_test",
    srcs = ["rational_factor_resampler_test.c"],
    deps = [
        ":logging",
        ":math_constants",
        ":phasor_rotator",
        ":rational_factor_resampler",
        ":rational_factor_resampler_kernel",
    ],
)

c_library(
    name = "rational_factor_resampler_kernel",
    srcs = ["rational_factor_resampler_kernel.c"],
    hdrs = ["rational_factor_resampler_kernel.h"],
    deps = [
        ":math_constants",
    ],
)

c_test(
    name = "rational_factor_resampler_kernel_test",
    srcs = ["rational_factor_resampler_kernel_test.c"],
    deps = [
        ":logging",
        ":math_constants",
        ":rational_factor_resampler_kernel",
    ],
)

c_library(
    name = "serialize",
    hdrs = ["serialize.h"],
)

c_test(
    name = "serialize_test",
    srcs = ["serialize_test.c"],
    deps = [
        ":logging",
        ":serialize",
    ],
)

c_library(
    name = "write_wav_file",
    srcs = ["write_wav_file.c"],
    hdrs = ["write_wav_file.h"],
    deps = [":write_wav_file_generic"],
)

c_library(
    name = "write_wav_file_generic",
    srcs = ["write_wav_file_generic.c"],
    hdrs = ["write_wav_file_generic.h"],
    deps = [":serialize"],
)

c_test(
    name = "write_wav_file_test",
    srcs = ["write_wav_file_test.c"],
    deps = [
        ":logging",
        ":write_wav_file",
    ],
)

# C++ support.
cc_library(
    name = "write_wav_string",
    srcs = ["write_wav_string.cc"],
    hdrs = ["write_wav_string.h"],
    deps = [
        ":write_wav_file_generic",
        "@com_google_absl//absl/strings",
    ],
)

