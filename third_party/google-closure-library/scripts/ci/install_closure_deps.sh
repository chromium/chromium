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
# Script to install all necessary dependencies for compiling Closure and running
# Closure tests.

set -ex

cd ..

# Fetches Closure Compiler components from oss.sonatype.org
# Sonatype doesn't appear to have a REST API, so instead, we manually scrape the
# relevant pages for a particular repository.
# For example, we are scraping
# https://oss.sonatype.org/content/repositories/snapshots/com/google/javascript/closure-compiler/1.0-SNAPSHOT/
# for the compiler.
fetch () {
  local name="$1"
  local snapshots="https://oss.sonatype.org/content/repositories/snapshots"
  local repo="$snapshots/com/google/javascript"
  local dir="$repo/$name/1.0-SNAPSHOT"
  local jar="$name-1.0-SNAPSHOT.jar"
  # use Curl to download the HTML of the page above and scrape it for all jar
  # links. Then, sort this list (latest ends up last), and use it.
  # Previously, this script might have relied on the page contents being sorted
  # latest first.
  # TODO(user): Change this to read the maven-metadata.xml file instead
  local url=$(
    curl -o - "$dir/" |
      sed -n 's+.*<a href="\('"$dir/$name"'-[-.0-9]*\.jar\)".*+\1+p' |
      sort |
      tail -n 1)
  [ -n "$url" ]
  wget -O "$jar" "$url"
}

# Install closure compiler and linter.
fetch closure-compiler
fetch closure-compiler-linter
