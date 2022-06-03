load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "actioneventwrapper",
    srcs = ["actioneventwrapper.js"],
    deps = [
        ":eventhandler",
        ":events",
        ":eventtype",
        ":eventwrapper",
        ":keycodes",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "actionhandler",
    srcs = ["actionhandler.js"],
    deps = [
        ":browserevent",
        ":events",
        ":eventtarget",
        ":eventtype",
        ":keycodes",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "browserevent",
    srcs = ["browserevent.js"],
    deps = [
        ":browserfeature",
        ":event",
        ":eventtype",
        "//closure/goog/debug",
        "//closure/goog/reflect",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "browserfeature",
    srcs = ["browserfeature.js"],
    deps = ["//closure/goog/useragent"],
)

closure_js_library(
    name = "event",
    srcs = [
        "event.js",
        "eventlike.js",
    ],
    deps = [
        ":eventid",
        "//closure/goog/disposable",
    ],
)

alias(
    name = "eventhandler",
    actual = ":events",
)

closure_js_library(
    name = "eventid",
    srcs = ["eventid.js"],
)

closure_js_library(
    name = "events",
    srcs = [
        "eventhandler.js",
        "events.js",
        "eventtarget.js",
        "eventwrapper.js",
    ],
    deps = [
        ":browserevent",
        ":browserfeature",
        ":event",
        ":eventid",
        ":listenable",
        ":listener",
        ":listenermap",
        "//closure/goog/asserts",
        "//closure/goog/debug:entrypointregistry",
        "//closure/goog/debug:errorhandler",
        "//closure/goog/disposable",
        "//closure/goog/object",
    ],
)

alias(
    name = "eventtarget",
    actual = ":events",
)

closure_js_library(
    name = "eventtargettester",
    testonly = 1,
    srcs = ["eventtargettester.js"],
    deps = [
        ":event",
        ":eventtarget",
        ":listenable",
        "//closure/goog/array",
        "//closure/goog/disposable",
        "//closure/goog/testing:asserts",
        "//closure/goog/testing:recordfunction",
    ],
)

closure_js_library(
    name = "eventtype",
    srcs = ["eventtype.js"],
    deps = [
        ":browserfeature",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "eventwrapper",
    actual = ":events",
)

closure_js_library(
    name = "filedrophandler",
    srcs = ["filedrophandler.js"],
    deps = [
        ":browserevent",
        ":eventhandler",
        ":eventtarget",
        ":eventtype",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/log",
    ],
)

closure_js_library(
    name = "focushandler",
    srcs = ["focushandler.js"],
    deps = [
        ":browserevent",
        ":events",
        ":eventtarget",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "imehandler",
    srcs = ["imehandler.js"],
    deps = [
        ":browserevent",
        ":event",
        ":eventhandler",
        ":eventtarget",
        ":eventtype",
        ":keycodes",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "inputhandler",
    srcs = ["inputhandler.js"],
    deps = [
        ":browserevent",
        ":eventhandler",
        ":eventtarget",
        ":keycodes",
        "//closure/goog/dom:tagname",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "keycodes",
    srcs = ["keycodes.js"],
    deps = [
        ":browserevent",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "keyhandler",
    srcs = [
        "keyevent.js",
        "keyhandler.js",
    ],
    deps = [
        ":browserevent",
        ":events",
        ":eventtarget",
        ":eventtype",
        ":keycodes",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "keynames",
    srcs = ["keynames.js"],
)

closure_js_library(
    name = "keys",
    srcs = ["keys.js"],
)

closure_js_library(
    name = "listenable",
    srcs = [
        "listenable.js",
        "listenablekey.js",
    ],
    deps = [
        ":event",
        ":eventid",
    ],
)

closure_js_library(
    name = "listener",
    srcs = ["listener.js"],
    deps = [":listenable"],
)

closure_js_library(
    name = "listenermap",
    srcs = ["listenermap.js"],
    deps = [
        ":eventid",
        ":listenable",
        ":listener",
        "//closure/goog/array",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "mousewheelhandler",
    srcs = ["mousewheelhandler.js"],
    deps = [
        ":browserevent",
        ":events",
        ":eventtarget",
        "//closure/goog/dom",
        "//closure/goog/math",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "onlinehandler",
    srcs = ["onlinehandler.js"],
    deps = [
        ":eventhandler",
        ":eventtarget",
        ":eventtype",
        "//closure/goog/net:networkstatusmonitor",
    ],
)

closure_js_library(
    name = "pastehandler",
    srcs = ["pastehandler.js"],
    deps = [
        ":browserevent",
        ":eventhandler",
        ":eventtarget",
        ":eventtype",
        ":keycodes",
        "//closure/goog/async:conditionaldelay",
        "//closure/goog/log",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "wheelevent",
    srcs = ["wheelevent.js"],
    deps = [
        ":browserevent",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "wheelhandler",
    srcs = ["wheelhandler.js"],
    deps = [
        ":browserevent",
        ":events",
        ":eventtarget",
        ":wheelevent",
        "//closure/goog/dom",
        "//closure/goog/style",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
        "//closure/goog/useragent:product_isversion",
    ],
)
