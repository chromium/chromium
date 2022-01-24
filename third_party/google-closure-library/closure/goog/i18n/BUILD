load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "bidi",
    srcs = ["bidi.js"],
)

closure_js_library(
    name = "bidiformatter",
    srcs = ["bidiformatter.js"],
    deps = [
        ":bidi",
        "//closure/goog/html:safehtml",
    ],
)

closure_js_library(
    name = "charlistdecompressor",
    srcs = ["charlistdecompressor.js"],
    deps = [
        ":uchar",
        "//closure/goog/array",
    ],
)

closure_js_library(
    name = "charpickerdata",
    srcs = ["charpickerdata.js"],
)

closure_js_library(
    name = "collation",
    srcs = ["collation.js"],
)

closure_js_library(
    name = "compactnumberformatsymbols",
    srcs = ["compactnumberformatsymbols.js"],
)

closure_js_library(
    name = "compactnumberformatsymbolsext",
    srcs = ["compactnumberformatsymbolsext.js"],
    deps = [":compactnumberformatsymbols"],
)

closure_js_library(
    name = "currency",
    srcs = ["currency.js"],
)

closure_js_library(
    name = "currencycodemap",
    srcs = ["currencycodemap.js"],
)

closure_js_library(
    name = "dateintervalformat",
    srcs = ["dateintervalformat.js"],
    deps = [
        ":dateintervalsymbols",
        ":datetimeformat",
        ":datetimesymbols",
        ":timezone",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/date",
        "//closure/goog/date:datelike",
        "//closure/goog/date:daterange",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "dateintervalpatterns",
    srcs = ["dateintervalpatterns.js"],
    deps = [":dateintervalsymbols"],
)

closure_js_library(
    name = "dateintervalpatternsext",
    srcs = ["dateintervalpatternsext.js"],
    deps = [":dateintervalpatterns"],
)

closure_js_library(
    name = "dateintervalsymbols",
    srcs = ["dateintervalsymbols.js"],
)

closure_js_library(
    name = "dateintervalsymbolsext",
    srcs = ["dateintervalsymbolsext.js"],
    deps = [":dateintervalsymbols"],
)

closure_js_library(
    name = "datetimeformat",
    srcs = ["datetimeformat.js"],
    deps = [
        ":datetimesymbols",
        ":timezone",
        "//closure/goog/asserts",
        "//closure/goog/date",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "datetimeparse",
    srcs = ["datetimeparse.js"],
    deps = [
        ":datetimeformat",
        ":datetimesymbols",
        "//closure/goog/asserts",
        "//closure/goog/date",
    ],
)

closure_js_library(
    name = "datetimepatternstype",
    srcs = ["datetimepatternstype.js"],
)

closure_js_library(
    name = "datetimepatterns",
    srcs = ["datetimepatterns.js"],
    deps = [":datetimepatternstype"],
)

closure_js_library(
    name = "datetimepatternsext",
    srcs = ["datetimepatternsext.js"],
    deps = [":datetimepatterns"],
)

closure_js_library(
    name = "datetimesymbols",
    srcs = ["datetimesymbols.js"],
)

closure_js_library(
    name = "datetimesymbolsext",
    srcs = ["datetimesymbolsext.js"],
    deps = [":datetimesymbols"],
)

closure_js_library(
    name = "graphemebreak",
    srcs = ["graphemebreak.js"],
    deps = [
        ":uchar",
        "//closure/goog/asserts",
        "//closure/goog/structs:inversionmap",
    ],
)

closure_js_library(
    name = "localefeature",
    srcs = ["localefeature.js"],
)

closure_js_library(
    name = "messageformat",
    srcs = ["messageformat.js"],
    deps = [
        ":compactnumberformatsymbols",
        ":numberformat",
        ":numberformatsymbols",
        ":ordinalrules",
        ":pluralrules",
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "mime",
    srcs = ["mime.js"],
    deps = [
        ":uchar",
        "//closure/goog/array",
    ],
)

closure_js_library(
    name = "numberformat",
    srcs = ["numberformat.js"],
    deps = [
        ":compactnumberformatsymbols",
        ":currency",
        ":localefeature",
        ":numberformatsymbols",
        "//closure/goog/asserts",
        "//closure/goog/math",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "numberformatsymbols",
    srcs = [
        "numberformatsymbols.js",
        "numberformatsymbolstype.js",
    ],
)

closure_js_library(
    name = "numberformatsymbolsext",
    srcs = ["numberformatsymbolsext.js"],
    deps = [":numberformatsymbols"],
)

closure_js_library(
    name = "ordinalrules",
    srcs = ["ordinalrules.js"],
    deps = [":localefeature"],
)

closure_js_library(
    name = "pluralrules",
    srcs = ["pluralrules.js"],
    deps = [":localefeature"],
)

closure_js_library(
    name = "relativedatetimeformat",
    srcs = ["relativedatetimeformat.js"],
    deps = [
        ":localefeature",
        ":messageformat",
        ":relativedatetimesymbols",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "relativedatetimesymbols",
    srcs = ["relativedatetimesymbols.js"],
)

closure_js_library(
    name = "relativedatetimesymbolsext",
    srcs = ["relativedatetimesymbolsext.js"],
    deps = [":relativedatetimesymbols"],
)

closure_js_library(
    name = "timezone",
    srcs = ["timezone.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/date:datelike",
        "//closure/goog/object",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "uchar",
    srcs = ["uchar.js"],
)

closure_js_library(
    name = "ucharnames",
    srcs = ["ucharnames.js"],
    deps = [":uchar"],
)
