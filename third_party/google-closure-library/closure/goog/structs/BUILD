load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "avltree",
    srcs = ["avltree.js"],
    deps = [
        ":collection",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "circularbuffer",
    srcs = ["circularbuffer.js"],
)

closure_js_library(
    name = "collection",
    srcs = ["collection.js"],
)

closure_js_library(
    name = "heap",
    srcs = ["heap.js"],
    deps = [
        ":node",
        "//closure/goog/array",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "inversionmap",
    srcs = ["inversionmap.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "linkedmap",
    srcs = ["linkedmap.js"],
    deps = [":map"],
)

closure_js_library(
    name = "map",
    srcs = ["map.js"],
    deps = [
        "//closure/goog/collections:iters",
        "//closure/goog/iter",
        "//closure/goog/iter:es6",
    ],
)

closure_js_library(
    name = "node",
    srcs = ["node.js"],
)

closure_js_library(
    name = "pool",
    srcs = ["pool.js"],
    deps = [
        ":queue",
        ":set",
        "//closure/goog/disposable",
    ],
)

closure_js_library(
    name = "prioritypool",
    srcs = ["prioritypool.js"],
    deps = [
        ":pool",
        ":priorityqueue",
    ],
)

closure_js_library(
    name = "priorityqueue",
    srcs = ["priorityqueue.js"],
    deps = [":heap"],
)

closure_js_library(
    name = "quadtree",
    srcs = ["quadtree.js"],
    deps = ["//closure/goog/math:coordinate"],
)

closure_js_library(
    name = "queue",
    srcs = ["queue.js"],
    deps = ["//closure/goog/array"],
)

closure_js_library(
    name = "set",
    srcs = ["set.js"],
    deps = [
        ":collection",
        ":map",
        ":structs",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "simplepool",
    srcs = ["simplepool.js"],
    deps = ["//closure/goog/disposable"],
)

closure_js_library(
    name = "stringset",
    srcs = ["stringset.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "structs",
    srcs = ["structs.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "treenode",
    srcs = ["treenode.js"],
    deps = [
        ":node",
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "trie",
    srcs = ["trie.js"],
    deps = [
        ":structs",
        "//closure/goog/object",
    ],
)
