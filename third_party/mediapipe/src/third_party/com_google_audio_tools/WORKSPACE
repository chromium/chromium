load("@bazel_tools//tools/build_defs/repo:git.bzl",
     "git_repository", "new_git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# ===== absl =====
git_repository(
    name = "com_google_absl",
    remote = "https://github.com/abseil/abseil-cpp.git",
    commit = "c51510d1d87ebce8615ae1752fd5aca912f6cf4c",
)

# ===== fft2 =====
http_archive(
    name = "fft2d",
    build_file = "fft2d.BUILD",
    sha256 = "ada7e99087c4ed477bfdf11413f2ba8db8a840ba9bbf8ac94f4f3972e2a7cec9",
    urls = [
        "http://www.kurims.kyoto-u.ac.jp/~ooura/fft2d.tgz",
    ],
)

# ===== gtest =====
git_repository(
    name = "gtest",
    remote = "https://github.com/google/googletest.git",
    tag = "release-1.12.1",
)

# ==== kissfft ====
new_git_repository(
    name = "kissfft",
    build_file = "kissfft.BUILD",
    remote = "https://github.com/mborgerding/kissfft.git",
    tag = "v131",
)

# ===== eigen =====
http_archive(
    name = "eigen_archive",
    build_file = "eigen.BUILD",
    sha256 = "f3d69ac773ecaf3602cb940040390d4e71a501bb145ca9e01ce5464cf6d4eb68",
    strip_prefix = "eigen-eigen-049af2f56331",
    urls = [
        "http://mirror.tensorflow.org/bitbucket.org/eigen/eigen/get/049af2f56331.tar.gz",
        "https://bitbucket.org/eigen/eigen/get/049af2f56331.tar.gz",
    ],
)

# ===== benchmark =====
git_repository(
    name = "com_google_benchmark",
    remote = "https://github.com/google/benchmark.git",
    tag = "v1.8.0",
)

# ===== gflags, required by glog =====
git_repository(
    name = "com_github_gflags_gflags",
    remote = "https://github.com/gflags/gflags.git",
    tag = "v2.2.2",
)

# ===== glog =====
git_repository(
    name = "com_github_glog_glog",
    remote = "https://github.com/google/glog.git",
    tag = "v0.4.0",
)

# ===== protobuf ===== (Everything below here is required by protobuf)
# proto_library rules implicitly depend on @com_google_protobuf//:protoc
git_repository(
    name = "com_google_protobuf",
    remote = "https://github.com/protocolbuffers/protobuf.git",
    tag = "v3.12.3",
)

git_repository(
    name = "bazel_skylib",
    remote = "https://github.com/bazelbuild/bazel-skylib.git",
    tag = "1.0.2",
)

git_repository(
    name = "rules_python",
    remote = "https://github.com/bazelbuild/rules_python.git",
    tag = "0.0.2",
)

http_archive(
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    sha256 = "c3e5e9fdd5004dcb542feda5ee4f0ff0744628baf8ed2dd5d66f8ca1197cb1a1",
    strip_prefix = "zlib-1.2.11",
    urls = [
        "https://mirror.bazel.build/zlib.net/zlib-1.2.11.tar.gz",
        "https://zlib.net/zlib-1.2.11.tar.gz",
    ],
)
