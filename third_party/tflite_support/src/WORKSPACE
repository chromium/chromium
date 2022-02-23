workspace(name = "org_tensorflow_lite_support")

load("@bazel_tools//tools/build_defs/repo:java.bzl", "java_import_external")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_file")
load("@//third_party/py:python_configure.bzl", "python_configure")

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
    sha256 = "0052d452af7742c8f3a4e0929763388a66403de363775db7e90adecb2ba4944b",
    urls = [
        "https://github.com/bazelbuild/rules_apple/releases/download/0.31.3/rules_apple.0.31.3.tar.gz",
    ],
)

# https://github.com/bazelbuild/rules_swift/releases
http_archive(
    name = "build_bazel_rules_swift",
    sha256 = "8407fa0fd04a7ce1d6bb95e90b216404466f809eda459c23cb57b5fa1ef9d639",
    urls = [
        "https://github.com/bazelbuild/rules_swift/releases/download/0.21.0/rules_swift.0.21.0.tar.gz",
    ],
)

# TF on 2022-01-28.
TENSORFLOW_COMMIT = "f2c2144d767a64236261fb4e4dd45947bd5f5815"
TENSORFLOW_SHA256 = "32ba2f6ea07572fd05cdae7520fe1bc38409f1a21bb4524076df27f1e23d09c1"
http_archive(
    name = "org_tensorflow",
    sha256 = TENSORFLOW_SHA256,
    strip_prefix = "tensorflow-" + TENSORFLOW_COMMIT,
    urls = [
        "https://github.com/tensorflow/tensorflow/archive/" + TENSORFLOW_COMMIT
        + ".tar.gz",
    ],
    patches = [
        # We need to rename lite/ios/BUILD.apple to lite/ios/BUILD.
        "@//third_party:tensorflow_lite_ios_build.patch",
    ],
    patch_args = ["-p1"],
)

# Set up dependencies. Need to do this before set up TF so that our modification
# could take effects.
load("//third_party:repo.bzl", "third_party_http_archive")

# Use our patched gflags which fixes a linking issue.
load("//third_party/gflags:workspace.bzl", gflags = "repo")
gflags()

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

# TODO(b/211393391): Updates the commit number once the new change is ready.
PP_COMMIT = "30f02dd9ccd2fc7046c36ed913ed510fd1aa7301"
PP_SHA256 = "178bcd587956b0f449fff2f46e663dc10baa6d4951a0a7f48cddfeef57d593a8"
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
    patches = ["//third_party/pybind11_protobuf:com_google_protobuf_build.patch"],
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
    build_file = "//third_party:six.BUILD",
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

http_archive(
    name = "org_tensorflow_text",
    sha256 = "f64647276f7288d1b1fe4c89581d51404d0ce4ae97f2bcc4c19bd667549adca8",
    strip_prefix = "text-2.2.0",
    urls = [
        "https://github.com/tensorflow/text/archive/v2.2.0.zip",
    ],
    patches = ["@//third_party:tensorflow_text_remove_tf_deps.patch"],
    patch_args = ["-p1"],
    repo_mapping = {"@com_google_re2": "@com_googlesource_code_re2"},
)

http_archive(
    name = "com_googlesource_code_re2",
    sha256 = "d070e2ffc5476c496a6a872a6f246bfddce8e7797d6ba605a7c8d72866743bf9",
    strip_prefix = "re2-506cfa4bffd060c06ec338ce50ea3468daa6c814",
    urls = [
        "https://github.com/google/re2/archive/506cfa4bffd060c06ec338ce50ea3468daa6c814.tar.gz",
    ],
)

# ABSL cpp library lts_2021_03_24 Patch2
# See https://github.com/abseil/abseil-cpp/releases for details.
# Needed for absl/status and absl/status:statusor
http_archive(
    name = "com_google_absl",
    build_file = "//third_party:com_google_absl.BUILD",
    urls = [
        "https://github.com/abseil/abseil-cpp/archive/20210324.2.tar.gz",
    ],
    # Remove after https://github.com/abseil/abseil-cpp/issues/326 is solved.
    patches = [
        "@//third_party:com_google_absl_f863b622fe13612433fdf43f76547d5edda0c93001.diff"
    ],
    patch_args = [
        "-p1",
    ],
    strip_prefix = "abseil-cpp-20210324.2",
    sha256 = "59b862f50e710277f8ede96f083a5bb8d7c9595376146838b9580be90374ee1f"
)

http_archive(
    name = "com_google_glog",
    sha256 = "1ee310e5d0a19b9d584a855000434bb724aa744745d5b8ab1855c85bff8a8e21",
    strip_prefix = "glog-028d37889a1e80e8a07da1b8945ac706259e5fd8",
    urls = [
        "https://mirror.bazel.build/github.com/google/glog/archive/028d37889a1e80e8a07da1b8945ac706259e5fd8.tar.gz",
        "https://github.com/google/glog/archive/028d37889a1e80e8a07da1b8945ac706259e5fd8.tar.gz",
    ],
)


http_archive(
    name = "zlib",
    build_file = "//third_party:zlib.BUILD",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = [
        "http://mirror.bazel.build/zlib.net/fossils/zlib-1.2.11.tar.gz",
        "http://zlib.net/fossils/zlib-1.2.11.tar.gz",  # 2017-01-15
    ],
)

http_archive(
    name = "org_libzip",
    build_file = "//third_party:libzip.BUILD",
    sha256 = "a5d22f0c87a2625450eaa5e10db18b8ee4ef17042102d04c62e311993a2ba363",
    strip_prefix = "libzip-rel-1-5-1",
    urls = [
        # Bazel does not like the official download link at libzip.org,
        # so use the GitHub release tag.
        "https://mirror.bazel.build/github.com/nih-at/libzip/archive/rel-1-5-1.zip",
        "https://github.com/nih-at/libzip/archive/rel-1-5-1.zip",
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
    build_file = "//third_party:libyuv.BUILD",
)

http_archive(
    name = "stblib",
    strip_prefix = "stb-b42009b3b9d4ca35bc703f5310eedc74f584be58",
    sha256 = "13a99ad430e930907f5611325ec384168a958bf7610e63e60e2fd8e7b7379610",
    urls = ["https://github.com/nothings/stb/archive/b42009b3b9d4ca35bc703f5310eedc74f584be58.tar.gz"],
    build_file = "//third_party:stblib.BUILD",
)

http_archive(
    name = "google_toolbox_for_mac",
    url = "https://github.com/google/google-toolbox-for-mac/archive/v2.2.1.zip",
    sha256 = "e3ac053813c989a88703556df4dc4466e424e30d32108433ed6beaec76ba4fdc",
    strip_prefix = "google-toolbox-for-mac-2.2.1",
    build_file = "@//third_party:google_toolbox_for_mac.BUILD",
)

http_archive(
    name = "utf_archive",
    build_file = "@//third_party:utf.BUILD",
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
    build_file = "@//third_party:icu.BUILD",
)

http_archive(
    name = "fft2d",
    build_file = "@//third_party/fft2d:fft2d.BUILD",
    sha256 = "5f4dabc2ae21e1f537425d58a49cdca1c49ea11db0d6271e2a4b27e9697548eb",
    strip_prefix = "OouraFFT-1.0",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/petewarden/OouraFFT/archive/v1.0.tar.gz",
        "https://github.com/petewarden/OouraFFT/archive/v1.0.tar.gz",
    ],
)

http_archive(
    name = "darts_clone",
    build_file = "@//third_party:darts_clone.BUILD",
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
    "https://github.com/google-coral/libedgetpu/archive/ea1eaddbddece0c9ca1166e868f8fd03f4a3199e.tar.gz"
  ],
)

# Set up TensorFlow version for Coral.
load("@libedgetpu//:workspace.bzl", "libedgetpu_dependencies")
libedgetpu_dependencies(TENSORFLOW_COMMIT, TENSORFLOW_SHA256)

# AutoValue 1.6+ shades Guava, Auto Common, and JavaPoet. That's OK
# because none of these jars become runtime dependencies.
java_import_external(
    name = "com_google_auto_value",
    jar_sha256 = "fd811b92bb59ae8a4cf7eb9dedd208300f4ea2b6275d726e4df52d8334aaae9d",
    jar_urls = [
        "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/auto/value/auto-value/1.6/auto-value-1.6.jar",
        "https://repo1.maven.org/maven2/com/google/auto/value/auto-value/1.6/auto-value-1.6.jar",
    ],
    licenses = ["notice"],  # Apache 2.0
    generated_rule_name = "processor",
    exports = ["@com_google_auto_value_annotations"],
    extra_build_file_content = "\n".join([
        "java_plugin(",
        "    name = \"AutoAnnotationProcessor\",",
        "    output_licenses = [\"unencumbered\"],",
        "    processor_class = \"com.google.auto.value.processor.AutoAnnotationProcessor\",",
        "    tags = [\"annotation=com.google.auto.value.AutoAnnotation;genclass=${package}.AutoAnnotation_${outerclasses}${classname}_${methodname}\"],",
        "    deps = [\":processor\"],",
        ")",
        "",
        "java_plugin(",
        "    name = \"AutoOneOfProcessor\",",
        "    output_licenses = [\"unencumbered\"],",
        "    processor_class = \"com.google.auto.value.processor.AutoOneOfProcessor\",",
        "    tags = [\"annotation=com.google.auto.value.AutoValue;genclass=${package}.AutoOneOf_${outerclasses}${classname}\"],",
        "    deps = [\":processor\"],",
        ")",
        "",
        "java_plugin(",
        "    name = \"AutoValueProcessor\",",
        "    output_licenses = [\"unencumbered\"],",
        "    processor_class = \"com.google.auto.value.processor.AutoValueProcessor\",",
        "    tags = [\"annotation=com.google.auto.value.AutoValue;genclass=${package}.AutoValue_${outerclasses}${classname}\"],",
        "    deps = [\":processor\"],",
        ")",
        "",
        "java_library(",
        "    name = \"com_google_auto_value\",",
        "    exported_plugins = [",
        "        \":AutoAnnotationProcessor\",",
        "        \":AutoOneOfProcessor\",",
        "        \":AutoValueProcessor\",",
        "    ],",
        "    exports = [\"@com_google_auto_value_annotations\"],",
        ")",
    ]),
)

# Auto value annotations
java_import_external(
    name = "com_google_auto_value_annotations",
    jar_sha256 = "d095936c432f2afc671beaab67433e7cef50bba4a861b77b9c46561b801fae69",
    jar_urls = [
        "https://mirror.bazel.build/repo1.maven.org/maven2/com/google/auto/value/auto-value-annotations/1.6/auto-value-annotations-1.6.jar",
        "https://repo1.maven.org/maven2/com/google/auto/value/auto-value-annotations/1.6/auto-value-annotations-1.6.jar",
    ],
    licenses = ["notice"],  # Apache 2.0
    neverlink = True,
    default_visibility = ["@com_google_auto_value//:__pkg__"],
)

http_archive(
    name = "robolectric",
    urls = ["https://github.com/robolectric/robolectric-bazel/archive/4.7.3.tar.gz"],
    strip_prefix = "robolectric-bazel-4.7.3",
)
load("@robolectric//bazel:robolectric.bzl", "robolectric_repositories")
robolectric_repositories()

load("//third_party/flatbuffers:workspace.bzl", flatbuffers = "repo")

flatbuffers()

RULES_JVM_EXTERNAL_TAG = "4.2"

http_archive(
    name = "rules_jvm_external",
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    sha256 = "cd1a77b7b02e8e008439ca76fd34f5b07aecb8c752961f9640dea15e9e5ba1ca",
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/refs/tags/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:defs.bzl", "maven_install")

# Set up TF.
load("@org_tensorflow//tensorflow:workspace3.bzl", "workspace")
workspace()
load("@org_tensorflow//tensorflow:workspace2.bzl", "workspace")  # buildifier: disable=load
workspace()
load("@org_tensorflow//tensorflow:workspace1.bzl", "workspace")  # buildifier: disable=load
workspace()
load("@org_tensorflow//tensorflow:workspace0.bzl", "workspace")  # buildifier: disable=load
workspace()

load("//third_party/tensorflow:tf_configure.bzl", "tf_configure")
tf_configure(name = "local_config_tf")

# TF submodule compilation doesn't take care of grpc deps. Do it manually here.
load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")
grpc_deps()

load(
    "@build_bazel_rules_apple//apple:repositories.bzl",
    "apple_rules_dependencies",
)
apple_rules_dependencies()

load(
    "@build_bazel_apple_support//lib:repositories.bzl",
    "apple_support_dependencies",
)
apple_support_dependencies()

load("@upb//bazel:repository_defs.bzl", "bazel_version_repository")
bazel_version_repository(name = "bazel_version")

python_configure(name = "local_config_python")

ATS_TAG = "androidx-test-1.3.0"
http_archive(
    name = "android_test_support",
    strip_prefix = "android-test-%s" % ATS_TAG,
    urls = ["https://github.com/android/android-test/archive/%s.tar.gz" % ATS_TAG],
)
load("@android_test_support//:repo.bzl", "android_test_repositories")
android_test_repositories()

# Maven dependencies.

maven_install(
    artifacts = [
        "androidx.annotation:annotation:aar:1.1.0",
        "androidx.annotation:annotation-experimental:1.1.0",
        "androidx.multidex:multidex:jar:2.0.1",
        "androidx.test:core:jar:1.3.0",
        "androidx.test.ext:junit:jar:1.1.2",
        "androidx.test:runner:jar:1.3.0",
        "com.google.android.odml:image:aar:1.0.0-beta1",
        "com.google.truth:truth:jar:1.1",
        "commons-io:commons-io:jar:2.8.0",
        # Mockito >= 3.4.6 cannot pass bazel desugar.
        "org.mockito:mockito-android:jar:3.0.0",
        "org.mockito:mockito-core:jar:3.0.0",
        "org.mockito:mockito-inline:jar:3.0.0",
        "org.robolectric:robolectric:jar:4.7.3",
        "junit:junit:jar:4.13",
    ],
    repositories = [
        "https://maven.google.com",
        "https://dl.google.com/dl/android/maven2",
        "https://repo1.maven.org/maven2",
    ],
    fetch_sources = True,
    version_conflict_policy = "pinned",
)

http_archive(
    name = "tf_toolchains",
    sha256 = "d72b2e52baf0592f5b94347b128ef75422fc22f63dfcf2d5fd46bc732cab052b",
    strip_prefix = "toolchains-1.3.0",
    urls = [
        "http://mirror.tensorflow.org/github.com/tensorflow/toolchains/archive/v1.3.0.tar.gz",
        "https://github.com/tensorflow/toolchains/archive/v1.3.0.tar.gz",
    ],
)

load("@tf_toolchains//toolchains/embedded/arm-linux:arm_linux_toolchain_configure.bzl", "arm_linux_toolchain_configure")

# TFLite crossbuild toolchain for embeddeds Linux
arm_linux_toolchain_configure(
    name = "local_config_embedded_arm",
    build_file = "@tf_toolchains//toolchains/embedded/arm-linux:BUILD",
    aarch64_repo = "../aarch64_linux_toolchain",
    armhf_repo = "../armhf_linux_toolchain",
)
