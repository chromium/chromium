load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "aes",
    srcs = ["aes.js"],
    deps = [
        ":blockcipher",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "arc4",
    srcs = ["arc4.js"],
    deps = ["//closure/goog/asserts"],
)

closure_js_library(
    name = "base64",
    srcs = ["base64.js"],
    deps = [
        ":crypt",
        "//closure/goog/asserts",
        "//closure/goog/string:internal",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product",
    ],
)

closure_js_library(
    name = "basen",
    srcs = ["basen.js"],
)

closure_js_library(
    name = "blobhasher",
    srcs = ["blobhasher.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/events:eventtarget",
        "//closure/goog/fs",
        "//closure/goog/log",
    ],
)

closure_js_library(
    name = "blockcipher",
    srcs = ["blockcipher.js"],
)

closure_js_library(
    name = "bytestring_perf",
    testonly = 1,
    srcs = ["bytestring_perf.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/testing:performancetable",
    ],
)

closure_js_library(
    name = "cbc",
    srcs = ["cbc.js"],
    deps = [
        ":blockcipher",
        ":crypt",
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "crypt",
    srcs = ["crypt.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "ctr",
    srcs = ["ctr.js"],
    deps = [
        ":crypt",
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "hash",
    srcs = ["hash.js"],
)

closure_js_library(
    name = "hash32",
    srcs = ["hash32.js"],
    deps = [":crypt"],
)

closure_js_library(
    name = "hashtester",
    testonly = 1,
    srcs = ["hashtester.js"],
    deps = [
        ":crypt",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/reflect",
        "//closure/goog/testing:asserts",
        "//closure/goog/testing:performancetable",
        "//closure/goog/testing:pseudorandom",
    ],
)

closure_js_library(
    name = "hmac",
    srcs = ["hmac.js"],
    deps = [":hash"],
)

closure_js_library(
    name = "md5",
    srcs = ["md5.js"],
    deps = [":hash"],
)

closure_js_library(
    name = "pbkdf2",
    srcs = ["pbkdf2.js"],
    deps = [
        ":crypt",
        ":hmac",
        ":sha1",
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "sha1",
    srcs = ["sha1.js"],
    deps = [":hash"],
)

closure_js_library(
    name = "sha2",
    srcs = ["sha2.js"],
    deps = [
        ":hash",
        "//closure/goog/array",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "sha2_64bit",
    srcs = ["sha2_64bit.js"],
    deps = [
        ":hash",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/math:long",
    ],
)

closure_js_library(
    name = "sha224",
    srcs = ["sha224.js"],
    deps = [":sha2"],
)

closure_js_library(
    name = "sha256",
    srcs = ["sha256.js"],
    deps = [":sha2"],
)

closure_js_library(
    name = "sha384",
    srcs = ["sha384.js"],
    deps = [":sha2_64bit"],
)

closure_js_library(
    name = "sha512",
    srcs = ["sha512.js"],
    deps = [":sha2_64bit"],
)

closure_js_library(
    name = "sha512_256",
    srcs = ["sha512_256.js"],
    deps = [":sha2_64bit"],
)
