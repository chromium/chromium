load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "cssspecificity",
    srcs = ["cssspecificity.js"],
    deps = [
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "legacyconversions",
    srcs = ["legacyconversions.js"],
    deps = [
        ":safehtml",
        ":safescript",
        ":safestyle",
        ":safestylesheet",
        ":safeurl",
        ":trustedresourceurl",
    ],
)

closure_js_library(
    name = "safehtml",
    srcs = ["safehtml.js"],
    deps = [
        ":safescript",
        ":safestyle",
        ":safestylesheet",
        ":safeurl",
        ":trustedresourceurl",
        ":trustedtypes",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom:tagname",
        "//closure/goog/dom:tags",
        "//closure/goog/i18n:bidi",
        "//closure/goog/labs/useragent:browser",
        "//closure/goog/object",
        "//closure/goog/string:const",
        "//closure/goog/string:internal",
        "//closure/goog/string:typedstring",
    ],
)

closure_js_library(
    name = "safehtmlformatter",
    srcs = ["safehtmlformatter.js"],
    deps = [
        ":safehtml",
        "//closure/goog/asserts",
        "//closure/goog/dom:tags",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "safescript",
    srcs = ["safescript.js"],
    deps = [
        ":trustedtypes",
        "//closure/goog/asserts",
        "//closure/goog/string:const",
        "//closure/goog/string:typedstring",
    ],
)

closure_js_library(
    name = "safestyle",
    srcs = ["safestyle.js"],
    deps = [
        ":safeurl",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/string:const",
        "//closure/goog/string:internal",
        "//closure/goog/string:typedstring",
    ],
)

closure_js_library(
    name = "safestylesheet",
    srcs = ["safestylesheet.js"],
    deps = [
        ":safestyle",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/object",
        "//closure/goog/string:const",
        "//closure/goog/string:internal",
        "//closure/goog/string:typedstring",
    ],
)

closure_js_library(
    name = "safeurl",
    srcs = ["safeurl.js"],
    deps = [
        ":trustedresourceurl",
        "//closure/goog/asserts",
        "//closure/goog/fs:url",
        "//closure/goog/i18n:bidi",
        "//closure/goog/string:const",
        "//closure/goog/string:internal",
        "//closure/goog/string:typedstring",
    ],
)

closure_js_library(
    name = "safeurl_test_vectors",
    testonly = 1,
    srcs = ["safeurl_test_vectors.js"],
)

closure_js_library(
    name = "testing",
    testonly = False,
    srcs = ["testing.js"],
    deps = [
        ":safehtml",
        ":safescript",
        ":safestyle",
        ":safestylesheet",
        ":safeurl",
        ":trustedresourceurl",
        "//closure/goog/i18n:bidi",
        "//closure/goog/testing:mockmatchers",
    ],
)

closure_js_library(
    name = "textextractor",
    srcs = ["textextractor.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/dom:tagname",
        "//closure/goog/html/sanitizer:htmlsanitizer",
        "//closure/goog/object",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "trustedresourceurl",
    srcs = ["trustedresourceurl.js"],
    deps = [
        ":safescript",
        ":trustedtypes",
        "//closure/goog/asserts",
        "//closure/goog/fs:blob",
        "//closure/goog/fs:url",
        "//closure/goog/i18n:bidi",
        "//closure/goog/string:const",
        "//closure/goog/string:typedstring",
    ],
)

closure_js_library(
    name = "trustedtypes",
    srcs = ["trustedtypes.js"],
    deps = ["//closure/goog/memoize"],
)

closure_js_library(
    name = "uncheckedconversions",
    srcs = ["uncheckedconversions.js"],
    deps = [
        ":safehtml",
        ":safescript",
        ":safestyle",
        ":safestylesheet",
        ":safeurl",
        ":trustedresourceurl",
        "//closure/goog/asserts",
        "//closure/goog/i18n:bidi",
        "//closure/goog/string:const",
        "//closure/goog/string:internal",
    ],
)

closure_js_library(
    name = "utils",
    srcs = ["utils.js"],
    deps = ["//closure/goog/string"],
)
