workspace(name = "org_tensorflow_text")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "icu",
    strip_prefix = "icu-release-64-2",
    sha256 = "dfc62618aa4bd3ca14a3df548cd65fe393155edd213e49c39f3a30ccd618fc27",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/unicode-org/icu/archive/release-64-2.zip",
        "https://github.com/unicode-org/icu/archive/release-64-2.zip",
    ],
    build_file = "//third_party/icu:BUILD.bzl",
    patches = ["//third_party/icu:udata.patch"],
    patch_args = ["-p1"],
)

http_archive(
    name = "com_google_sentencepiece",
    strip_prefix = "sentencepiece-1.0.0",
    sha256 = "c05901f30a1d0ed64cbcf40eba08e48894e1b0e985777217b7c9036cac631346",
    urls = [
        "https://github.com/google/sentencepiece/archive/1.0.0.zip"
    ],
    patches = ["//third_party/sentencepiece:processor.patch"],
    patch_args = ["-p1"],
)

http_archive(
    name = "darts_clone",
    build_file = "//third_party/darts_clone:BUILD.bzl",
    sha256 = "c97f55d05c98da6fcaf7f9ecc6a6dc6bc5b18b8564465f77abff8879d446491c",
    strip_prefix = "darts-clone-e40ce4627526985a7767444b6ed6893ab6ff8983",
    urls = [
        "https://github.com/s-yata/darts-clone/archive/e40ce4627526985a7767444b6ed6893ab6ff8983.zip",
    ],
)

http_archive(
    name = "io_bazel_rules_closure",
    sha256 = "5b00383d08dd71f28503736db0500b6fb4dda47489ff5fc6bed42557c07c6ba9",
    strip_prefix = "rules_closure-308b05b2419edb5c8ee0471b67a40403df940149",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/bazelbuild/rules_closure/archive/308b05b2419edb5c8ee0471b67a40403df940149.tar.gz",
        "https://github.com/bazelbuild/rules_closure/archive/308b05b2419edb5c8ee0471b67a40403df940149.tar.gz",  # 2019-06-13
    ],
)

# NOTE: according to
# https://docs.bazel.build/versions/master/external.html#transitive-dependencies
# we should list the transitive dependencies of @org_tensorflow_hub in this
# WORKSPACE file.  Still, all of them are already listed by tf_workspace() which
# is called later in this file.
http_archive(
    name = "org_tensorflow_hub",
    strip_prefix = "hub-0.8.0",
    sha256 = "968af30c448d51c36501b68df2c916fb4a61007db3240adc9248fa3a9be2da6f",
    urls = [
        "https://github.com/tensorflow/hub/archive/v0.8.0.zip"
    ],
)

http_archive(
    name = "org_tensorflow",
    strip_prefix = "tensorflow-2.7.0",
    sha256 = "249b48ddee927801c7a4f8e5442cf1a3c860f6f46b85a2ff7a78b501507dd561",
    urls = [
        "https://github.com/tensorflow/tensorflow/archive/v2.7.0.zip"
    ],
)

http_archive(
    name = "org_tensorflow_datasets",
    sha256 = "c6ff4e2306387f0ca45d4f616d9a1c5e79e02ef16d0a8958230a8049ea07fc98",
    strip_prefix = "datasets-3.2.0",
    urls = [
        "https://github.com/tensorflow/datasets/archive/v3.2.0.zip",
    ],
)

http_archive(
    name = "pybind11",
    strip_prefix = "pybind11-2.6.0",
    sha256 = "90b705137b69ee3b5fc655eaca66d0dc9862ea1759226f7ccd3098425ae69571",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/pybind/pybind11/archive/v2.6.0.tar.gz",
        "https://github.com/pybind/pybind11/archive/v2.6.0.tar.gz",
    ],
   build_file = "//third_party/pybind11:BUILD.bzl",
)

# Initialize TensorFlow dependencies.
load("@org_tensorflow//tensorflow:workspace3.bzl", "tf_workspace3")
tf_workspace3()
load("@org_tensorflow//tensorflow:workspace2.bzl", "tf_workspace2")
tf_workspace2()
load("@org_tensorflow//tensorflow:workspace1.bzl", "tf_workspace1")
tf_workspace1()
load("@org_tensorflow//tensorflow:workspace0.bzl", "tf_workspace0")
tf_workspace0()

load("//third_party/tensorflow:tf_configure.bzl", "tf_configure")

tf_configure(name = "local_config_tf")

# Set up Android.
load("@org_tensorflow//third_party/android:android_configure.bzl", "android_configure")
android_configure(name="local_config_android")
load("@local_config_android//:android.bzl", "android_workspace")
android_workspace()
