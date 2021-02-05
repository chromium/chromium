# Tools for building the TF.Text pip package.

package(default_visibility = ["//visibility:private"])

licenses(["notice"])  # Apache 2.0

sh_binary(
    name = "build_pip_package",
    srcs = ["build_pip_package.sh"],
    data = [
        "LICENSE",
        "MANIFEST.in",
        "setup.nightly.py",
        "setup.py",
        "//tensorflow_text",
    ],
)
