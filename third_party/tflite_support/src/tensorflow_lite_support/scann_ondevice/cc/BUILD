package(
    default_visibility = [
        "//tensorflow_lite_support:users",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "mem_random_access_file",
    srcs = ["mem_random_access_file.cc"],
    hdrs = ["mem_random_access_file.h"],
    deps = [
        "@com_google_leveldb//:util",
    ],
)

cc_library(
    name = "index",
    srcs = ["index.cc"],
    hdrs = ["index.h"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":mem_random_access_file",
        ":utils",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/scann_ondevice/proto:index_config_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_leveldb//:table",
        "@com_google_leveldb//:util",
    ],
)

cc_library(
    name = "index_builder",
    srcs = ["index_builder.cc"],
    hdrs = ["index_builder.h"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":mem_writable_file",
        ":utils",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/scann_ondevice/cc/core:serialized_searcher_cc_proto",
        "//tensorflow_lite_support/scann_ondevice/proto:index_config_cc_proto",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@com_google_leveldb//:db",
        "@com_google_leveldb//:table",
        "@com_google_leveldb//:util",
    ],
)

cc_library(
    name = "mem_writable_file",
    srcs = ["mem_writable_file.cc"],
    hdrs = ["mem_writable_file.h"],
    deps = [
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:cord",
        "@com_google_leveldb//:util",
    ],
)

cc_library(
    name = "utils",
    hdrs = ["utils.h"],
    deps = ["@com_google_absl//absl/strings:str_format"],
)
