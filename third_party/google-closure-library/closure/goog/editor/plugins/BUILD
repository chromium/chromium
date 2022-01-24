load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "abstractbubbleplugin",
    srcs = ["abstractbubbleplugin.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:range",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:plugin",
        "//closure/goog/editor:style",
        "//closure/goog/events",
        "//closure/goog/events:actioneventwrapper",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/functions",
        "//closure/goog/string",
        "//closure/goog/ui:component",
        "//closure/goog/ui/editor:bubble",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "abstractdialogplugin",
    srcs = ["abstractdialogplugin.js"],
    deps = [
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:range",
        "//closure/goog/editor:field",
        "//closure/goog/editor:plugin",
        "//closure/goog/editor:range",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/ui/editor:abstractdialog",
    ],
)

closure_js_library(
    name = "abstracttabhandler",
    srcs = ["abstracttabhandler.js"],
    deps = [
        "//closure/goog/editor:plugin",
        "//closure/goog/events:event",
        "//closure/goog/events:keycodes",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "basictextformatter",
    srcs = ["basictextformatter.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:range",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:browserfeature",
        "//closure/goog/editor:command",
        "//closure/goog/editor:link",
        "//closure/goog/editor:node",
        "//closure/goog/editor:plugin",
        "//closure/goog/editor:range",
        "//closure/goog/editor:style",
        "//closure/goog/iter",
        "//closure/goog/log",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/ui/editor:messages",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "blockquote",
    srcs = ["blockquote.js"],
    deps = [
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:browserfeature",
        "//closure/goog/editor:command",
        "//closure/goog/editor:node",
        "//closure/goog/editor:plugin",
        "//closure/goog/functions",
        "//closure/goog/log",
    ],
)

closure_js_library(
    name = "emoticons",
    srcs = ["emoticons.js"],
    deps = [
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:plugin",
        "//closure/goog/editor:range",
        "//closure/goog/functions",
        "//closure/goog/ui/emoji",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "enterhandler",
    srcs = ["enterhandler.js"],
    deps = [
        ":blockquote",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:nodeoffset",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:range",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:browserfeature",
        "//closure/goog/editor:node",
        "//closure/goog/editor:plugin",
        "//closure/goog/editor:range",
        "//closure/goog/editor:style",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:keycodes",
        "//closure/goog/functions",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "firststrong",
    srcs = ["firststrong.js"],
    deps = [
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagiterator",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:command",
        "//closure/goog/editor:field",
        "//closure/goog/editor:node",
        "//closure/goog/editor:plugin",
        "//closure/goog/editor:range",
        "//closure/goog/i18n:bidi",
        "//closure/goog/i18n:uchar",
        "//closure/goog/iter",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "headerformatter",
    srcs = ["headerformatter.js"],
    deps = [
        "//closure/goog/editor:command",
        "//closure/goog/editor:plugin",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "linkbubble",
    srcs = ["linkbubble.js"],
    deps = [
        ":abstractbubbleplugin",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:range",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:command",
        "//closure/goog/editor:link",
        "//closure/goog/events:browserevent",
        "//closure/goog/functions",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/ui/editor:messages",
        "//closure/goog/uri:utils",
        "//closure/goog/window",
    ],
)

closure_js_library(
    name = "linkdialogplugin",
    srcs = ["linkdialogplugin.js"],
    deps = [
        ":abstractdialogplugin",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/editor:command",
        "//closure/goog/editor:link",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/functions",
        "//closure/goog/html:safehtml",
        "//closure/goog/ui/editor:abstractdialog",
        "//closure/goog/ui/editor:linkdialog",
        "//closure/goog/uri:utils",
    ],
)

closure_js_library(
    name = "linkshortcutplugin",
    srcs = ["linkshortcutplugin.js"],
    deps = [
        "//closure/goog/editor:command",
        "//closure/goog/editor:link",
        "//closure/goog/editor:plugin",
    ],
)

closure_js_library(
    name = "listtabhandler",
    srcs = ["listtabhandler.js"],
    deps = [
        ":abstracttabhandler",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:command",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "loremipsum",
    srcs = ["loremipsum.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/editor:command",
        "//closure/goog/editor:field",
        "//closure/goog/editor:node",
        "//closure/goog/editor:plugin",
        "//closure/goog/functions",
        "//closure/goog/html:safehtml",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "removeformatting",
    srcs = ["removeformatting.js"],
    deps = [
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:range",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:browserfeature",
        "//closure/goog/editor:node",
        "//closure/goog/editor:plugin",
        "//closure/goog/editor:range",
        "//closure/goog/html:legacyconversions",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "spacestabhandler",
    srcs = ["spacestabhandler.js"],
    deps = [
        ":abstracttabhandler",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:range",
    ],
)

closure_js_library(
    name = "tableeditor",
    srcs = ["tableeditor.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:range",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:node",
        "//closure/goog/editor:plugin",
        "//closure/goog/editor:range",
        "//closure/goog/editor:table",
        "//closure/goog/object",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "tagonenterhandler",
    srcs = ["tagonenterhandler.js"],
    deps = [
        ":enterhandler",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:range",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:command",
        "//closure/goog/editor:node",
        "//closure/goog/editor:range",
        "//closure/goog/editor:style",
        "//closure/goog/events:keycodes",
        "//closure/goog/functions",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "undoredo",
    srcs = ["undoredo.js"],
    deps = [
        ":undoredomanager",
        ":undoredostate",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:nodeoffset",
        "//closure/goog/dom:range",
        "//closure/goog/editor:browserfeature",
        "//closure/goog/editor:command",
        "//closure/goog/editor:field",
        "//closure/goog/editor:node",
        "//closure/goog/editor:plugin",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/log",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "undoredomanager",
    srcs = ["undoredomanager.js"],
    deps = [
        ":undoredostate",
        "//closure/goog/events",
        "//closure/goog/events:eventtarget",
    ],
)

closure_js_library(
    name = "undoredostate",
    srcs = ["undoredostate.js"],
    deps = ["//closure/goog/events:eventtarget"],
)
