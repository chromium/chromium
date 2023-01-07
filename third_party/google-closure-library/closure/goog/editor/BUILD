load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "browserfeature",
    srcs = ["browserfeature.js"],
    deps = [
        ":defines",
        "//closure/goog/labs/useragent:browser",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
        "//closure/goog/useragent:product_isversion",
    ],
)

closure_js_library(
    name = "clicktoeditwrapper",
    srcs = ["clicktoeditwrapper.js"],
    deps = [
        ":browserfeature",
        ":command",
        ":field",
        ":range",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:range",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "command",
    srcs = ["command.js"],
)

closure_js_library(
    name = "contenteditablefield",
    srcs = ["contenteditablefield.js"],
    deps = [
        ":field",
        "//closure/goog/asserts",
        "//closure/goog/log",
    ],
)

closure_js_library(
    name = "defines",
    srcs = ["defines.js"],
)

closure_js_library(
    name = "field",
    srcs = [
        "field.js",
        "plugin_impl.js",
    ],
    deps = [
        ":browserfeature",
        ":command",
        ":icontent",
        ":node",
        ":range",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/async:delay",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:range",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/functions",
        "//closure/goog/html:legacyconversions",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safestylesheet",
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/log",
        "//closure/goog/object",
        "//closure/goog/reflect",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "focus",
    srcs = ["focus.js"],
    deps = ["//closure/goog/dom:selection"],
)

closure_js_library(
    name = "icontent",
    srcs = ["icontent.js"],
    deps = [
        ":browserfeature",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/html:legacyconversions",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "link",
    srcs = ["link.js"],
    deps = [
        ":browserfeature",
        ":command",
        ":field",
        ":node",
        ":range",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:range",
        "//closure/goog/dom:tagname",
        "//closure/goog/string",
        "//closure/goog/uri:utils",
    ],
)

closure_js_library(
    name = "node",
    srcs = ["node.js"],
    deps = [
        "//closure/goog/asserts:dom",
        "//closure/goog/dom",
        "//closure/goog/dom:iter",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:legacyconversions",
        "//closure/goog/iter",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "plugin",
    srcs = ["plugin.js"],
    deps = [
        ":field",
        ":plugin_impl",
    ],
)

alias(
    name = "plugin_impl",
    actual = ":field",
)

closure_js_library(
    name = "range",
    srcs = ["range.js"],
    deps = [
        ":node",
        ":style",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:range",
        "//closure/goog/dom:rangeendpoint",
        "//closure/goog/dom:savedcaretrange",
        "//closure/goog/dom:tagname",
        "//closure/goog/iter",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "seamlessfield",
    srcs = ["seamlessfield.js"],
    deps = [
        ":browserfeature",
        ":field",
        ":icontent",
        ":node",
        "//closure/goog/cssom/iframe:style",
        "//closure/goog/dom",
        "//closure/goog/dom:range",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:eventtype",
        "//closure/goog/html:safehtml",
        "//closure/goog/log",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "style",
    srcs = ["style.js"],
    deps = [
        ":browserfeature",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/object",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "table",
    srcs = [
        "table.js",
        "tablecell.js",
        "tablerow.js",
    ],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/log",
        "//closure/goog/string",
        "//closure/goog/style",
    ],
)
