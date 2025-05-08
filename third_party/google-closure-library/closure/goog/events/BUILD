load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "actioneventwrapper",
    srcs = ["actioneventwrapper.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
)

closure_js_library(
    name = "event",
    srcs = [
        "event.js",
        "eventlike.js",
    ],
    lenient = True,
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
    lenient = True,
)

closure_js_library(
    name = "events",
    srcs = [
        "eventhandler.js",
        "events.js",
        "eventtarget.js",
        "eventwrapper.js",
    ],
    lenient = True,
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
    lenient = True,
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
    srcs = [
        "eventtype.js",
        "mouseasmouseeventtype.js",
        "mouseevents.js",
        "pointerasmouseeventtype.js",
        "pointerastoucheventtype.js",
        "pointerfallbackeventtype.js",
        "pointertouchfallbackeventtype.js",
    ],
    lenient = True,
    deps = [
        ":eventtypehelpers",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "eventtypehelpers",
    srcs = ["eventtypehelpers.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":browserevent",
        ":eventhandler",
        ":keycodes",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "keycodes",
    srcs = ["keycodes.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
)

closure_js_library(
    name = "keys",
    srcs = ["keys.js"],
    lenient = True,
)

closure_js_library(
    name = "listenable",
    srcs = [
        "listenable.js",
        "listenablekey.js",
    ],
    lenient = True,
    deps = [
        ":event",
        ":eventid",
    ],
)

closure_js_library(
    name = "listener",
    srcs = ["listener.js"],
    lenient = True,
    deps = [":listenable"],
)

closure_js_library(
    name = "listenermap",
    srcs = ["listenermap.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":browserevent",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "wheelhandler",
    srcs = ["wheelhandler.js"],
    lenient = True,
    deps = [
        ":browserevent",
        ":events",
        ":wheelevent",
        "//closure/goog/dom",
        "//closure/goog/style",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
    ],
)
