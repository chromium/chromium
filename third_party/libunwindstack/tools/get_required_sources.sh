#!/bin/bash

# Outputs the files required to build libunwindstack on stdout.

set -e -u

if [[ -z ${1+x} ]]; then
  echo Usage: $0 out_dir
  exit 1
fi
out_dir=$1

# Change to the Chrome repository root.
cd $(dirname $0)/../../..

# Temporarily set up the build to write angle includes to the deps file since
# libunwindstack uses these to include its own headers.
perl -i -pe 's/-MMD/-MD/' $out_dir/toolchain.ninja

# `clean` can return a non-zero exit code if targets write files to the output
# directory that ninja doesn't know about.
autoninja -C $out_dir -t clean >&2 || true
autoninja -C $out_dir -d keepdepfile stack_unwinder >&2

# Restore the default deps file behavior.
gn gen $out_dir >&2

{
  find $out_dir/obj/third_party/libunwindstack/libunwindstack \
      $out_dir/obj/base/native_unwinder_android -name \*.d |
    xargs cat |
    # Depenencies are listed in the deps files indented by spaces, and may be
    # followed by a space and backslash. Extract the dependencies under the
    # libunwindstack path.
    perl -lne 'print $1 if m: .*third_party/libunwindstack/(\S+):'
  # Dep files aren't generated for assembly so these files must be explicitly
  # included.
  (cd third_party/libunwindstack && find src -path src/libunwindstack/\*.S)
} | sort -u
