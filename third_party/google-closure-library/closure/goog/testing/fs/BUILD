load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "blob",
    srcs = ["blob.js"],
    deps = [
        "//closure/goog/crypt",
        "//closure/goog/crypt:base64",
    ],
)

closure_js_library(
    name = "filesystem",
    srcs = [
        "entry.js",
        "filesystem.js",
        "filewriter.js",
    ],
    deps = [
        ":blob",
        ":file",
        ":progressevent",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/events:eventtarget",
        "//closure/goog/fs:entryimpl",
        "//closure/goog/fs:error",
        "//closure/goog/fs:filesaver",
        "//closure/goog/fs:filesystem",
        "//closure/goog/functions",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/timer",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "file",
    srcs = ["file.js"],
    deps = [":blob"],
)

closure_js_library(
    name = "filereader",
    srcs = ["filereader.js"],
    deps = [
        ":blob",
        ":progressevent",
        "//closure/goog/events:eventtarget",
        "//closure/goog/fs:error",
        "//closure/goog/fs:filereader",
        "//closure/goog/timer",
    ],
)

alias(
    name = "entry",
    actual = ":filesystem",
)

alias(
    name = "filewriter",
    actual = ":filesystem",
)

closure_js_library(
    name = "fs",
    srcs = ["fs.js"],
    deps = [
        ":blob",
        ":filesystem",
        "//closure/goog/array",
        "//closure/goog/fs",
        "//closure/goog/fs:url",
        "//closure/goog/testing:propertyreplacer",
        "//closure/goog/timer",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "progressevent",
    srcs = ["progressevent.js"],
    deps = [
        "//closure/goog/events:event",
        "//closure/goog/fs:filereader",
        "//closure/goog/fs:filesaver",
    ],
)
