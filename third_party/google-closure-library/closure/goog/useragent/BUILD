load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "adobereader",
    srcs = ["adobereader.js"],
    lenient = True,
    deps = [
        ":useragent",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "iphoto",
    srcs = ["iphoto.js"],
    lenient = True,
    deps = [
        ":useragent",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "jscript",
    srcs = ["jscript.js"],
    lenient = True,
    deps = ["//closure/goog/string"],
)

closure_js_library(
    name = "keyboard",
    srcs = ["keyboard.js"],
    lenient = True,
    deps = ["//closure/goog/labs/useragent:platform"],
)

closure_js_library(
    name = "platform",
    srcs = ["platform.js"],
    lenient = True,
    deps = [
        ":useragent",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "product",
    srcs = ["product.js"],
    lenient = True,
    deps = [
        ":useragent",
        "//closure/goog/labs/useragent:browser",
        "//closure/goog/labs/useragent:platform",
    ],
)

closure_js_library(
    name = "product_isversion",
    srcs = ["product_isversion.js"],
    lenient = True,
    deps = [
        ":product",
        ":useragent",
        "//closure/goog/labs/useragent:platform",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "useragent",
    srcs = ["useragent.js"],
    lenient = True,
    deps = [
        "//closure/goog/labs/useragent:browser",
        "//closure/goog/labs/useragent:engine",
        "//closure/goog/labs/useragent:platform",
        "//closure/goog/labs/useragent:util",
        "//closure/goog/reflect",
        "//closure/goog/string:internal",
    ],
)

closure_js_library(
    name = "useragenttestutil",
    testonly = 1,
    srcs = ["useragenttestutil.js"],
    lenient = True,
    deps = [
        ":keyboard",
        ":platform",
        ":product",
        ":product_isversion",
        ":useragent",
        "//closure/goog/labs/useragent:browser",
        "//closure/goog/labs/useragent:engine",
        "//closure/goog/labs/useragent:platform",
        "//closure/goog/object",
    ],
)
