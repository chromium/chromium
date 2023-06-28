workspace(name = "com_google_sentencepiece")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# proto_library, cc_proto_library, and java_proto_library rules implicitly
# depend on @com_google_protobuf for protoc and proto runtimes.
# This statement defines the @com_google_protobuf repo.
PROTOBUF_URLS = [
  "http://mirror.tensorflow.org/github.com/protocolbuffers/protobuf/archive/v3.6.1.2.tar.gz",
  "https://github.com/protocolbuffers/protobuf/archive/v3.6.1.2.tar.gz",
]
PROTOBUF_SHA256 = "2244b0308846bb22b4ff0bcc675e99290ff9f1115553ae9671eba1030af31bc0"
PROTOBUF_STRIP_PREFIX = "protobuf-3.6.1.2"

http_archive(
  name = "com_google_protobuf",
  sha256 = PROTOBUF_SHA256,
  strip_prefix = PROTOBUF_STRIP_PREFIX,
  urls = PROTOBUF_URLS,
)

http_archive(
  name = "com_google_protobuf_cc",
  sha256 = PROTOBUF_SHA256,
  strip_prefix = PROTOBUF_STRIP_PREFIX,
  urls = PROTOBUF_URLS,
)

# Bazel toolchains
http_archive(
  name = "bazel_toolchains",
  urls = [
    "https://mirror.bazel.build/github.com/bazelbuild/bazel-toolchains/archive/bc09b995c137df042bb80a395b73d7ce6f26afbe.tar.gz",
    "https://github.com/bazelbuild/bazel-toolchains/archive/bc09b995c137df042bb80a395b73d7ce6f26afbe.tar.gz",
  ],
  strip_prefix = "bazel-toolchains-bc09b995c137df042bb80a395b73d7ce6f26afbe",
  sha256 = "4329663fe6c523425ad4d3c989a8ac026b04e1acedeceb56aa4b190fa7f3973c",
)

# ABSL
http_archive(
  name = "com_google_absl",
  urls = [
    "http://mirror.tensorflow.org/github.com/abseil/abseil-cpp/archive/666fc1266bccfd8e6eaaa084e7b42580bb8eb199.tar.gz",
    "https://github.com/abseil/abseil-cpp/archive/666fc1266bccfd8e6eaaa084e7b42580bb8eb199.tar.gz",
  ],
  sha256 = "d1535b8bd6ac41a0f899b906c1b5ef375136475e34dd53fb6775eb287487eef7",
  strip_prefix = "abseil-cpp-666fc1266bccfd8e6eaaa084e7b42580bb8eb199",
)

# Provides flags support until ABSL Flags is released.
http_archive(
        name = "com_github_gflags_gflags",
        sha256 = "ae27cdbcd6a2f935baa78e4f21f675649271634c092b1be01469440495609d0e",
        strip_prefix = "gflags-2.2.1",
        urls = [
            "http://mirror.tensorflow.org/github.com/gflags/gflags/archive/v2.2.1.tar.gz",
            "https://github.com/gflags/gflags/archive/v2.2.1.tar.gz",
        ],
)

# GoogleTest/GoogleMock framework. Used by most unit-tests.
http_archive(
     name = "com_google_googletest",
     urls = ["https://github.com/google/googletest/archive/b6cd405286ed8635ece71c72f118e659f4ade3fb.zip"],  # 2019-01-07
     strip_prefix = "googletest-b6cd405286ed8635ece71c72f118e659f4ade3fb",
     sha256 = "ff7a82736e158c077e76188232eac77913a15dac0b22508c390ab3f88e6d6d86",
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
