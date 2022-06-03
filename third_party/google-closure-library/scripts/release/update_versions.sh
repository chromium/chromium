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

# Updates the versions of Closure's packages for release. Run this before npm
# publish.

if [[ -z "$1" ]]; then
  echo "Usage: $0 <version>"
  exit 1
fi

perl -pi -e "s/\"version\": \"v?[\d.]+\"/\"version\": \"$1\"/" \
    package.json
perl -pi -e "s/\"version\": \"v?[\d.]+\"/\"version\": \"$1\"/" \
    closure-deps/package.json
perl -pi -e 's/"google-closure-compiler": ".*"/"google-closure-compiler": "\^'"$1"'"/' \
    closure-deps/package.json
perl -pi -e 's/"google-closure-library": ".*"/"google-closure-library": "\^'"$1"'"/' \
    closure-deps/package.json
