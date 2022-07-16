load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "assertionfailure",
    srcs = ["assertionfailure.js"],
    deps = [
        ":asserts",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "asserts",
    srcs = ["asserts.js"],
    deps = [
        ":jsunitexception",
        "//closure/goog/dom:safe",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/string",
        "//closure/goog/string:const",
    ],
)

closure_js_library(
    name = "asynctestcase",
    srcs = ["asynctestcase.js"],
    deps = [
        ":asserts",
        ":testcase",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "benchmark",
    testonly = 1,
    srcs = ["benchmark.js"],
    deps = [
        ":performancetable",
        ":performancetimer",
        ":testcase",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
    ],
)

closure_js_library(
    name = "continuationtestcase",
    srcs = ["continuationtestcase.js"],
    deps = [
        ":asserts",
        ":testcase",
        "//closure/goog/array",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
    ],
)

closure_js_library(
    name = "deferredtestcase",
    srcs = ["deferredtestcase.js"],
    deps = [
        ":asynctestcase",
        ":testcase",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "dom",
    srcs = ["dom.js"],
    deps = [
        ":asserts",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:nodeiterator",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagiterator",
        "//closure/goog/dom:tagname",
        "//closure/goog/iter",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "expectedfailures",
    srcs = ["expectedfailures.js"],
    deps = [
        ":asserts",
        ":jsunitexception",
        ":testcase",
        "//closure/goog/asserts",
        "//closure/goog/debug:divconsole",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:eventtype",
        "//closure/goog/log",
        "//closure/goog/style",
    ],
)

closure_js_library(
    name = "functionmock",
    srcs = ["functionmock.js"],
    deps = [
        ":loosemock",
        ":mock",
        ":mockinterface",
        ":propertyreplacer",
        ":strictmock",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "graphics",
    srcs = ["graphics.js"],
    deps = [
        ":asserts",
        "//closure/goog/graphics:path",
    ],
)

closure_js_library(
    name = "jstdasyncwrapper",
    srcs = ["jstdasyncwrapper.js"],
    deps = ["//closure/goog/promise"],
)

closure_js_library(
    name = "jstdtestcaseadapter",
    srcs = ["jstdtestcaseadapter.js"],
    deps = [
        ":jstdasyncwrapper",
        ":jsunit",
        ":testcase",
        "//closure/goog/async:run",
        "//closure/goog/functions",
    ],
)

closure_js_library(
    name = "jsunit",
    srcs = ["jsunit.js"],
    deps = [
        ":testcase",
        ":testrunner",
        "//closure/goog/dom:tagname",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "jsunitexception",
    srcs = ["jsunitexception.js"],
    deps = [":stacktrace"],
)

closure_js_library(
    name = "loosemock",
    srcs = ["loosemock.js"],
    deps = [
        ":mock",
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "mock",
    srcs = [
        "mock.js",
        "mockmatchers.js",
    ],
    deps = [
        ":asserts",
        ":jsunitexception",
        ":mockinterface",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/object",
        "//closure/goog/promise",
        "//closure/goog/promise:resolver",
    ],
)

closure_js_library(
    name = "mockclassfactory",
    srcs = ["mockclassfactory.js"],
    deps = [
        ":loosemock",
        ":mockmatchers",
        ":strictmock",
        ":testcase",
        "//closure/goog/array",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "mockclock",
    srcs = ["mockclock.js"],
    deps = [
        ":propertyreplacer",
        "//closure/goog/asserts",
        "//closure/goog/async:run",
        "//closure/goog/disposable",
        "//closure/goog/promise",
        "//closure/goog/promise:thenable",
        "//closure/goog/testing/events",
    ],
)

closure_js_library(
    name = "mockcontrol",
    srcs = ["mockcontrol.js"],
    deps = [
        ":functionmock",
        ":loosemock",
        ":mockinterface",
        ":strictmock",
        "//closure/goog/array",
        "//closure/goog/promise",
    ],
)

closure_js_library(
    name = "mockinterface",
    srcs = ["mockinterface.js"],
    deps = ["//closure/goog/promise"],
)

closure_js_library(
    name = "mockmatchers",
    exports = [":mock"],
)

closure_js_library(
    name = "mockrandom",
    srcs = ["mockrandom.js"],
    deps = ["//closure/goog/disposable"],
)

closure_js_library(
    name = "mockrange",
    srcs = ["mockrange.js"],
    deps = [
        ":loosemock",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:range",
    ],
)

closure_js_library(
    name = "mockstorage",
    srcs = ["mockstorage.js"],
)

closure_js_library(
    name = "mockuseragent",
    srcs = ["mockuseragent.js"],
    deps = [
        ":propertyreplacer",
        "//closure/goog/disposable",
        "//closure/goog/labs/useragent:util",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "multitestrunner",
    srcs = ["multitestrunner.js"],
    deps = [
        ":testcase",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/functions",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/timer",
        "//closure/goog/ui:component",
        "//closure/goog/ui:serverchart",
        "//closure/goog/ui:tablesorter",
    ],
)

closure_js_library(
    name = "objectpropertystring",
    srcs = ["objectpropertystring.js"],
)

closure_js_library(
    name = "objects",
    srcs = ["objects.js"],
    deps = [":asserts"],
)

closure_js_library(
    name = "parallel_closure_test_suite",
    testonly = 1,
    srcs = ["parallel_closure_test_suite.js"],
    deps = [
        ":multitestrunner",
        ":testcase",
        ":testsuite",
        "//closure/goog/asserts",
        "//closure/goog/events",
        "//closure/goog/json",
        "//closure/goog/promise",
    ],
)

closure_js_library(
    name = "performancetable",
    srcs = ["performancetable.js"],
    deps = [
        ":performancetimer",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/string:const",
    ],
)

closure_js_library(
    name = "performancetimer",
    srcs = ["performancetimer.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/math",
        "//closure/goog/promise",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "propertyreplacer",
    srcs = ["propertyreplacer.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "pseudorandom",
    srcs = ["pseudorandom.js"],
    deps = ["//closure/goog/disposable"],
)

closure_js_library(
    name = "recordfunction",
    srcs = ["recordfunction.js"],
    deps = [
        ":asserts",
        "//closure/goog/promise",
        "//closure/goog/promise:resolver",
    ],
)

closure_js_library(
    name = "shardingtestcase",
    srcs = ["shardingtestcase.js"],
    deps = [
        ":testcase",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "singleton",
    srcs = ["singleton.js"],
    deps = ["//closure/goog/singleton"],
)

closure_js_library(
    name = "stacktrace",
    srcs = ["stacktrace.js"],
)

closure_js_library(
    name = "strictmock",
    srcs = ["strictmock.js"],
    deps = [
        ":mock",
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "testcase",
    srcs = ["testcase.js"],
    deps = [
        ":asserts",
        ":cspviolationobserver",
        ":jsunitexception",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/debug",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/object",
        "//closure/goog/promise",
        "//closure/goog/url",
    ],
)

closure_js_library(
    name = "cspviolationobserver",
    srcs = ["cspviolationobserver.js"],
    deps = [":cspviolationobserver_externs"],
)

closure_js_library(
    name = "cspviolationobserver_externs",
    srcs = ["cspviolationobserver_externs.js"],
)

closure_js_library(
    name = "testqueue",
    srcs = ["testqueue.js"],
)

closure_js_library(
    name = "testrunner",
    srcs = ["testrunner.js"],
    deps = [
        ":cspviolationobserver",
        ":testcase",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/json",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "testsuite",
    srcs = ["testsuite.js"],
    deps = [
        ":testcase",
        "//closure/goog/labs/testing:environment",
    ],
)
