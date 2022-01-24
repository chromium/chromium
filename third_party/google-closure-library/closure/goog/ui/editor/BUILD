load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "abstractdialog",
    srcs = ["abstractdialog.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
        "//closure/goog/string",
        "//closure/goog/ui:dialog",
        "//closure/goog/ui:popupbase",
    ],
)

closure_js_library(
    name = "bubble",
    srcs = ["bubble.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/dom:viewportsizemonitor",
        "//closure/goog/editor:style",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/functions",
        "//closure/goog/html:safehtml",
        "//closure/goog/log",
        "//closure/goog/math:box",
        "//closure/goog/object",
        "//closure/goog/positioning",
        "//closure/goog/string",
        "//closure/goog/string:const",
        "//closure/goog/style",
        "//closure/goog/ui:component",
        "//closure/goog/ui:popupbase",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "defaulttoolbar",
    srcs = ["defaulttoolbar.js"],
    deps = [
        ":messages",
        ":toolbarfactory",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:command",
        "//closure/goog/style",
        "//closure/goog/ui:button",
        "//closure/goog/ui:buttonrenderer",
        "//closure/goog/ui:colormenubuttonrenderer",
        "//closure/goog/ui:control",
        "//closure/goog/ui:controlcontent",
        "//closure/goog/ui:menubuttonrenderer",
        "//closure/goog/ui:menuitem",
        "//closure/goog/ui:select",
        "//closure/goog/ui:toolbar",
        "//closure/goog/ui:toolbarcolormenubutton",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "linkdialog",
    srcs = ["linkdialog.js"],
    deps = [
        ":abstractdialog",
        ":messages",
        ":tabpane",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/editor:browserfeature",
        "//closure/goog/editor:focus",
        "//closure/goog/editor:link",
        "//closure/goog/editor:node",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:inputhandler",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safehtmlformatter",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/ui:button",
        "//closure/goog/ui:component",
        "//closure/goog/ui:linkbuttonrenderer",
        "//closure/goog/ui:tab",
        "//closure/goog/useragent",
        "//closure/goog/window",
    ],
)

closure_js_library(
    name = "messages",
    srcs = ["messages.js"],
    deps = [
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safehtmlformatter",
    ],
)

closure_js_library(
    name = "tabpane",
    srcs = ["tabpane.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/style",
        "//closure/goog/ui:component",
        "//closure/goog/ui:control",
        "//closure/goog/ui:tab",
        "//closure/goog/ui:tabbar",
    ],
)

closure_js_library(
    name = "toolbarcontroller",
    srcs = ["toolbarcontroller.js"],
    deps = [
        "//closure/goog/editor:field",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/ui:component",
    ],
)

closure_js_library(
    name = "toolbarfactory",
    srcs = ["toolbarfactory.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/ui:button",
        "//closure/goog/ui:buttonrenderer",
        "//closure/goog/ui:colormenubutton",
        "//closure/goog/ui:colormenubuttonrenderer",
        "//closure/goog/ui:component",
        "//closure/goog/ui:container",
        "//closure/goog/ui:control",
        "//closure/goog/ui:controlcontent",
        "//closure/goog/ui:menubutton",
        "//closure/goog/ui:menubuttonrenderer",
        "//closure/goog/ui:option",
        "//closure/goog/ui:select",
        "//closure/goog/ui:toolbar",
        "//closure/goog/ui:toolbarbutton",
        "//closure/goog/ui:toolbarcolormenubutton",
        "//closure/goog/ui:toolbarmenubutton",
        "//closure/goog/ui:toolbarrenderer",
        "//closure/goog/ui:toolbarselect",
        "//closure/goog/useragent",
    ],
)
