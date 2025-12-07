load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

alias(
    name = "abstractmultirange",
    actual = ":range",
)

closure_js_library(
    name = "abstractrange",
    srcs = [
        "abstractrange.js",
        "savedrange.js",
    ],
    lenient = True,
    deps = [
        ":dom",
        ":nodetype",
        ":tagiterator",
        "//closure/goog/disposable",
        "//closure/goog/log",
        "//closure/goog/math:coordinate",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "annotate",
    srcs = ["annotate.js"],
    lenient = True,
    deps = [
        ":dom",
        ":nodetype",
        ":safe",
        ":tagname",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/html:safehtml",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "asserts",
    srcs = ["asserts.js"],
    lenient = True,
    deps = ["//closure/goog/asserts"],
)

closure_js_library(
    name = "attr",
    srcs = ["attr.js"],
    lenient = True,
)

closure_js_library(
    name = "browserfeature",
    srcs = ["browserfeature.js"],
    lenient = True,
    deps = ["//closure/goog/useragent"],
)

closure_js_library(
    name = "bufferedviewportsizemonitor",
    srcs = ["bufferedviewportsizemonitor.js"],
    lenient = True,
    deps = [
        ":dom",
        ":viewportsizemonitor",
        "//closure/goog/asserts",
        "//closure/goog/async:delay",
        "//closure/goog/events",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:size",
    ],
)

closure_js_library(
    name = "classes",
    srcs = ["classes.js"],
    lenient = True,
    deps = ["//closure/goog/array"],
)

closure_js_library(
    name = "classlist",
    srcs = ["classlist.js"],
    lenient = True,
    deps = ["//closure/goog/array"],
)

alias(
    name = "controlrange",
    actual = ":range",
)

closure_js_library(
    name = "dataset",
    srcs = ["dataset.js"],
    lenient = True,
    deps = [
        "//closure/goog/labs/useragent:browser",
        "//closure/goog/string",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "dom",
    srcs = ["dom.js"],
    lenient = True,
    deps = [
        ":browserfeature",
        ":nodetype",
        ":safe",
        ":tagname",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/asserts:dom",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/math:coordinate",
        "//closure/goog/math:size",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/string:const",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "element",
    srcs = ["element.js"],
    lenient = True,
    deps = [
        ":nodetype",
        ":tagname",
    ],
)

closure_js_library(
    name = "fontsizemonitor",
    srcs = ["fontsizemonitor.js"],
    lenient = True,
    deps = [
        ":dom",
        ":tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "forms",
    srcs = ["forms.js"],
    lenient = True,
    deps = [
        ":inputtype",
        ":safe",
        ":tagname",
        "//closure/goog/structs:map",
        "//closure/goog/window",
    ],
)

closure_js_library(
    name = "fullscreen",
    srcs = ["fullscreen.js"],
    lenient = True,
    deps = [":dom"],
)

closure_js_library(
    name = "htmlelement",
    srcs = ["htmlelement.js"],
    lenient = True,
)

closure_js_library(
    name = "iframe",
    srcs = ["iframe.js"],
    lenient = True,
    deps = [
        ":dom",
        ":safe",
        ":tagname",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safestyle",
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/string:const",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "inputtype",
    srcs = ["inputtype.js"],
    lenient = True,
)

closure_js_library(
    name = "iter",
    srcs = ["iter.js"],
    lenient = True,
    deps = ["//closure/goog/iter"],
)

alias(
    name = "multirange",
    actual = ":range",
)

closure_js_library(
    name = "nodeiterator",
    srcs = ["nodeiterator.js"],
    lenient = True,
    deps = [
        ":tagiterator",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "nodeoffset",
    srcs = ["nodeoffset.js"],
    lenient = True,
    deps = [
        ":tagname",
        "//closure/goog/disposable",
    ],
)

closure_js_library(
    name = "nodetype",
    srcs = ["nodetype.js"],
    lenient = True,
)

closure_js_library(
    name = "range",
    srcs = [
        "abstractmultirange.js",
        "controlrange.js",
        "multirange.js",
        "range.js",
        "savedcaretrange.js",
        "textrange.js",
    ],
    lenient = True,
    deps = [
        ":abstractrange",
        ":browserfeature",
        ":dom",
        ":nodetype",
        ":tagiterator",
        ":tagname",
        ":textrangeiterator",
        "//closure/goog/array",
        "//closure/goog/dom/browserrange",
        "//closure/goog/dom/browserrange:abstractrange",
        "//closure/goog/iter",
        "//closure/goog/log",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "rangeendpoint",
    srcs = ["rangeendpoint.js"],
    lenient = True,
)

closure_js_library(
    name = "safe",
    srcs = ["safe.js"],
    lenient = True,
    deps = [
        ":asserts",
        "//closure/goog/asserts",
        "//closure/goog/asserts:dom",
        "//closure/goog/functions",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safescript",
        "//closure/goog/html:safestyle",
        "//closure/goog/html:safeurl",
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/string:const",
        "//closure/goog/string:internal",
    ],
)

alias(
    name = "savedcaretrange",
    actual = ":range",
)

alias(
    name = "savedrange",
    actual = ":abstractrange",
)

closure_js_library(
    name = "selection",
    srcs = ["selection.js"],
    lenient = True,
    deps = [
        ":inputtype",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "tagiterator",
    srcs = ["tagiterator.js"],
    lenient = True,
    deps = [
        ":dom",
        ":nodetype",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "tagname",
    srcs = ["tagname.js"],
    lenient = True,
    deps = [":htmlelement"],
)

closure_js_library(
    name = "tags",
    srcs = ["tags.js"],
    lenient = True,
    deps = ["//closure/goog/object"],
)

closure_js_library(
    name = "textassert",
    srcs = ["textassert.js"],
    lenient = True,
    deps = [
        ":dom",
        ":tagname",
        "//closure/goog/asserts",
    ],
)

alias(
    name = "textrange",
    actual = ":range",
)

closure_js_library(
    name = "textrangeiterator",
    srcs = ["textrangeiterator.js"],
    lenient = True,
    deps = [
        ":abstractrange",
        ":dom",
        ":nodetype",
        ":tagname",
        "//closure/goog/array",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "uri",
    srcs = ["uri.js"],
    lenient = True,
    deps = [
        ":dom",
        ":safe",
        ":tagname",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/string:const",
    ],
)

closure_js_library(
    name = "vendor",
    srcs = ["vendor.js"],
    lenient = True,
    deps = [
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "viewportsizemonitor",
    srcs = ["viewportsizemonitor.js"],
    lenient = True,
    deps = [
        ":dom",
        "//closure/goog/disposable",
        "//closure/goog/events",
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/math:size",
    ],
)

closure_js_library(
    name = "xml",
    srcs = ["xml.js"],
    lenient = True,
    deps = [
        ":dom",
        ":nodetype",
        ":safe",
        "//closure/goog/html:legacyconversions",
        "//closure/goog/useragent",
    ],
)
