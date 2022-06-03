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
# Script to run the Compiler's Linter on only the modified or added files in the
# current branch. Should be run from the base git directory with the PR branch
# checked out.

CURRENT_BRANCH=$(git rev-parse --abbrev-ref HEAD)
CHANGED_FILES=$(git diff --name-only --diff-filter=AM master.."$CURRENT_BRANCH" |
    grep -E "\.js$" |
    grep -v -E "test\.js$" |
    grep -v -f scripts/ci/lint_ignore.txt)

if [[ -n "$CHANGED_FILES" ]]; then
  set -x
  java -jar ../closure-compiler-linter-1.0-SNAPSHOT.jar $CHANGED_FILES
else
  echo "No .js files found to lint in this Pull Request."
fi
