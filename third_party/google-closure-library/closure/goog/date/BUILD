load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "date",
    srcs = ["date.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/i18n:datetimesymbols",
        "//closure/goog/string",
    ],
)

alias(
    name = "datelike",
    actual = ":date",
)

closure_js_library(
    name = "daterange",
    srcs = ["daterange.js"],
    deps = [
        ":date",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "duration",
    srcs = ["duration.js"],
    deps = [
        "//closure/goog/i18n:datetimeformat",
        "//closure/goog/i18n:messageformat",
    ],
)

closure_js_library(
    name = "relative",
    srcs = ["relative.js"],
    deps = [
        ":date",
        "//closure/goog/i18n:datetimeformat",
        "//closure/goog/i18n:datetimepatterns",
        "//closure/goog/i18n:relativedatetimeformat",
    ],
)

closure_js_library(
    name = "relativecommontests",
    testonly = 1,
    srcs = ["relativecommontests.js"],
    deps = [
        ":date",
        ":relative",
        "//closure/goog/i18n:datetimeformat",
        "//closure/goog/i18n:datetimepatterns",
        "//closure/goog/i18n:datetimesymbols",
        "//closure/goog/i18n:numberformatsymbols",
        "//closure/goog/i18n:relativedatetimesymbols",
        "//closure/goog/testing:propertyreplacer",
        "//closure/goog/testing:testsuite",
    ],
)

closure_js_library(
    name = "utcdatetime",
    srcs = ["utcdatetime.js"],
    deps = [":date"],
)
