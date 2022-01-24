# TensorFlow Lite Task Library - Text

load(
    "@org_tensorflow//tensorflow/lite/experimental/ios:ios.bzl",
    "TFL_MINIMUM_OS_VERSION",
    "tflite_ios_static_framework",
)
load(
    "//tensorflow_lite_support/ios:ios.bzl",
    "strip_c_api_include_path_prefix",
)

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

strip_c_api_include_path_prefix(
    name = "strip_c_api_include_path",
    hdr_labels = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:bert_nl_classifier_c_api.h",
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier_c_api.h",
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier_c_api_common.h",
        "//tensorflow_lite_support/cc/task/text/qa:bert_qa_c_api.h",
    ],
)

# This target builds a monolithic static framework for the TFLite Text API,
# which includes the TFLite runtime in it.
#
# bazel build -c opt --config=ios_fat //tensorflow_lite_support/ios:TensorFlowLiteTaskTextC_framework
tflite_ios_static_framework(
    name = "TensorFlowLiteTaskTextC_framework",
    hdrs = [
        ":bert_nl_classifier_c_api.h",
        ":bert_qa_c_api.h",
        ":nl_classifier_c_api.h",
        ":nl_classifier_c_api_common.h",
    ],
    allowlist_symbols_file = ":allowlist_TensorFlowLiteTaskText.txt",
    bundle_name = "TensorFlowLiteTaskTextC",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    deps = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:bert_nl_classifier_c_api",
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier_c_api",
        "//tensorflow_lite_support/cc/task/text/qa:bert_qa_c_api",
    ],
)
