"""
Partial workspace defintion for the TFLite Support Library. See WORKSPACE for usage.
"""

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load("//third_party:repo.bzl", "third_party_http_archive")
load("//third_party/tensorflow:version.bzl", "TENSORFLOW_COMMIT", "TENSORFLOW_SHA256")

def tflite_support_workspace9():
    """Partial workspace definition for the TFLite Support Library. See WORKSPACE for usage."""

    http_file(
        name = "mobilebert_float",
        sha256 = "883bf5d40f0b0ae435326bb21ed0f4c9004b22c3fd1539383fd16d68623696dd",
        urls = ["https://tfhub.dev/tensorflow/lite-model/mobilebert/1/default/1?lite-format=tflite"],
    )

    http_file(
        name = "mobilebert_with_metadata",
        sha256 = "e79d3c70108bbdee02da657b679349cab46dbb859a05b599c76b53d98e82f272",
        urls = ["https://tfhub.dev/tensorflow/lite-model/mobilebert/1/metadata/1?lite-format=tflite"],
    )

    http_file(
        name = "30k-clean",
        sha256 = "fefb02b667a6c5c2fe27602d28e5fb3428f66ab89c7d6f388e7c8d44a02d0336",
        urls = ["https://storage.googleapis.com/download.tensorflow.org/models/tflite_support/bert_qa/30k-clean.model"],
    )

    http_file(
        name = "mobilebert_vocab",
        sha256 = "07eced375cec144d27c900241f3e339478dec958f92fddbc551f295c992038a3",
        urls = ["https://storage.googleapis.com/download.tensorflow.org/models/tflite_support/bert_qa/mobilebert_vocab.txt"],
    )

    http_file(
        name = "universal_sentence_encoder_qa_with_metadata",
        sha256 = "82c2d0450aa458adbec2f78eff33cfbf2a41b606b44246726ab67373926e32bc",
        urls = ["https://storage.googleapis.com/download.tensorflow.org/models/tflite_support/bert_qa/universal_sentence_encoder_qa_with_metadata.tflite"],
    )

    http_file(
        name = "albert",
        sha256 = "4a29c7063c518925960229f49dd03e8da5d6682001cf73037815dcd98afd728a",
        urls = ["https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/1?lite-format=tflite"],
    )

    http_file(
        name = "albert_with_metadata",
        sha256 = "8a8a91856b94b945e4a9f22f0332bbf105c3b6b878bb23abfc97eb89d3e8436a",
        urls = ["https://tfhub.dev/tensorflow/lite-model/albert_lite_base/squadv1/metadata/1?lite-format=tflite"],
    )

    http_file(
        name = "mobilebert_embedding_with_metadata",
        sha256 = "fa47142dcc6f446168bc672f2df9605b6da5d0c0d6264e9be62870282365b95c",
        urls = ["https://storage.googleapis.com/download.tensorflow.org/models/tflite_support/bert_embedder/mobilebert_embedding_with_metadata.tflite"],
    )

    http_file(
        name = "bert_clu_annotator_with_metadata",
        sha256 = "ad18e4b67e4c3f6563fdf9b59c62760fea276f6af7f351341e64cd460c39da19",
        urls = ["https://storage.googleapis.com/download.tensorflow.org/models/tflite_support/bert_clu/bert_clu_annotator_with_metadata.tflite"],
    )

    http_file(
        name = "bert_nl_classifier",
        sha256 = "1e5a550c09bff0a13e61858bcfac7654d7fcc6d42106b4f15e11117695069600",
        urls = ["https://storage.googleapis.com/download.tensorflow.org/models/tflite_support/bert_nl_classifier/bert_nl_classifier.tflite"],
    )

    http_file(
        name = "bert_nl_classifier_no_metadata",
        sha256 = "9b4554f6e28a72a3f40511964eed1ccf4e74cc074f81543cacca4faf169a173e",
        urls = ["https://storage.googleapis.com/download.tensorflow.org/models/tflite_support/bert_nl_classifier/bert_nl_classifier_no_metadata.tflite"],
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

    # GoogleTest/GoogleMock framework. Used by most unit-tests.
    http_archive(
        name = "com_google_googletest",
        urls = ["https://github.com/google/googletest/archive/4ec4cd23f486bf70efcc5d2caa40f24368f752e3.zip"],
        strip_prefix = "googletest-4ec4cd23f486bf70efcc5d2caa40f24368f752e3",
        sha256 = "de682ea824bfffba05b4e33b67431c247397d6175962534305136aa06f92e049",
    )

    # Apple and Swift rules.
    # https://github.com/bazelbuild/rules_apple/releases
    http_archive(
        name = "build_bazel_rules_apple",
        sha256 = "a5f00fd89eff67291f6cd3efdc8fad30f4727e6ebb90718f3f05bbf3c3dd5ed7",
        urls = [
            "https://github.com/bazelbuild/rules_apple/releases/download/0.33.0/rules_apple.0.33.0.tar.gz",
        ],
    )

    # https://github.com/bazelbuild/rules_swift/releases
    http_archive(
        name = "build_bazel_rules_swift",
        sha256 = "8a49da750560b185804a4bc95c82d3f9cc4c2caf788960b0e21945759155fdd9",
        urls = [
            "https://github.com/bazelbuild/rules_swift/releases/download/0.25.0/rules_swift.0.25.0.tar.gz",
        ],
    )

    http_archive(
        name = "org_tensorflow",
        sha256 = TENSORFLOW_SHA256,
        strip_prefix = "tensorflow-" + TENSORFLOW_COMMIT,
        urls = [
            "https://github.com/tensorflow/tensorflow/archive/" + TENSORFLOW_COMMIT +
            ".tar.gz",
        ],
        patches = [
            # We need to rename lite/ios/BUILD.apple to lite/ios/BUILD.
            Label("//third_party:tensorflow_lite_ios_build.patch"),
        ],
        patch_args = ["-p1"],
    )

    third_party_http_archive(
        name = "pybind11",
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/github.com/pybind/pybind11/archive/v2.7.1.tar.gz",
            "https://github.com/pybind/pybind11/archive/v2.7.1.tar.gz",
        ],
        sha256 = "616d1c42e4cf14fa27b2a4ff759d7d7b33006fdc5ad8fd603bb2c22622f27020",
        strip_prefix = "pybind11-2.7.1",
        build_file = "@pybind11_bazel//:pybind11.BUILD",
    )

    PP_COMMIT = "3594106f2df3d725e65015ffb4c7886d6eeee683"
    PP_SHA256 = "baa1f53568283630a5055c85f0898b8810f7a6431bd01bbaedd32b4c1defbcb1"
    http_archive(
        name = "pybind11_protobuf",
        sha256 = PP_SHA256,
        strip_prefix = "pybind11_protobuf-{commit}".format(commit = PP_COMMIT),
        urls = [
            "https://github.com/pybind/pybind11_protobuf/archive/{commit}.tar.gz".format(commit = PP_COMMIT),
        ],
    )

    http_archive(
        name = "com_google_protobuf",
        sha256 = "bb1ddd8172b745cbdc75f06841bd9e7c9de0b3956397723d883423abfab8e176",
        strip_prefix = "protobuf-3.18.0",
        # Patched to give visibility into private targets to pybind11_protobuf
        patches = [Label("//third_party/pybind11_protobuf:com_google_protobuf_build.patch")],
        urls = [
            "https://github.com/protocolbuffers/protobuf/archive/v3.18.0.zip",
        ],
        repo_mapping = {"@six": "@six_archive"},
    )

    http_archive(
        name = "absl_py",
        sha256 = "603febc9b95a8f2979a7bdb77d2f5e4d9b30d4e0d59579f88eba67d4e4cc5462",
        strip_prefix = "abseil-py-pypi-v0.9.0",
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/github.com/abseil/abseil-py/archive/pypi-v0.9.0.tar.gz",
            "https://github.com/abseil/abseil-py/archive/pypi-v0.9.0.tar.gz",
        ],
    )

    http_archive(
        name = "six_archive",
        build_file = Label("//third_party:six.BUILD"),
        sha256 = "d16a0141ec1a18405cd4ce8b4613101da75da0e9a7aec5bdd4fa804d0e0eba73",
        strip_prefix = "six-1.12.0",
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/pypi.python.org/packages/source/s/six/six-1.12.0.tar.gz",
            "https://pypi.python.org/packages/source/s/six/six-1.12.0.tar.gz",
        ],
    )

    http_archive(
        name = "com_google_sentencepiece",
        strip_prefix = "sentencepiece-1.0.0",
        sha256 = "c05901f30a1d0ed64cbcf40eba08e48894e1b0e985777217b7c9036cac631346",
        urls = [
            "https://github.com/google/sentencepiece/archive/1.0.0.zip",
        ],
    )

    # TODO(b/238430210): Update RE2 depedency and remove the patch.
    http_archive(
        name = "org_tensorflow_text",
        sha256 = "f64647276f7288d1b1fe4c89581d51404d0ce4ae97f2bcc4c19bd667549adca8",
        strip_prefix = "text-2.2.0",
        urls = [
            "https://github.com/tensorflow/text/archive/v2.2.0.zip",
        ],
        patches = [
            Label("//third_party:tensorflow_text_remove_tf_deps.patch"),
            Label("//third_party:tensorflow_text_a0f49e63.patch"),
        ],
        patch_args = ["-p1"],
        repo_mapping = {"@com_google_re2": "@com_googlesource_code_re2"},
    )

    http_archive(
        name = "com_googlesource_code_re2",
        sha256 = "e06b718c129f4019d6e7aa8b7631bee38d3d450dd980246bfaf493eb7db67868",
        strip_prefix = "re2-fe4a310131c37f9a7e7f7816fa6ce2a8b27d65a8",
        urls = [
            "https://github.com/google/re2/archive/fe4a310131c37f9a7e7f7816fa6ce2a8b27d65a8.tar.gz",
        ],
    )

    # ABSL cpp library lts_2021_03_24 Patch2
    # See https://github.com/abseil/abseil-cpp/releases for details.
    # Needed for absl/status and absl/status:statusor
    http_archive(
        name = "com_google_absl",
        build_file = Label("//third_party:com_google_absl.BUILD"),
        urls = [
            "https://github.com/abseil/abseil-cpp/archive/20210324.2.tar.gz",
        ],
        # Remove after https://github.com/abseil/abseil-cpp/issues/326 is solved.
        patches = [
            Label("//third_party:com_google_absl_f863b622fe13612433fdf43f76547d5edda0c93001.diff"),
        ],
        patch_args = [
            "-p1",
        ],
        strip_prefix = "abseil-cpp-20210324.2",
        sha256 = "59b862f50e710277f8ede96f083a5bb8d7c9595376146838b9580be90374ee1f",
    )

    http_archive(
        name = "com_google_glog",
        sha256 = "50a05b9119802beffe6ec9f8302aa1ab770db10f2297b659b8e8f15e55854aed",
        strip_prefix = "glog-c515e1ae2fc8b36ca19362842f9347e9429be7ad",
        urls = [
            "https://mirror.bazel.build/github.com/google/glog/archive/c515e1ae2fc8b36ca19362842f9347e9429be7ad.tar.gz",
            "https://github.com/google/glog/archive/c515e1ae2fc8b36ca19362842f9347e9429be7ad.tar.gz",
        ],
    )

    http_archive(
        name = "com_github_gflags_gflags",
        sha256 = "34af2f15cf7367513b352bdcd2493ab14ce43692d2dcd9dfc499492966c64dcf",
        strip_prefix = "gflags-2.2.2",
        urls = [
            "http://mirror.tensorflow.org/github.com/gflags/gflags/archive/v2.2.2.tar.gz",
            "https://github.com/gflags/gflags/archive/v2.2.2.tar.gz",
        ],
        patches = [Label("//third_party:gflags_fix_android_pthread.diff")],
        patch_args = ["-p1"],
    )

    http_archive(
        name = "zlib",
        build_file = Label("//third_party:zlib.BUILD"),
        sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
        strip_prefix = "zlib-1.2.11",
        urls = [
            "http://mirror.bazel.build/zlib.net/fossils/zlib-1.2.11.tar.gz",
            "http://zlib.net/fossils/zlib-1.2.11.tar.gz",  # 2017-01-15
        ],
        patches = [
            Label("//third_party:zlib.patch"),
        ],
        patch_args = [
            "-p1",
        ],
    )

    http_archive(
        name = "libyuv",
        urls = ["https://chromium.googlesource.com/libyuv/libyuv/+archive/39240f7149cffde62e3620344d222c8ab2c21178.tar.gz"],
        # Adding the constrain of sha256 and strip_prefix will cause failure as of
        # Jan 2021. It seems that the downloaded libyuv was different every time,
        # so that the specified sha256 and strip_prefix cannot match.
        # sha256 = "01c2e30eb8e83880f9ba382f6bece9c38cd5b07f9cadae46ef1d5a69e07fafaf",
        # strip_prefix = "libyuv-39240f7149cffde62e3620344d222c8ab2c21178",
        build_file = Label("//third_party:libyuv.BUILD"),
    )

    http_archive(
        name = "stblib",
        strip_prefix = "stb-b42009b3b9d4ca35bc703f5310eedc74f584be58",
        sha256 = "13a99ad430e930907f5611325ec384168a958bf7610e63e60e2fd8e7b7379610",
        urls = ["https://github.com/nothings/stb/archive/b42009b3b9d4ca35bc703f5310eedc74f584be58.tar.gz"],
        build_file = Label("//third_party:stblib.BUILD"),
    )

    http_archive(
        name = "google_toolbox_for_mac",
        url = "https://github.com/google/google-toolbox-for-mac/archive/v2.2.1.zip",
        sha256 = "e3ac053813c989a88703556df4dc4466e424e30d32108433ed6beaec76ba4fdc",
        strip_prefix = "google-toolbox-for-mac-2.2.1",
        build_file = Label("//third_party:google_toolbox_for_mac.BUILD"),
    )

    http_archive(
        name = "utf_archive",
        build_file = Label("//third_party:utf.BUILD"),
        sha256 = "262a902f622dcd28e05b8a4be10da0aa3899050d0be8f4a71780eed6b2ea65ca",
        urls = [
            "https://mirror.bazel.build/9fans.github.io/plan9port/unix/libutf.tgz",
            "https://9fans.github.io/plan9port/unix/libutf.tgz",
        ],
    )

    http_archive(
        name = "icu",
        strip_prefix = "icu-release-64-2",
        sha256 = "dfc62618aa4bd3ca14a3df548cd65fe393155edd213e49c39f3a30ccd618fc27",
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/github.com/unicode-org/icu/archive/release-64-2.zip",
            "https://github.com/unicode-org/icu/archive/release-64-2.zip",
        ],
        build_file = Label("//third_party:icu.BUILD"),
    )

    http_archive(
        name = "fft2d",
        build_file = Label("//third_party/fft2d:fft2d.BUILD"),
        sha256 = "5f4dabc2ae21e1f537425d58a49cdca1c49ea11db0d6271e2a4b27e9697548eb",
        strip_prefix = "OouraFFT-1.0",
        urls = [
            "https://storage.googleapis.com/mirror.tensorflow.org/github.com/petewarden/OouraFFT/archive/v1.0.tar.gz",
            "https://github.com/petewarden/OouraFFT/archive/v1.0.tar.gz",
        ],
    )

    http_archive(
        name = "darts_clone",
        build_file = Label("//third_party:darts_clone.BUILD"),
        sha256 = "c97f55d05c98da6fcaf7f9ecc6a6dc6bc5b18b8564465f77abff8879d446491c",
        strip_prefix = "darts-clone-e40ce4627526985a7767444b6ed6893ab6ff8983",
        urls = [
            "https://github.com/s-yata/darts-clone/archive/e40ce4627526985a7767444b6ed6893ab6ff8983.zip",
        ],
    )

    http_archive(
        name = "libedgetpu",
        sha256 = "a179016a5874c58db969a5edd3fecf57610604e751b5c4d6d82ad58c383ffd64",
        strip_prefix = "libedgetpu-ea1eaddbddece0c9ca1166e868f8fd03f4a3199e",
        urls = [
            "https://github.com/google-coral/libedgetpu/archive/ea1eaddbddece0c9ca1166e868f8fd03f4a3199e.tar.gz",
        ],
    )

    http_archive(
        name = "eigen",
        sha256 = "b4c198460eba6f28d34894e3a5710998818515104d6e74e5cc331ce31e46e626",
        strip_prefix = "eigen-3.4.0",
        urls = [
            "https://gitlab.com/libeigen/eigen/-/archive/3.4.0/eigen-3.4.0.tar.bz2",
        ],
        build_file = Label("//third_party:eigen3.BUILD"),
    )

    http_archive(
        name = "com_google_leveldb",
        build_file = Label("//third_party:leveldb.BUILD"),
        patch_cmds = [
            """mkdir leveldb; cp include/leveldb/* leveldb""",
        ],
        sha256 = "9a37f8a6174f09bd622bc723b55881dc541cd50747cbd08831c2a82d620f6d76",
        strip_prefix = "leveldb-1.23",
        urls = [
            "https://github.com/google/leveldb/archive/refs/tags/1.23.tar.gz",
        ],
    )

    http_archive(
        name = "snappy",
        build_file = Label("//third_party:snappy.BUILD"),
        sha256 = "16b677f07832a612b0836178db7f374e414f94657c138e6993cbfc5dcc58651f",
        strip_prefix = "snappy-1.1.8",
        urls = ["https://github.com/google/snappy/archive/1.1.8.tar.gz"],
    )
