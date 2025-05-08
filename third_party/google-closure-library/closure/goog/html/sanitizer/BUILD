load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "html_test_vectors_js",
    srcs = ["html_test_vectors.js"],
    lenient = True,
)

closure_js_library(
    name = "attributeallowlists",
    srcs = ["attributeallowlists.js"],
    lenient = True,
)

closure_js_library(
    name = "csspropertysanitizer",
    srcs = ["csspropertysanitizer.js"],
    lenient = True,
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/html:safeurl",
        "//closure/goog/object",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "csssanitizer",
    srcs = ["csssanitizer.js"],
    lenient = True,
    deps = [
        ":csspropertysanitizer",
        ":noclobber",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:cssspecificity",
        "//closure/goog/html:safestyle",
        "//closure/goog/html:safestylesheet",
        "//closure/goog/html:safeurl",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/string:const",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "elementweakmap",
    srcs = ["elementweakmap.js"],
    lenient = True,
    deps = [":noclobber"],
)

closure_js_library(
    name = "htmlsanitizer",
    srcs = ["htmlsanitizer.js"],
    lenient = True,
    deps = [
        ":attributeallowlists",
        ":csssanitizer",
        ":noclobber",
        ":safedomtreeprocessor",
        ":tagblacklist",
        ":tagwhitelist",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/functions",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safestyle",
        "//closure/goog/html:safestylesheet",
        "//closure/goog/html:safeurl",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/string:const",
    ],
)

closure_js_library(
    name = "noclobber",
    srcs = ["noclobber.js"],
    lenient = True,
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/dom:nodetype",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "safedomtreeprocessor",
    srcs = ["safedomtreeprocessor.js"],
    lenient = True,
    deps = [
        ":elementweakmap",
        ":noclobber",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/log",
        "//closure/goog/string:const",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "tagblacklist",
    srcs = ["tagblacklist.js"],
    lenient = True,
)

closure_js_library(
    name = "tagwhitelist",
    srcs = ["tagwhitelist.js"],
    lenient = True,
)

closure_js_library(
    name = "unsafe",
    srcs = ["unsafe.js"],
    lenient = True,
    deps = [
        ":htmlsanitizer",
        "//closure/goog/asserts",
        "//closure/goog/string",
        "//closure/goog/string:const",
    ],
)
