load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "ac",
    srcs = ["ac.js"],
    deps = [
        ":arraymatcher",
        ":autocomplete",
        ":inputhandler",
        ":renderer",
    ],
)

closure_js_library(
    name = "arraymatcher",
    srcs = ["arraymatcher.js"],
    deps = ["//closure/goog/string"],
)

closure_js_library(
    name = "autocomplete",
    srcs = ["autocomplete.js"],
    deps = [
        ":inputhandler",
        ":renderoptions",
        "//closure/goog/asserts",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "cachingmatcher",
    srcs = ["cachingmatcher.js"],
    deps = [
        ":arraymatcher",
        ":renderoptions",
        "//closure/goog/array",
        "//closure/goog/async:throttle",
    ],
)

closure_js_library(
    name = "inputhandler",
    srcs = ["inputhandler.js"],
    deps = [
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:selection",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/string",
        "//closure/goog/timer",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "remote",
    srcs = ["remote.js"],
    deps = [
        ":autocomplete",
        ":inputhandler",
        ":remotearraymatcher",
        ":renderer",
        "//closure/goog/structs:map",
    ],
)

closure_js_library(
    name = "remotearraymatcher",
    srcs = ["remotearraymatcher.js"],
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/net:eventtype",
        "//closure/goog/net:xhrio",
        "//closure/goog/net:xmlhttpfactory",
        "//closure/goog/structs:map",
        "//closure/goog/uri",
    ],
)

closure_js_library(
    name = "renderer",
    srcs = ["renderer.js"],
    deps = [
        ":autocomplete",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/fx:animation",
        "//closure/goog/fx:dom",
        "//closure/goog/positioning",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/ui:idgenerator",
    ],
)

closure_js_library(
    name = "renderoptions",
    srcs = ["renderoptions.js"],
)

closure_js_library(
    name = "richinputhandler",
    srcs = ["richinputhandler.js"],
    deps = [":inputhandler"],
)

closure_js_library(
    name = "richremote",
    srcs = ["richremote.js"],
    deps = [
        ":autocomplete",
        ":remote",
        ":renderer",
        ":richinputhandler",
        ":richremotearraymatcher",
    ],
)

closure_js_library(
    name = "richremotearraymatcher",
    srcs = ["richremotearraymatcher.js"],
    deps = [
        ":remotearraymatcher",
        "//closure/goog/dom",
    ],
)
