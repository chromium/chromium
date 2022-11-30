#!/bin/bash
set -e  # fail and exit on any command erroring
set -x  # print evaluated commands

osname="$(uname -s)"
if [[ $osname == "Darwin" ]]; then
  # Update to macos extensions
  sed -i '' 's/".so"/".dylib"/' tensorflow_text/tftext.bzl
  perl -pi -e "s/(load_library.load_op_library.*)\\.so'/\$1.dylib'/" $(find tensorflow_text/python -type f)
  export CC_OPT_FLAGS='-mavx'
fi

# Run configure.
./oss_scripts/configure.sh

# Build the pip package.
bazel build --enable_runfiles oss_scripts/pip_package:build_pip_package
./bazel-bin/oss_scripts/pip_package/build_pip_package .
