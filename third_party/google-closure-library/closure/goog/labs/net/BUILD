load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "net",
    srcs = [],
    exports = [
        ":image",
        ":webchannel",
        ":webchanneltransport",
        ":webchanneltransportfactory",
        ":xhr",
        "//closure/goog/labs/net/webchannel",
    ],
)

closure_js_library(
    name = "image",
    srcs = ["image.js"],
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/dom:safe",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/html:safeurl",
        "//closure/goog/net:eventtype",
        "//closure/goog/promise",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "webchannel",
    srcs = ["webchannel.js"],
    deps = [
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:listenable",
        "//closure/goog/net:xmlhttpfactory",
    ],
)

closure_js_library(
    name = "webchanneltransport",
    srcs = ["webchanneltransport.js"],
    deps = [":webchannel"],
)

closure_js_library(
    name = "webchanneltransportfactory",
    srcs = ["webchanneltransportfactory.js"],
    deps = [
        ":webchanneltransport",
        "//closure/goog/labs/net/webchannel:webchannelbasetransport",
    ],
)

closure_js_library(
    name = "xhr",
    srcs = ["xhr.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/debug:error",
        "//closure/goog/net:httpstatus",
        "//closure/goog/net:xhrlike",
        "//closure/goog/net:xmlhttp",
        "//closure/goog/net:xmlhttpfactory",
        "//closure/goog/object",
        "//closure/goog/promise",
        "//closure/goog/string",
        "//closure/goog/uri:utils",
        "//closure/goog/useragent",
    ],
)
