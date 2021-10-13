workspace(name = "com_google_javascript_closure_library")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Integration of Closure Library with Bazel is handled by rules_closure
# maintainers, and any Closure Library issues encountered when using
# rules_closure should be filed in that repository first.
http_archive(
    name = "io_bazel_rules_closure",
    sha256 = "9498e57368efb82b985db1ed426a767cbf1ba0398fd7aed632fc3908654e1b1e",
    urls = [
        "https://github.com/bazelbuild/rules_closure/archive/refs/tags/0.12.0.tar.gz",  # 2021-06-23
    ],
)

load("@io_bazel_rules_closure//closure:defs.bzl", "closure_repositories")

closure_repositories()