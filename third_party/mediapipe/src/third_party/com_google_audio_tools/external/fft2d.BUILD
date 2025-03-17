package(default_visibility = ["//visibility:public"])

# Unrestricted use; can only distribute original package.
# See fft2d/readme2d.txt
licenses(["notice"])

exports_files(["LICENSE"])

# This is the main 2D FFT library.  The 2D FFTs in this library call
# 1D FFTs.  In addition, fast DCTs are provided for the special case
# of 8x8 and 16x16.  This code in this library is referred to as
# "Version II" on http://momonga.t.u-tokyo.ac.jp/~ooura/fft.html.
cc_library(
    name = "fft2d",
    srcs = [
        "fft2d/alloc.c",
        "fft2d/fftsg.c",
        "fft2d/fftsg2d.c",
        "fft2d/shrtdct.c",
    ],
    textual_hdrs = [
        "fft2d/alloc.h",
    ],
    linkopts = ["-lm"],
)
