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
# Script to determine if .js files in Pull Request are properly formatted.
# Exits with non 0 exit code if formatting is needed.

FILES_TO_CHECK=$(git diff --name-only master | grep -E "\.js$")

if [ -z "${FILES_TO_CHECK}" ]; then
  echo "No .js files to check for formatting."
  exit 0
fi

FORMAT_DIFF=$(git diff -U0 master -- ${FILES_TO_CHECK} |
              ../clang/share/clang/clang-format-diff.py -p1 -style=Google)

if [ -z "${FORMAT_DIFF}" ]; then
  # No formatting necessary.
  echo "All files in PR properly formatted."
  exit 0
else
  # Found diffs.
  echo "ERROR: Found formatting errors!"
  echo "${FORMAT_DIFF}"
  echo "See https://goo.gl/wUEkW9 for instructions to format your PR."
  exit 1
fi
