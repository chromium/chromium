"""ICU data build."""

licenses(["notice"])  # Apache 2.0

package(
    default_visibility = ["//visibility:public"],
)

exports_files(["LICENSE"])

# To replicate this pre-processing (if you want additional ICU data), do the
# following:
#
# Note: I had to build from master, but I'm sure any version after 64.2 would
# will work.
#
# For guidance, see: go/icu-data &
# http://google3/third_party/icu/g3data/icu_data.bzl?l=47&rcl=252943749
#
# First, download, build, and install ICU. This installs tools such as makeconv.
# Then, run the following from your icu4c/source directory:
#   $ ICU_DATA_FILTER_FILE=filters.json ./runConfigureICU Linux
#   $ make clean && make
#   $ cd data/out/tmp
#   $ genccode icudt64l.dat
#   $ echo 'U_CAPI const void * U_EXPORT2 uprv_getICUData_other() { return icudt64l_dat.bytes; }' >> icudt64l_dat.c
# This creates icudt64l_dat.c which you can rename and move in here.
cc_library(
    name = "icu_normalization_data",
    srcs = ["normalization_data.c"],
    deps = ["@icu//:headers"],
    alwayslink = 1,
)
