load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "blob",
    srcs = ["blob.js"],
)

alias(
    name = "entry",
    actual = ":filesystem",
)

closure_js_library(
    name = "entryimpl",
    srcs = ["entryimpl.js"],
    deps = [
        ":error",
        ":filesystem",
        ":filewriter",
        "//closure/goog/array",
        "//closure/goog/functions",
        "//closure/goog/string",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "error",
    srcs = ["error.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/debug:error",
        "//closure/goog/object",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "filereader",
    srcs = ["filereader.js"],
    deps = [
        ":error",
        ":progressevent",
        "//closure/goog/events:eventtarget",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "filesaver",
    srcs = ["filesaver.js"],
    deps = [
        ":error",
        ":progressevent",
        "//closure/goog/events:eventtarget",
    ],
)

closure_js_library(
    name = "filesystem",
    srcs = [
        "entry.js",
        "filesystem.js",
    ],
    deps = [
        ":error",
        ":filewriter",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "filesystemimpl",
    srcs = ["filesystemimpl.js"],
    deps = [
        ":entryimpl",
        ":filesystem",
    ],
)

closure_js_library(
    name = "filewriter",
    srcs = ["filewriter.js"],
    deps = [
        ":error",
        ":filesaver",
    ],
)

closure_js_library(
    name = "fs",
    srcs = ["fs.js"],
    deps = [
        ":error",
        ":filereader",
        ":filesystemimpl",
        ":url",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "progressevent",
    srcs = ["progressevent.js"],
    deps = ["//closure/goog/events:event"],
)

closure_js_library(
    name = "url",
    srcs = ["url.js"],
)
