#!/bin/bash
#
# Copyright The Closure Library Authors
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
# Wraps the unit tests not using the Google framework so they may be run
# on the continuous build.

set -e

source googletest.sh || exit 1

CLOSURE_SRCDIR=$TEST_SRCDIR/google3/third_party/javascript/closure/labs/bin/code

PYTHONPATH=$CLOSURE_SRCDIR

$CLOSURE_SRCDIR/generate_jsdoc_test.py
