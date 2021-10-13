load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "abstractspellchecker",
    srcs = ["abstractspellchecker.js"],
    deps = [
        ":component",
        ":menuitem",
        ":menuseparator",
        ":popupmenu",
        "//closure/goog/a11y/aria",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:selection",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:coordinate",
        "//closure/goog/spell:spellcheck",
        "//closure/goog/structs:set",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "activitymonitor",
    srcs = ["activitymonitor.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "advancedtooltip",
    srcs = ["advancedtooltip.js"],
    deps = [
        ":tooltip",
        "//closure/goog/dom",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:box",
        "//closure/goog/math:coordinate",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "animatedzippy",
    srcs = ["animatedzippy.js"],
    deps = [
        ":zippy",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/fx:animation",
        "//closure/goog/fx:easing",
        "//closure/goog/fx:transition",
    ],
)

closure_js_library(
    name = "attachablemenu",
    srcs = ["attachablemenu.js"],
    deps = [
        ":itemevent",
        ":menubase",
        ":popupbase",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/events:event",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "bidiinput",
    srcs = ["bidiinput.js"],
    deps = [
        ":component",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:inputhandler",
        "//closure/goog/i18n:bidi",
    ],
)

closure_js_library(
    name = "bubble",
    srcs = ["bubble.js"],
    deps = [
        ":component",
        ":popup",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/events",
        "//closure/goog/events:eventtype",
        "//closure/goog/html:safehtml",
        "//closure/goog/math:box",
        "//closure/goog/positioning",
        "//closure/goog/positioning:absoluteposition",
        "//closure/goog/positioning:abstractposition",
        "//closure/goog/positioning:anchoredposition",
        "//closure/goog/string:const",
        "//closure/goog/style",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "button",
    srcs = ["button.js"],
    deps = [
        ":buttonrenderer",
        ":buttonside",
        ":component",
        ":control",
        ":controlcontent",
        ":nativebuttonrenderer",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
    ],
)

closure_js_library(
    name = "buttonrenderer",
    srcs = ["buttonrenderer.js"],
    deps = [
        ":buttonside",
        ":component",
        ":controlrenderer",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "buttonside",
    srcs = ["buttonside.js"],
)

closure_js_library(
    name = "charcounter",
    srcs = ["charcounter.js"],
    deps = [
        "//closure/goog/dom",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:inputhandler",
    ],
)

closure_js_library(
    name = "charpicker",
    srcs = ["charpicker.js"],
    deps = [
        ":button",
        ":component",
        ":containerscroller",
        ":flatbuttonrenderer",
        ":hovercard",
        ":labelinput",
        ":menu",
        ":menubutton",
        ":menuitem",
        ":tooltip",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:inputhandler",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/i18n:charlistdecompressor",
        "//closure/goog/i18n:charpickerdata",
        "//closure/goog/i18n:uchar",
        "//closure/goog/i18n/uchar:namefetcher",
        "//closure/goog/structs:set",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "checkbox",
    srcs = ["checkbox.js"],
    deps = [
        ":checkboxrenderer",
        ":component",
        ":control",
        ":registry",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/dom",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "checkboxmenuitem",
    srcs = ["checkboxmenuitem.js"],
    deps = [
        ":controlcontent",
        ":menuitem",
        ":registry",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "checkboxrenderer",
    srcs = ["checkboxrenderer.js"],
    deps = [
        ":control",
        ":controlrenderer",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "colormenubutton",
    srcs = ["colormenubutton.js"],
    deps = [
        ":colormenubuttonrenderer",
        ":colorpalette",
        ":component",
        ":control",
        ":controlcontent",
        ":menu",
        ":menubutton",
        ":menubuttonrenderer",
        ":registry",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/events:event",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "colormenubuttonrenderer",
    srcs = ["colormenubuttonrenderer.js"],
    deps = [
        ":control",
        ":controlcontent",
        ":menubuttonrenderer",
        "//closure/goog/asserts",
        "//closure/goog/color",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "colorpalette",
    srcs = ["colorpalette.js"],
    deps = [
        ":palette",
        ":paletterenderer",
        "//closure/goog/array",
        "//closure/goog/color",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "colorpicker",
    srcs = ["colorpicker.js"],
    deps = [
        ":colorpalette",
        ":component",
        "//closure/goog/dom",
        "//closure/goog/events:event",
        "//closure/goog/math:size",
    ],
)

closure_js_library(
    name = "combobox",
    srcs = ["combobox.js"],
    deps = [
        ":component",
        ":controlcontent",
        ":itemevent",
        ":labelinput",
        ":menu",
        ":menuitem",
        ":menuitemrenderer",
        ":menuseparator",
        ":registry",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:inputhandler",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/log",
        "//closure/goog/positioning",
        "//closure/goog/positioning:menuanchoredposition",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "component",
    srcs = ["component.js"],
    deps = [
        ":idgenerator",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/object",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "componentutil",
    srcs = ["componentutil.js"],
    deps = ["//closure/goog/events:eventtype"],
)

closure_js_library(
    name = "container",
    srcs = ["container.js"],
    deps = [
        ":component",
        ":componentutil",
        ":containerrenderer",
        ":control",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/object",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "containerrenderer",
    srcs = ["containerrenderer.js"],
    deps = [
        ":control",
        ":registry",
        "//closure/goog/a11y/aria",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "containerscroller",
    srcs = ["containerscroller.js"],
    deps = [
        ":component",
        ":container",
        "//closure/goog/disposable",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/style",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "control",
    srcs = ["control.js"],
    deps = [
        ":component",
        ":componentutil",
        ":controlcontent",
        ":controlrenderer",
        ":registry",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "controlcontent",
    srcs = ["controlcontent.js"],
)

closure_js_library(
    name = "controlrenderer",
    srcs = ["controlrenderer.js"],
    deps = [
        ":component",
        ":controlcontent",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "cookieeditor",
    srcs = ["cookieeditor.js"],
    deps = [
        ":component",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/net:cookies",
        "//closure/goog/string",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "css3buttonrenderer",
    srcs = ["css3buttonrenderer.js"],
    deps = [
        ":button",
        ":buttonrenderer",
        ":component",
        ":control",
        ":cssnames",
        ":registry",
        "//closure/goog/asserts",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "css3menubuttonrenderer",
    srcs = ["css3menubuttonrenderer.js"],
    deps = [
        ":controlcontent",
        ":cssnames",
        ":menubutton",
        ":menubuttonrenderer",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "cssnames",
    srcs = ["cssnames.js"],
)

closure_js_library(
    name = "custombutton",
    srcs = ["custombutton.js"],
    deps = [
        ":button",
        ":buttonrenderer",
        ":controlcontent",
        ":custombuttonrenderer",
        ":registry",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "custombuttonrenderer",
    srcs = ["custombuttonrenderer.js"],
    deps = [
        ":buttonrenderer",
        ":control",
        ":controlcontent",
        ":cssnames",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "customcolorpalette",
    srcs = ["customcolorpalette.js"],
    deps = [
        ":colorpalette",
        ":component",
        ":paletterenderer",
        "//closure/goog/color",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
    ],
)

closure_js_library(
    name = "datepicker",
    srcs = ["datepicker.js"],
    deps = [
        ":component",
        ":datepickerrenderer",
        ":defaultdatepickerrenderer",
        ":idgenerator",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/asserts",
        "//closure/goog/date",
        "//closure/goog/date:daterange",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keyhandler",
        "//closure/goog/i18n:datetimeformat",
        "//closure/goog/i18n:datetimepatterns",
        "//closure/goog/i18n:datetimesymbols",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "datepickerrenderer",
    srcs = ["datepickerrenderer.js"],
)

closure_js_library(
    name = "decorate",
    srcs = ["decorate.js"],
    deps = [
        ":component",
        ":registry",
    ],
)

closure_js_library(
    name = "defaultdatepickerrenderer",
    srcs = ["defaultdatepickerrenderer.js"],
    deps = [
        ":datepickerrenderer",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "dialog",
    srcs = ["dialog.js"],
    deps = [
        ":map",
        ":modalpopup",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keys",
        "//closure/goog/fx:dragger",
        "//closure/goog/html:safehtml",
        "//closure/goog/math:rect",
        "//closure/goog/string",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "dimensionpicker",
    srcs = ["dimensionpicker.js"],
    deps = [
        ":component",
        ":componentutil",
        ":control",
        ":dimensionpickerrenderer",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/math:size",
    ],
)

closure_js_library(
    name = "dimensionpickerrenderer",
    srcs = ["dimensionpickerrenderer.js"],
    deps = [
        ":controlrenderer",
        "//closure/goog/a11y/aria:announcer",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/i18n:bidi",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "dragdropdetector",
    srcs = ["dragdropdetector.js"],
    deps = [
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:coordinate",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "drilldownrow",
    srcs = ["drilldownrow.js"],
    deps = [
        ":component",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:safehtml",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "filteredmenu",
    srcs = ["filteredmenu.js"],
    deps = [
        ":component",
        ":control",
        ":filterobservingmenuitem",
        ":menu",
        ":menuitem",
        ":menurenderer",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:inputhandler",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "filterobservingmenuitem",
    srcs = ["filterobservingmenuitem.js"],
    deps = [
        ":controlcontent",
        ":filterobservingmenuitemrenderer",
        ":menuitem",
        ":menuitemrenderer",
        ":registry",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "filterobservingmenuitemrenderer",
    srcs = ["filterobservingmenuitemrenderer.js"],
    deps = [":menuitemrenderer"],
)

closure_js_library(
    name = "flatbuttonrenderer",
    srcs = ["flatbuttonrenderer.js"],
    deps = [
        ":button",
        ":buttonrenderer",
        ":control",
        ":cssnames",
        ":registry",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "flatmenubuttonrenderer",
    srcs = ["flatmenubuttonrenderer.js"],
    deps = [
        ":button",
        ":control",
        ":controlcontent",
        ":cssnames",
        ":flatbuttonrenderer",
        ":menu",
        ":menubutton",
        ":menurenderer",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "formpost",
    srcs = ["formpost.js"],
    deps = [
        ":component",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:safehtml",
    ],
)

closure_js_library(
    name = "gauge",
    srcs = ["gauge.js"],
    deps = [
        ":component",
        ":gaugetheme",
        "//closure/goog/a11y/aria",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/fx:animation",
        "//closure/goog/fx:easing",
        "//closure/goog/fx:transition",
        "//closure/goog/graphics",
        "//closure/goog/graphics:font",
        "//closure/goog/graphics:path",
        "//closure/goog/graphics:solidfill",
        "//closure/goog/math",
    ],
)

closure_js_library(
    name = "gaugetheme",
    srcs = ["gaugetheme.js"],
    deps = [
        "//closure/goog/graphics:lineargradient",
        "//closure/goog/graphics:solidfill",
        "//closure/goog/graphics:stroke",
    ],
)

closure_js_library(
    name = "hovercard",
    srcs = ["hovercard.js"],
    deps = [
        ":advancedtooltip",
        ":popupbase",
        ":tooltip",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/positioning:abstractposition",
    ],
)

closure_js_library(
    name = "hsvapalette",
    srcs = ["hsvapalette.js"],
    deps = [
        ":component",
        ":hsvpalette",
        "//closure/goog/array",
        "//closure/goog/color:alpha",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:rect",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "hsvpalette",
    srcs = ["hsvpalette.js"],
    deps = [
        ":component",
        "//closure/goog/color",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:inputhandler",
        "//closure/goog/math:rect",
        "//closure/goog/style",
        "//closure/goog/style:bidi",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "idgenerator",
    srcs = ["idgenerator.js"],
)

closure_js_library(
    name = "idletimer",
    srcs = ["idletimer.js"],
    deps = [
        ":activitymonitor",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
        "//closure/goog/structs:set",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "iframemask",
    srcs = ["iframemask.js"],
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:iframe",
        "//closure/goog/events:eventhandler",
        "//closure/goog/structs:pool",
        "//closure/goog/style",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "imagelessbuttonrenderer",
    srcs = ["imagelessbuttonrenderer.js"],
    deps = [
        ":button",
        ":component",
        ":controlcontent",
        ":cssnames",
        ":custombuttonrenderer",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "imagelessmenubuttonrenderer",
    srcs = ["imagelessmenubuttonrenderer.js"],
    deps = [
        ":button",
        ":controlcontent",
        ":cssnames",
        ":menubutton",
        ":menubuttonrenderer",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "inputdatepicker",
    srcs = ["inputdatepicker.js"],
    deps = [
        ":component",
        ":datepicker",
        ":labelinput",
        ":popupbase",
        ":popupdatepicker",
        "//closure/goog/date",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/i18n:datetimeformat",
        "//closure/goog/i18n:datetimeparse",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "itemevent",
    srcs = ["itemevent.js"],
    deps = ["//closure/goog/events:event"],
)

closure_js_library(
    name = "keyboardeventdata",
    srcs = ["keyboardeventdata.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/events:browserevent",
    ],
)

closure_js_library(
    name = "keyboardshortcuthandler",
    srcs = [
        "keyboardshortcutevent.js",
        "keyboardshortcuthandler.js",
    ],
    deps = [
        ":keyboardeventdata",
        ":synthetickeyboardevent",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keynames",
        "//closure/goog/events:keys",
        "//closure/goog/object",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "labelinput",
    srcs = ["labelinput.js"],
    deps = [
        ":component",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "linkbuttonrenderer",
    srcs = ["linkbuttonrenderer.js"],
    deps = [
        ":button",
        ":flatbuttonrenderer",
        ":registry",
    ],
)

closure_js_library(
    name = "map",
    srcs = ["map.js"],
)

closure_js_library(
    name = "menu",
    srcs = ["menu.js"],
    deps = [
        ":component",
        ":container",
        ":menuheader",
        ":menuitem",
        ":menurenderer",
        ":menuseparator",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/math:coordinate",
        "//closure/goog/string",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "menubar",
    srcs = ["menubar.js"],
    deps = [
        ":container",
        ":containerrenderer",
        ":menubarrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "menubardecorator",
    srcs = ["menubardecorator.js"],
    deps = [
        ":menubar",
        ":menubarrenderer",
        ":registry",
    ],
)

closure_js_library(
    name = "menubarrenderer",
    srcs = ["menubarrenderer.js"],
    deps = [
        ":container",
        ":containerrenderer",
        "//closure/goog/a11y/aria:roles",
    ],
)

closure_js_library(
    name = "menubase",
    srcs = ["menubase.js"],
    deps = [
        ":popup",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keyhandler",
    ],
)

closure_js_library(
    name = "menubutton",
    srcs = ["menubutton.js"],
    deps = [
        ":button",
        ":buttonrenderer",
        ":component",
        ":control",
        ":controlcontent",
        ":idgenerator",
        ":menu",
        ":menubuttonrenderer",
        ":menuitem",
        ":menurenderer",
        ":menuseparator",
        ":registry",
        ":submenu",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/math:box",
        "//closure/goog/math:coordinate",
        "//closure/goog/math:rect",
        "//closure/goog/math:size",
        "//closure/goog/positioning",
        "//closure/goog/positioning:anchoredposition",
        "//closure/goog/positioning:menuanchoredposition",
        "//closure/goog/style",
        "//closure/goog/timer",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "menubuttonrenderer",
    srcs = ["menubuttonrenderer.js"],
    deps = [
        ":control",
        ":controlcontent",
        ":cssnames",
        ":custombuttonrenderer",
        ":menu",
        ":menurenderer",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "menuheader",
    srcs = ["menuheader.js"],
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":menuheaderrenderer",
        ":registry",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "menuheaderrenderer",
    srcs = ["menuheaderrenderer.js"],
    deps = [":controlrenderer"],
)

closure_js_library(
    name = "menuitem",
    srcs = ["menuitem.js"],
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":menuitemrenderer",
        ":registry",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/events:keycodes",
        "//closure/goog/math:coordinate",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "menuitemrenderer",
    srcs = ["menuitemrenderer.js"],
    deps = [
        ":component",
        ":controlcontent",
        ":controlrenderer",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "menurenderer",
    srcs = ["menurenderer.js"],
    deps = [
        ":containerrenderer",
        ":control",
        ":separator",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "menuseparator",
    srcs = ["menuseparator.js"],
    deps = [
        ":menuseparatorrenderer",
        ":registry",
        ":separator",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "menuseparatorrenderer",
    srcs = ["menuseparatorrenderer.js"],
    deps = [
        ":controlcontent",
        ":controlrenderer",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "mockactivitymonitor",
    srcs = ["mockactivitymonitor.js"],
    deps = [
        ":activitymonitor",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "modalariavisibilityhelper",
    srcs = ["modalariavisibilityhelper.js"],
    deps = [
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "modalpopup",
    srcs = ["modalpopup.js"],
    deps = [
        ":component",
        ":modalariavisibilityhelper",
        ":popupbase",
        "//closure/goog/asserts",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:iframe",
        "//closure/goog/dom:tagname",
        "//closure/goog/dom/animationframe",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:focushandler",
        "//closure/goog/fx:transition",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "nativebuttonrenderer",
    srcs = ["nativebuttonrenderer.js"],
    deps = [
        ":buttonrenderer",
        ":component",
        "//closure/goog/asserts",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "option",
    srcs = ["option.js"],
    deps = [
        ":component",
        ":controlcontent",
        ":menuitem",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/events:event",
    ],
)

closure_js_library(
    name = "palette",
    srcs = ["palette.js"],
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":paletterenderer",
        ":selectionmodel",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/math:size",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "paletterenderer",
    srcs = ["paletterenderer.js"],
    deps = [
        ":controlcontent",
        ":controlrenderer",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:dataset",
        "//closure/goog/dom:nodeiterator",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/iter",
        "//closure/goog/math:size",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "plaintextspellchecker",
    srcs = ["plaintextspellchecker.js"],
    deps = [
        ":abstractspellchecker",
        ":component",
        ":popupmenu",
        "//closure/goog/a11y/aria",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/math:size",
        "//closure/goog/spell:spellcheck",
        "//closure/goog/style",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "popup",
    srcs = ["popup.js"],
    deps = [
        ":popupbase",
        "//closure/goog/math:box",
        "//closure/goog/positioning",
        "//closure/goog/positioning:abstractposition",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "popupbase",
    srcs = ["popupbase.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/fx:transition",
        "//closure/goog/style",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "popupcolorpicker",
    srcs = ["popupcolorpicker.js"],
    deps = [
        ":colorpicker",
        ":component",
        ":popup",
        ":popupbase",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/positioning",
        "//closure/goog/positioning:anchoredposition",
    ],
)

closure_js_library(
    name = "popupdatepicker",
    srcs = ["popupdatepicker.js"],
    deps = [
        ":component",
        ":datepicker",
        ":popup",
        ":popupbase",
        "//closure/goog/date",
        "//closure/goog/dom",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/positioning",
        "//closure/goog/positioning:anchoredviewportposition",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "popupmenu",
    srcs = ["popupmenu.js"],
    deps = [
        ":component",
        ":menu",
        ":menurenderer",
        ":popupbase",
        "//closure/goog/dom",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/math:box",
        "//closure/goog/positioning",
        "//closure/goog/positioning:abstractposition",
        "//closure/goog/positioning:anchoredviewportposition",
        "//closure/goog/positioning:menuanchoredposition",
        "//closure/goog/positioning:viewportclientposition",
        "//closure/goog/structs:map",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "progressbar",
    srcs = ["progressbar.js"],
    deps = [
        ":component",
        ":rangemodel",
        "//closure/goog/a11y/aria",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "prompt",
    srcs = ["prompt.js"],
    deps = [
        ":component",
        ":dialog",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:eventtype",
        "//closure/goog/functions",
        "//closure/goog/html:safehtml",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "rangemodel",
    srcs = ["rangemodel.js"],
    deps = [
        ":component",
        "//closure/goog/events:eventtarget",
    ],
)

closure_js_library(
    name = "ratings",
    srcs = ["ratings.js"],
    deps = [
        ":component",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "registry",
    srcs = ["registry.js"],
    deps = [
        ":component",
        ":controlrenderer",
        "//closure/goog/asserts",
        "//closure/goog/dom:classlist",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "richtextspellchecker",
    srcs = ["richtextspellchecker.js"],
    deps = [
        ":abstractspellchecker",
        ":component",
        ":popupmenu",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:range",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/math:coordinate",
        "//closure/goog/spell:spellcheck",
        "//closure/goog/string:stringbuffer",
        "//closure/goog/style",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "roundedpanel",
    srcs = ["roundedpanel.js"],
    deps = [
        ":component",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/graphics",
        "//closure/goog/graphics:path",
        "//closure/goog/graphics:solidfill",
        "//closure/goog/graphics:stroke",
        "//closure/goog/math",
        "//closure/goog/math:coordinate",
        "//closure/goog/math:size",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "roundedtabrenderer",
    srcs = ["roundedtabrenderer.js"],
    deps = [
        ":control",
        ":controlcontent",
        ":registry",
        ":tab",
        ":tabbar",
        ":tabrenderer",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "scrollfloater",
    srcs = ["scrollfloater.js"],
    deps = [
        ":component",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:rect",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "select",
    srcs = ["select.js"],
    deps = [
        ":buttonrenderer",
        ":component",
        ":control",
        ":controlcontent",
        ":idgenerator",
        ":menu",
        ":menubutton",
        ":menuitem",
        ":menurenderer",
        ":menuseparator",
        ":registry",
        ":selectionmodel",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "selectionmenubutton",
    srcs = ["selectionmenubutton.js"],
    deps = [
        ":buttonrenderer",
        ":component",
        ":menubutton",
        ":menuitem",
        ":menuitemrenderer",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "selectionmodel",
    srcs = ["selectionmodel.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "separator",
    srcs = ["separator.js"],
    deps = [
        ":component",
        ":control",
        ":menuseparatorrenderer",
        ":registry",
        "//closure/goog/a11y/aria",
        "//closure/goog/asserts",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "serverchart",
    srcs = ["serverchart.js"],
    deps = [
        ":component",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/string",
        "//closure/goog/uri",
    ],
)

closure_js_library(
    name = "slider",
    srcs = ["slider.js"],
    deps = [
        ":sliderbase",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "sliderbase",
    srcs = ["sliderbase.js"],
    deps = [
        ":component",
        ":rangemodel",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/events:mousewheelhandler",
        "//closure/goog/functions",
        "//closure/goog/fx:animation",
        "//closure/goog/fx:animationqueue",
        "//closure/goog/fx:dom",
        "//closure/goog/fx:dragger",
        "//closure/goog/fx:transition",
        "//closure/goog/fx:transitionbase",
        "//closure/goog/math",
        "//closure/goog/math:coordinate",
        "//closure/goog/style",
        "//closure/goog/style:bidi",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "splitpane",
    srcs = ["splitpane.js"],
    deps = [
        ":component",
        "//closure/goog/asserts",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/fx:dragger",
        "//closure/goog/math:rect",
        "//closure/goog/math:size",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "submenu",
    srcs = ["submenu.js"],
    deps = [
        ":component",
        ":controlcontent",
        ":menu",
        ":menuheader",
        ":menuitem",
        ":menuitemrenderer",
        ":menuseparator",
        ":registry",
        ":submenurenderer",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/positioning",
        "//closure/goog/positioning:anchoredviewportposition",
        "//closure/goog/style",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "submenurenderer",
    srcs = ["submenurenderer.js"],
    deps = [
        ":control",
        ":controlcontent",
        ":menu",
        ":menuitemrenderer",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "synthetickeyboardevent",
    srcs = ["synthetickeyboardevent.js"],
    deps = [
        ":keyboardeventdata",
        "//closure/goog/events:event",
    ],
)

closure_js_library(
    name = "tab",
    srcs = ["tab.js"],
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":registry",
        ":tabrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "tabbar",
    srcs = ["tabbar.js"],
    deps = [
        ":component",
        ":container",
        ":control",
        ":registry",
        ":tab",
        ":tabbarrenderer",
        "//closure/goog/dom",
        "//closure/goog/events:event",
    ],
)

closure_js_library(
    name = "tabbarrenderer",
    srcs = ["tabbarrenderer.js"],
    deps = [
        ":container",
        ":containerrenderer",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "tablesorter",
    srcs = ["tablesorter.js"],
    deps = [
        ":component",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtype",
        "//closure/goog/functions",
    ],
)

closure_js_library(
    name = "tabpane",
    srcs = ["tabpane.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/html:safestylesheet",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "tabrenderer",
    srcs = ["tabrenderer.js"],
    deps = [
        ":component",
        ":controlrenderer",
        "//closure/goog/a11y/aria:roles",
    ],
)

closure_js_library(
    name = "textarea",
    srcs = ["textarea.js"],
    deps = [
        ":control",
        ":textarearenderer",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:box",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "textarearenderer",
    srcs = ["textarearenderer.js"],
    deps = [
        ":component",
        ":controlrenderer",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "togglebutton",
    srcs = ["togglebutton.js"],
    deps = [
        ":button",
        ":buttonrenderer",
        ":component",
        ":controlcontent",
        ":custombuttonrenderer",
        ":registry",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbar",
    srcs = ["toolbar.js"],
    deps = [
        ":container",
        ":toolbarrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbarbutton",
    srcs = ["toolbarbutton.js"],
    deps = [
        ":button",
        ":buttonrenderer",
        ":controlcontent",
        ":registry",
        ":toolbarbuttonrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbarbuttonrenderer",
    srcs = ["toolbarbuttonrenderer.js"],
    deps = [":custombuttonrenderer"],
)

closure_js_library(
    name = "toolbarcolormenubutton",
    srcs = ["toolbarcolormenubutton.js"],
    deps = [
        ":colormenubutton",
        ":colormenubuttonrenderer",
        ":controlcontent",
        ":menu",
        ":registry",
        ":toolbarcolormenubuttonrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbarcolormenubuttonrenderer",
    srcs = ["toolbarcolormenubuttonrenderer.js"],
    deps = [
        ":colormenubuttonrenderer",
        ":control",
        ":controlcontent",
        ":menubuttonrenderer",
        ":toolbarmenubuttonrenderer",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
    ],
)

closure_js_library(
    name = "toolbarmenubutton",
    srcs = ["toolbarmenubutton.js"],
    deps = [
        ":buttonrenderer",
        ":controlcontent",
        ":menu",
        ":menubutton",
        ":registry",
        ":toolbarmenubuttonrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbarmenubuttonrenderer",
    srcs = ["toolbarmenubuttonrenderer.js"],
    deps = [":menubuttonrenderer"],
)

closure_js_library(
    name = "toolbarrenderer",
    srcs = ["toolbarrenderer.js"],
    deps = [
        ":container",
        ":containerrenderer",
        ":control",
        ":separator",
        ":toolbarseparatorrenderer",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "toolbarselect",
    srcs = ["toolbarselect.js"],
    deps = [
        ":controlcontent",
        ":menu",
        ":menubuttonrenderer",
        ":registry",
        ":select",
        ":toolbarmenubuttonrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbarseparator",
    srcs = ["toolbarseparator.js"],
    deps = [
        ":registry",
        ":separator",
        ":toolbarseparatorrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbarseparatorrenderer",
    srcs = ["toolbarseparatorrenderer.js"],
    deps = [
        ":cssnames",
        ":menuseparatorrenderer",
        "//closure/goog/asserts",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "toolbartogglebutton",
    srcs = ["toolbartogglebutton.js"],
    deps = [
        ":controlcontent",
        ":registry",
        ":togglebutton",
        ":toolbarbuttonrenderer",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "tooltip",
    srcs = ["tooltip.js"],
    deps = [
        ":popup",
        ":popupbase",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:focushandler",
        "//closure/goog/html:safehtml",
        "//closure/goog/math:box",
        "//closure/goog/math:coordinate",
        "//closure/goog/positioning",
        "//closure/goog/positioning:abstractposition",
        "//closure/goog/positioning:anchoredposition",
        "//closure/goog/positioning:viewportposition",
        "//closure/goog/structs:set",
        "//closure/goog/style",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "tristatemenuitem",
    srcs = ["tristatemenuitem.js"],
    deps = [
        ":component",
        ":controlcontent",
        ":menuitem",
        ":menuitemrenderer",
        ":registry",
        ":tristatemenuitemrenderer",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
    ],
)

closure_js_library(
    name = "tristatemenuitemrenderer",
    srcs = ["tristatemenuitemrenderer.js"],
    deps = [
        ":menuitemrenderer",
        "//closure/goog/asserts",
        "//closure/goog/dom:classlist",
    ],
)

closure_js_library(
    name = "twothumbslider",
    srcs = ["twothumbslider.js"],
    deps = [
        ":sliderbase",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "zippy",
    srcs = ["zippy.js"],
    deps = [
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/style",
    ],
)
