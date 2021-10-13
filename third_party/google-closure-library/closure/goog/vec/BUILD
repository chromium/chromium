load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "float32array",
    srcs = ["float32array.js"],
)

closure_js_library(
    name = "float64array",
    srcs = ["float64array.js"],
)

closure_js_library(
    name = "mat3",
    srcs = ["mat3.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "mat3d",
    srcs = ["mat3d.js"],
    deps = [
        ":vec",
        ":vec3d",
    ],
)

closure_js_library(
    name = "mat3f",
    srcs = ["mat3f.js"],
    deps = [
        ":vec",
        ":vec3f",
    ],
)

closure_js_library(
    name = "mat4",
    srcs = ["mat4.js"],
    deps = [
        ":vec",
        ":vec3",
        ":vec4",
    ],
)

closure_js_library(
    name = "mat4d",
    srcs = ["mat4d.js"],
    deps = [
        ":quaternion",
        ":vec",
        ":vec3d",
        ":vec4d",
    ],
)

closure_js_library(
    name = "mat4f",
    srcs = ["mat4f.js"],
    deps = [
        ":quaternion",
        ":vec",
        ":vec3f",
        ":vec4f",
    ],
)

closure_js_library(
    name = "quaternion",
    srcs = ["quaternion.js"],
    deps = [
        ":vec",
        ":vec3",
        ":vec4",
    ],
)

closure_js_library(
    name = "ray",
    srcs = ["ray.js"],
    deps = [
        ":vec",
        ":vec3",
    ],
)

closure_js_library(
    name = "vec",
    srcs = ["vec.js"],
    deps = [
        ":float32array",
        ":float64array",
    ],
)

closure_js_library(
    name = "vec2",
    srcs = ["vec2.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "vec2d",
    srcs = ["vec2d.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "vec2f",
    srcs = ["vec2f.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "vec3",
    srcs = ["vec3.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "vec3d",
    srcs = ["vec3d.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "vec3f",
    srcs = ["vec3f.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "vec4",
    srcs = ["vec4.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "vec4d",
    srcs = ["vec4d.js"],
    deps = [":vec"],
)

closure_js_library(
    name = "vec4f",
    srcs = ["vec4f.js"],
    deps = [":vec"],
)
