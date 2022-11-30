#!/bin/bash
#
# Copyright 2018 The Closure Library Authors. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS-IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# Script to install all necessary dependencies for running Closure tests,
# linting, formatting and compiling.

declare -r CLANG_VERSION="3.7.1"
declare -r CLANG_BUILD="clang+llvm-${CLANG_VERSION}-x86_64-linux-gnu-ubuntu-14.04"
declare -r CLANG_TAR="${CLANG_BUILD}.tar.xz"
declare -r CLANG_URL="http://llvm.org/releases/${CLANG_VERSION}/${CLANG_TAR}"

set -ex

cd ..

# Fetches Closure Compiler components from oss.sonatype.org
fetch () {
  local name="$1"
  local snapshots="https://oss.sonatype.org/content/repositories/snapshots"
  local repo="$snapshots/com/google/javascript"
  local dir="$repo/$name/1.0-SNAPSHOT"
  local jar="$name-1.0-SNAPSHOT.jar"
  local url=$(
    curl -o - "$dir/" |
      sed -n 's+.*<a href="\('"$dir/$name"'-[-.0-9]*\.jar\)".*+\1+p' |
      head -n 1)
  [ -n "$url" ]
  wget -O "$jar" "$url"
}

# Install clang-format.
wget --quiet "$CLANG_URL"
tar xf "$CLANG_TAR"
mv "$CLANG_BUILD" clang
rm -f "$CLANG_TAR"

# Install closure compiler and linter.
fetch closure-compiler
fetch closure-compiler-linter
