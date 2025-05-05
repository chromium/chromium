load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "abstractmoduleloader",
    srcs = ["abstractmoduleloader.js"],
    lenient = True,
    deps = [
        ":module",
        ":moduleinfo",
    ],
)

closure_js_library(
    name = "basemodule",
    srcs = ["basemodule.js"],
    lenient = True,
    deps = [
        ":module",
        "//closure/goog/disposable",
    ],
)

closure_js_library(
    name = "loader",
    srcs = ["loader.js"],
    lenient = True,
    deps = [
        ":module",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:legacyconversions",
        "//closure/goog/object",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "module",
    srcs = ["module.js"],
    lenient = True,
)

closure_js_library(
    name = "moduleinfo",
    srcs = [
        "moduleinfo.js",
    ],
    lenient = True,
    deps = [
        ":basemodule",
        ":module",
        ":moduleloadcallback",
        ":moduleloadfailure",
        "//closure/goog/async:throwexception",
        "//closure/goog/disposable",
        "//closure/goog/functions",
        "//closure/goog/html:trustedresourceurl",
    ],
)

closure_js_library(
    name = "moduleloadfailure",
    srcs = [
        "moduleloadfailure.js",
    ],
    lenient = True,
)

closure_js_library(
    name = "moduleloadcallback",
    srcs = ["moduleloadcallback.js"],
    lenient = True,
    deps = [
        ":module",
        "//closure/goog/debug:entrypointregistry",
    ],
)

closure_js_library(
    name = "moduleloader",
    srcs = ["moduleloader.js"],
    lenient = True,
    deps = [
        ":abstractmoduleloader",
        ":moduleinfo",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventid",
        "//closure/goog/events:eventtarget",
        "//closure/goog/functions",
        "//closure/goog/html:legacyconversions",
        "//closure/goog/html:safescript",
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/labs/useragent:browser",
        "//closure/goog/log",
        "//closure/goog/net:bulkloader",
        "//closure/goog/net:eventtype",
        "//closure/goog/net:jsloader",
        "//closure/goog/timer",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "modulemanager",
    srcs = ["modulemanager.js"],
    lenient = True,
    deps = [
        ":abstractmoduleloader",
        ":module",
        ":moduleinfo",
        ":moduleloadcallback",
        ":moduleloadfailure",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/debug:error",
        "//closure/goog/disposable",
        "//closure/goog/disposable:idisposable",
        "//closure/goog/loader:abstractmodulemanager",
        "//closure/goog/loader:activemodulemanager",
        "//closure/goog/log",
        "//closure/goog/object",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)
