load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "abstractspellchecker",
    srcs = ["abstractspellchecker.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    srcs = [
        "button.js",
        "buttonrenderer.js",
        "nativebuttonrenderer.js",
    ],
    lenient = True,
    deps = [
        ":buttonside",
        ":component",
        ":control",
        ":controlcontent",
        ":registry",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
    ],
)

alias(
    name = "buttonrenderer",
    actual = ":button",
)

closure_js_library(
    name = "buttonside",
    srcs = ["buttonside.js"],
    lenient = True,
)

closure_js_library(
    name = "charcounter",
    srcs = ["charcounter.js"],
    lenient = True,
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
    lenient = True,
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
    srcs = [
        "checkbox.js",
        "checkboxrenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":control",
        ":registry",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/object",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "checkboxmenuitem",
    srcs = ["checkboxmenuitem.js"],
    lenient = True,
    deps = [
        ":controlcontent",
        ":menuitem",
        ":registry",
        "//closure/goog/dom",
    ],
)

alias(
    name = "checkboxrenderer",
    actual = ":checkbox",
)

closure_js_library(
    name = "colormenubutton",
    srcs = [
        "colormenubutton.js",
        "colormenubuttonrenderer.js",
    ],
    lenient = True,
    deps = [
        ":colorpalette",
        ":component",
        ":control",
        ":controlcontent",
        ":menu",
        ":menubutton",
        ":registry",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/color",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/object",
    ],
)

alias(
    name = "colormenubuttonrenderer",
    actual = ":colormenubutton",
)

closure_js_library(
    name = "colorpalette",
    srcs = ["colorpalette.js"],
    lenient = True,
    deps = [
        ":palette",
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":component",
        ":controlcontent",
        ":itemevent",
        ":labelinput",
        ":menu",
        ":menuitem",
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
    srcs = [
        "component.js",
        "componentutil.js",
    ],
    lenient = True,
    deps = [
        ":idgenerator",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/object",
        "//closure/goog/style",
    ],
)

alias(
    name = "componentutil",
    actual = ":component",
)

closure_js_library(
    name = "container",
    srcs = [
        "container.js",
        "containerrenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":componentutil",
        ":control",
        ":registry",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "containerrenderer",
    actual = ":container",
)

closure_js_library(
    name = "containerscroller",
    srcs = ["containerscroller.js"],
    lenient = True,
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

alias(
    name = "control",
    actual = ":registry",
)

closure_js_library(
    name = "controlcontent",
    srcs = ["controlcontent.js"],
    lenient = True,
)

alias(
    name = "controlrenderer",
    actual = ":registry",
)

closure_js_library(
    name = "cookieeditor",
    srcs = ["cookieeditor.js"],
    lenient = True,
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
    lenient = True,
    deps = [
        ":button",
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
    lenient = True,
    deps = [
        ":controlcontent",
        ":cssnames",
        ":menubutton",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "cssnames",
    srcs = ["cssnames.js"],
    lenient = True,
)

closure_js_library(
    name = "custombutton",
    srcs = [
        "custombutton.js",
        "custombuttonrenderer.js",
    ],
    lenient = True,
    deps = [
        ":button",
        ":control",
        ":controlcontent",
        ":cssnames",
        ":registry",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagname",
        "//closure/goog/string",
    ],
)

alias(
    name = "custombuttonrenderer",
    actual = ":custombutton",
)

closure_js_library(
    name = "customcolorpalette",
    srcs = ["customcolorpalette.js"],
    lenient = True,
    deps = [
        ":colorpalette",
        ":component",
        ":palette",
        "//closure/goog/color",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
    ],
)

closure_js_library(
    name = "datepicker",
    srcs = [
        "datepicker.js",
        "datepickerrenderer.js",
        "defaultdatepickerrenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
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

alias(
    name = "datepickerrenderer",
    actual = ":datepicker",
)

closure_js_library(
    name = "decorate",
    srcs = ["decorate.js"],
    lenient = True,
    deps = [
        ":component",
        ":registry",
    ],
)

alias(
    name = "defaultdatepickerrenderer",
    actual = ":datepicker",
)

closure_js_library(
    name = "dialog",
    srcs = ["dialog.js"],
    lenient = True,
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
    srcs = [
        "dimensionpicker.js",
        "dimensionpickerrenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":componentutil",
        ":control",
        ":registry",
        "//closure/goog/a11y/aria:announcer",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/i18n:bidi",
        "//closure/goog/math:size",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "dimensionpickerrenderer",
    actual = ":dimensionpicker",
)

closure_js_library(
    name = "dragdropdetector",
    srcs = ["dragdropdetector.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":component",
        ":control",
        ":filterobservingmenuitem",
        ":menu",
        ":menuitem",
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
    srcs = [
        "filterobservingmenuitem.js",
        "filterobservingmenuitemrenderer.js",
    ],
    lenient = True,
    deps = [
        ":controlcontent",
        ":menuitem",
        ":registry",
        "//closure/goog/dom",
    ],
)

alias(
    name = "filterobservingmenuitemrenderer",
    actual = ":filterobservingmenuitem",
)

closure_js_library(
    name = "flatbuttonrenderer",
    srcs = ["flatbuttonrenderer.js"],
    lenient = True,
    deps = [
        ":button",
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
    lenient = True,
    deps = [
        ":button",
        ":control",
        ":controlcontent",
        ":cssnames",
        ":flatbuttonrenderer",
        ":menu",
        ":menubutton",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "formpost",
    srcs = ["formpost.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        "//closure/goog/graphics:lineargradient",
        "//closure/goog/graphics:solidfill",
        "//closure/goog/graphics:stroke",
    ],
)

closure_js_library(
    name = "hovercard",
    srcs = ["hovercard.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
)

closure_js_library(
    name = "idletimer",
    srcs = ["idletimer.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":button",
        ":controlcontent",
        ":cssnames",
        ":menubutton",
        ":registry",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "inputdatepicker",
    srcs = ["inputdatepicker.js"],
    lenient = True,
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
    lenient = True,
    deps = ["//closure/goog/events:event"],
)

closure_js_library(
    name = "keyboardeventdata",
    srcs = ["keyboardeventdata.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":button",
        ":flatbuttonrenderer",
        ":registry",
    ],
)

closure_js_library(
    name = "map",
    srcs = ["map.js"],
    lenient = True,
)

closure_js_library(
    name = "menu",
    srcs = [
        "menu.js",
        "menuitem.js",
        "menuitemrenderer.js",
        "menurenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":container",
        ":control",
        ":controlcontent",
        ":menuheader",
        ":menuseparator",
        ":registry",
        ":separator",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/events:keycodes",
        "//closure/goog/math:coordinate",
        "//closure/goog/string",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "menubar",
    srcs = [
        "menubar.js",
        "menubarrenderer.js",
    ],
    lenient = True,
    deps = [
        ":container",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "menubardecorator",
    srcs = ["menubardecorator.js"],
    lenient = True,
    deps = [
        ":menubar",
        ":registry",
    ],
)

alias(
    name = "menubarrenderer",
    actual = ":menubar",
)

closure_js_library(
    name = "menubase",
    srcs = ["menubase.js"],
    lenient = True,
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
    srcs = [
        "menubutton.js",
        "menubuttonrenderer.js",
    ],
    lenient = True,
    deps = [
        ":button",
        ":component",
        ":control",
        ":controlcontent",
        ":cssnames",
        ":custombutton",
        ":idgenerator",
        ":menu",
        ":menuitem",
        ":menuseparator",
        ":registry",
        ":submenu",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
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

alias(
    name = "menubuttonrenderer",
    actual = ":menubutton",
)

closure_js_library(
    name = "menuheader",
    srcs = [
        "menuheader.js",
        "menuheaderrenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":registry",
        "//closure/goog/dom",
    ],
)

alias(
    name = "menuheaderrenderer",
    actual = ":menuheader",
)

alias(
    name = "menuitem",
    actual = ":menu",
)

alias(
    name = "menuitemrenderer",
    actual = ":menu",
)

alias(
    name = "menurenderer",
    actual = ":menu",
)

alias(
    name = "menuseparator",
    actual = ":separator",
)

alias(
    name = "menuseparatorrenderer",
    actual = ":separator",
)

closure_js_library(
    name = "mockactivitymonitor",
    srcs = ["mockactivitymonitor.js"],
    lenient = True,
    deps = [
        ":activitymonitor",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "modalariavisibilityhelper",
    srcs = ["modalariavisibilityhelper.js"],
    lenient = True,
    deps = [
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "modalpopup",
    srcs = ["modalpopup.js"],
    lenient = True,
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
        "//closure/goog/math:size",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "nativebuttonrenderer",
    actual = ":button",
)

closure_js_library(
    name = "option",
    srcs = ["option.js"],
    lenient = True,
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
    srcs = [
        "palette.js",
        "paletterenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":selectionmodel",
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
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/iter",
        "//closure/goog/math:size",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "paletterenderer",
    actual = ":palette",
)

closure_js_library(
    name = "plaintextspellchecker",
    srcs = ["plaintextspellchecker.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":component",
        ":menu",
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
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":component",
        "//closure/goog/events:eventtarget",
    ],
)

closure_js_library(
    name = "ratings",
    srcs = ["ratings.js"],
    lenient = True,
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
    srcs = [
        "control.js",
        "controlrenderer.js",
        "registry.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":controlcontent",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/events:keyhandler",
        "//closure/goog/functions",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "richtextspellchecker",
    srcs = ["richtextspellchecker.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
    deps = [
        ":control",
        ":controlcontent",
        ":registry",
        ":tab",
        ":tabbar",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "scrollfloater",
    srcs = ["scrollfloater.js"],
    lenient = True,
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
    lenient = True,
    deps = [
        ":button",
        ":component",
        ":control",
        ":controlcontent",
        ":idgenerator",
        ":menu",
        ":menubutton",
        ":menuitem",
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
    lenient = True,
    deps = [
        ":button",
        ":component",
        ":menubutton",
        ":menuitem",
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
    lenient = True,
    deps = [
        "//closure/goog/array",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
    ],
)

closure_js_library(
    name = "separator",
    srcs = [
        "menuseparator.js",
        "menuseparatorrenderer.js",
        "separator.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":registry",
        "//closure/goog/a11y/aria",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "serverchart",
    srcs = ["serverchart.js"],
    lenient = True,
    deps = [
        ":component",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/asserts:dom",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:event",
        "//closure/goog/string",
        "//closure/goog/uri",
    ],
)

closure_js_library(
    name = "slider",
    srcs = ["slider.js"],
    lenient = True,
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
    lenient = True,
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
    lenient = True,
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
    ],
)

closure_js_library(
    name = "submenu",
    srcs = [
        "submenu.js",
        "submenurenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":menu",
        ":menuheader",
        ":menuitem",
        ":menuseparator",
        ":registry",
        "//closure/goog/a11y/aria",
        "//closure/goog/a11y/aria:attributes",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
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

alias(
    name = "submenurenderer",
    actual = ":submenu",
)

closure_js_library(
    name = "synthetickeyboardevent",
    srcs = ["synthetickeyboardevent.js"],
    lenient = True,
    deps = [
        ":keyboardeventdata",
        "//closure/goog/events:event",
    ],
)

closure_js_library(
    name = "tab",
    srcs = [
        "tab.js",
        "tabrenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":control",
        ":controlcontent",
        ":registry",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "tabbar",
    srcs = [
        "tabbar.js",
        "tabbarrenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":container",
        ":control",
        ":registry",
        ":tab",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom",
        "//closure/goog/events:event",
        "//closure/goog/object",
    ],
)

alias(
    name = "tabbarrenderer",
    actual = ":tabbar",
)

closure_js_library(
    name = "tablesorter",
    srcs = ["tablesorter.js"],
    lenient = True,
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
    lenient = True,
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

alias(
    name = "tabrenderer",
    actual = ":tab",
)

closure_js_library(
    name = "textarea",
    srcs = [
        "textarea.js",
        "textarearenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":control",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:box",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "textarearenderer",
    actual = ":textarea",
)

closure_js_library(
    name = "togglebutton",
    srcs = ["togglebutton.js"],
    lenient = True,
    deps = [
        ":button",
        ":component",
        ":controlcontent",
        ":custombuttonrenderer",
        ":registry",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbar",
    srcs = [
        "toolbar.js",
        "toolbarrenderer.js",
    ],
    lenient = True,
    deps = [
        ":container",
        ":control",
        ":separator",
        ":toolbarseparator",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "toolbarbutton",
    srcs = [
        "toolbarbutton.js",
        "toolbarbuttonrenderer.js",
    ],
    lenient = True,
    deps = [
        ":button",
        ":controlcontent",
        ":custombutton",
        ":registry",
        ":toolbarseparator",
        "//closure/goog/dom",
    ],
)

alias(
    name = "toolbarbuttonrenderer",
    actual = ":toolbarbutton",
)

closure_js_library(
    name = "toolbarcolormenubutton",
    srcs = [
        "toolbarcolormenubutton.js",
        "toolbarcolormenubuttonrenderer.js",
    ],
    lenient = True,
    deps = [
        ":colormenubutton",
        ":control",
        ":controlcontent",
        ":menu",
        ":menubutton",
        ":registry",
        ":toolbarmenubutton",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
    ],
)

alias(
    name = "toolbarcolormenubuttonrenderer",
    actual = ":toolbarcolormenubutton",
)

closure_js_library(
    name = "toolbarmenubutton",
    srcs = [
        "toolbarmenubutton.js",
        "toolbarmenubuttonrenderer.js",
    ],
    lenient = True,
    deps = [
        ":button",
        ":controlcontent",
        ":menu",
        ":menubutton",
        ":registry",
        "//closure/goog/dom",
    ],
)

alias(
    name = "toolbarmenubuttonrenderer",
    actual = ":toolbarmenubutton",
)

alias(
    name = "toolbarrenderer",
    actual = ":toolbar",
)

closure_js_library(
    name = "toolbarselect",
    srcs = ["toolbarselect.js"],
    lenient = True,
    deps = [
        ":controlcontent",
        ":menu",
        ":menubutton",
        ":registry",
        ":select",
        ":toolbarmenubutton",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "toolbarseparator",
    srcs = [
        "toolbarseparator.js",
        "toolbarseparatorrenderer.js",
    ],
    lenient = True,
    deps = [
        ":control",
        ":cssnames",
        ":registry",
        ":separator",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
    ],
)

alias(
    name = "toolbarseparatorrenderer",
    actual = ":toolbarseparator",
)

closure_js_library(
    name = "toolbartogglebutton",
    srcs = ["toolbartogglebutton.js"],
    lenient = True,
    deps = [
        ":controlcontent",
        ":registry",
        ":togglebutton",
        ":toolbarbutton",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "tooltip",
    srcs = ["tooltip.js"],
    lenient = True,
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
    srcs = [
        "tristatemenuitem.js",
        "tristatemenuitemrenderer.js",
    ],
    lenient = True,
    deps = [
        ":component",
        ":controlcontent",
        ":menuitem",
        ":registry",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
    ],
)

alias(
    name = "tristatemenuitemrenderer",
    actual = ":tristatemenuitem",
)

closure_js_library(
    name = "twothumbslider",
    srcs = ["twothumbslider.js"],
    lenient = True,
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
    lenient = True,
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
