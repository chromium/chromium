workspace(name = "org_tensorflow_lite_support")

load("@bazel_tools//tools/build_defs/repo:java.bzl", "java_import_external")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")
load("@//third_party/py:python_configure.bzl", "python_configure")

http_archive(
    name = "io_bazel_rules_closure",
    sha256 = "5b00383d08dd71f28503736db0500b6fb4dda47489ff5fc6bed42557c07c6ba9",
    strip_prefix = "rules_closure-308b05b2419edb5c8ee0471b67a40403df940149",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/bazelbuild/rules_closure/archive/308b05b2419edb5c8ee0471b67a40403df940149.tar.gz",
        "https://github.com/bazelbuild/rules_closure/archive/308b05b2419edb5c8ee0471b67a40403df940149.tar.gz",  # 2019-06-13
    ],
)

# Apple and Swift rules.
# https://github.com/bazelbuild/rules_apple/releases
http_archive(
    name = "build_bazel_rules_apple",
    sha256 = "ee9e6073aeb5a65c100cb9c44b0017c937706a4ae03176e14a7e78620a198079",
    strip_prefix = "rules_apple-5131f3d46794bf227d296c82f30c2499c9de3c5b",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/bazelbuild/rules_apple/archive/5131f3d46794bf227d296c82f30c2499c9de3c5b.tar.gz",
        "https://github.com/bazelbuild/rules_apple/archive/5131f3d46794bf227d296c82f30c2499c9de3c5b.tar.gz",
    ],
)

# https://github.com/bazelbuild/rules_swift/releases
http_archive(
    name = "build_bazel_rules_swift",
    sha256 = "d0833bc6dad817a367936a5f902a0c11318160b5e80a20ece35fb85a5675c886",
    strip_prefix = "rules_swift-3eeeb53cebda55b349d64c9fc144e18c5f7c0eb8",
    urls = [
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/bazelbuild/rules_swift/archive/3eeeb53cebda55b349d64c9fc144e18c5f7c0eb8.tar.gz",
        "https://github.com/bazelbuild/rules_swift/archive/3eeeb53cebda55b349d64c9fc144e18c5f7c0eb8.tar.gz",
    ],
)

# tf-nightly-20200810
http_archive(
    name = "org_tensorflow",
    sha256 = "fc6d7c57cd9427e695a38ad00fb6ecc3f623bac792dd44ad73a3f85b338b68be",
    strip_prefix = "tensorflow-8a4ffe2e1ae722cff5306778df0cfca8b7f503fe",
    urls = [
        "https://github.com/tensorflow/tensorflow/archive/8a4ffe2e1ae722cff5306778df0cfca8b7f503fe.tar.gz",
    ],
    patches = ["@//third_party:tensorflow_lite_ios_build.patch"],
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
        "https://storage.googleapis.com/mirror.tensorflow.org/github.com/pybind/pybind11/archive/v2.4.3.tar.gz",
        "https://github.com/pybind/pybind11/archive/v2.4.3.tar.gz",
    ],
    sha256 = "1eed57bc6863190e35637290f97a20c81cfe4d9090ac0a24f3bbf08f265eb71d",
    strip_prefix = "pybind11-2.4.3",
    build_file = "//third_party:pybind11.BUILD",
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

# ABSL cpp library lts_2020_02_25
# Needed for absl/status
http_archive(
    name = "com_google_absl",
    build_file = "//third_party:com_google_absl.BUILD",
    urls = [
        "https://github.com/abseil/abseil-cpp/archive/20200225.tar.gz",
    ],
    # Remove after https://github.com/abseil/abseil-cpp/issues/326 is solved.
    patches = [
        "@//third_party:com_google_absl_f863b622fe13612433fdf43f76547d5edda0c93001.diff"
    ],
    patch_args = [
        "-p1",
    ],
    strip_prefix = "abseil-cpp-20200225",
    sha256 = "728a813291bdec2aa46eab8356ace9f75ac2ed9dfe2df5ab603c4e6c09f1c353"
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
    urls = ["https://chromium.googlesource.com/libyuv/libyuv/+archive/6d603ec3f57dafddc424ef895e5d903915e94ba6.tar.gz"],
    # Adding the constrain of sha256 and strip_prefix will cause failure.
    # It seems that the downloaded libyuv was different every time, so that
    # the specified sha256 and strip_prefix cannot match.
    # sha256 = "ce196c72858456baa8022fa4a0dc18b77d619265dbc0e3d58e25ad15ca402522",
    # strip_prefix = "libyuv-6d603ec3f57dafddc424ef895e5d903915e94ba6",
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
    name = "com_google_protobuf",
    sha256 = "a79d19dcdf9139fa4b81206e318e33d245c4c9da1ffed21c87288ed4380426f9",
    strip_prefix = "protobuf-3.11.4",
    urls = ["https://github.com/protocolbuffers/protobuf/archive/v3.11.4.tar.gz"],
    patches = [
        "@//third_party:com_google_protobuf_fixes.diff"
    ],
    patch_args = [
        "-p1",
    ],
)

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

load("//third_party/flatbuffers:workspace.bzl", flatbuffers = "repo")

flatbuffers()
# Set up TF.
load("@org_tensorflow//tensorflow:workspace.bzl", "tf_workspace")
tf_workspace(tf_repo_name="@org_tensorflow")

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


# Set up Android.
load("//third_party/android:android_configure.bzl", "android_configure")
android_configure(name="local_config_android")
load("@local_config_android//:android.bzl", "android_workspace")
android_workspace()

python_configure(name = "local_config_python")


# Maven dependencies.

RULES_JVM_EXTERNAL_TAG = "3.2"

http_archive(
    name = "rules_jvm_external",
    strip_prefix = "rules_jvm_external-%s" % RULES_JVM_EXTERNAL_TAG,
    sha256 = "82262ff4223c5fda6fb7ff8bd63db8131b51b413d26eb49e3131037e79e324af",
    url = "https://github.com/bazelbuild/rules_jvm_external/archive/%s.zip" % RULES_JVM_EXTERNAL_TAG,
)

load("@rules_jvm_external//:defs.bzl", "maven_install")

maven_install(
    artifacts = [
        "androidx.annotation:annotation:aar:1.1.0",
    ],
    repositories = [
        "https://jcenter.bintray.com",
        "https://maven.google.com",
        "https://dl.google.com/dl/android/maven2",
        "https://repo1.maven.org/maven2",
    ],
    fetch_sources = True,
    version_conflict_policy = "pinned",
)
