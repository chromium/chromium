load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "useragent",
    srcs = [],
    exports = [
        ":browser",
        ":device",
        ":engine",
        ":extra",
        ":platform",
        ":verifier",
    ],
)

closure_js_library(
    name = "browser",
    srcs = ["browser.js"],
    deps = [
        ":util",
        "//closure/goog/array",
        "//closure/goog/object",
        "//closure/goog/string:internal",
    ],
)

closure_js_library(
    name = "device",
    srcs = ["device.js"],
    deps = [":util"],
)

closure_js_library(
    name = "engine",
    srcs = ["engine.js"],
    deps = [
        ":util",
        "//closure/goog/array",
        "//closure/goog/string:internal",
    ],
)

closure_js_library(
    name = "extra",
    srcs = ["extra.js"],
    deps = [
        ":browser",
        ":platform",
    ],
)

closure_js_library(
    name = "platform",
    srcs = ["platform.js"],
    deps = [
        ":util",
        "//closure/goog/string:internal",
    ],
)

closure_js_library(
    name = "test_agents",
    testonly = 1,
    srcs = ["test_agents.js"],
    deps = ["//closure/goog/debug:deepfreeze"],
)

closure_js_library(
    name = "util",
    srcs = [
        "useragent.js",
        "util.js",
    ],
    deps = ["//closure/goog/string:internal"],
)

closure_js_library(
    name = "verifier",
    srcs = ["verifier.js"],
)
