load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "cssspecificity",
    srcs = ["cssspecificity.js"],
    lenient = True,
)

closure_js_library(
    name = "legacyconversions",
    srcs = ["legacyconversions.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":safestyle",
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
    lenient = True,
    deps = [
        ":trustedresourceurl",
        "//closure/goog/asserts",
        "//closure/goog/fs:url",
        "//closure/goog/string:const",
        "//closure/goog/string:internal",
        "//closure/goog/string:typedstring",
    ],
)

closure_js_library(
    name = "safeurl_test_vectors",
    testonly = 1,
    srcs = [
        "javascript_url_test_vectors.js",
        "safeurl_test_vectors.js",
    ],
    lenient = True,
)

closure_js_library(
    name = "testing",
    testonly = True,
    srcs = ["testing.js"],
    lenient = True,
    deps = [
        ":safehtml",
        ":safescript",
        ":safestyle",
        ":safestylesheet",
        ":safeurl",
        ":trustedresourceurl",
        "//closure/goog/testing:mockmatchers",
    ],
)

closure_js_library(
    name = "textextractor",
    srcs = ["textextractor.js"],
    lenient = True,
    deps = [
        "//closure/goog/dom:tagname",
        "//closure/goog/html/sanitizer:htmlsanitizer",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "trustedresourceurl",
    srcs = ["trustedresourceurl.js"],
    lenient = True,
    deps = [
        ":safescript",
        ":trustedtypes",
        "//closure/goog/asserts",
        "//closure/goog/fs:blob",
        "//closure/goog/fs:url",
        "//closure/goog/string:const",
        "//closure/goog/string:typedstring",
    ],
)

closure_js_library(
    name = "trustedtypes",
    srcs = ["trustedtypes.js"],
    lenient = True,
    deps = ["//closure/goog/memoize"],
)

closure_js_library(
    name = "uncheckedconversions",
    srcs = ["uncheckedconversions.js"],
    lenient = True,
    deps = [
        ":safehtml",
        ":safescript",
        ":safestyle",
        ":safestylesheet",
        ":safeurl",
        ":trustedresourceurl",
        "//closure/goog/asserts",
        "//closure/goog/string:const",
        "//closure/goog/string:internal",
    ],
)

closure_js_library(
    name = "utils",
    srcs = ["utils.js"],
    lenient = True,
    deps = ["//closure/goog/string"],
)
