package(
    default_visibility = [
        "//visibility:private",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_test(
    name = "mem_random_access_file_test",
    srcs = ["mem_random_access_file_test.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/scann_ondevice/cc:mem_random_access_file",
        "@com_google_leveldb//:util",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
)

cc_test(
    name = "index_test",
    srcs = ["index_test.cc"],
    data = [
        "testdata/dummy_index.ldb",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/cc/test:test_utils",
        "//tensorflow_lite_support/scann_ondevice/cc:index",
        "//tensorflow_lite_support/scann_ondevice/cc/core:serialized_searcher_cc_proto",
        "//tensorflow_lite_support/scann_ondevice/proto:index_config_cc_proto",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
)

cc_test(
    name = "mem_writable_file_test",
    srcs = ["mem_writable_file_test.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/scann_ondevice/cc:mem_writable_file",
    ],
)

cc_test(
    name = "index_builder_test",
    srcs = ["index_builder_test.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/test:test_utils",
        "//tensorflow_lite_support/scann_ondevice/cc:index_builder",
        "//tensorflow_lite_support/scann_ondevice/proto:index_config_cc_proto",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_leveldb//:table",
        "@com_google_leveldb//:util",
    ],
)
