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
# Compiles pertinent Closure library files.

# TODO(sdh): Make strictCheckTypes an error, or at least only whitelist
#     strictMissingProperties and fix the handful of strictPrimitiveOperator
#     violations.

JAR_FILE=$1

java -Xmx1G -jar "${JAR_FILE}" \
  -O ADVANCED \
  --warning_level VERBOSE \
  --jscomp_error='*' \
  --jscomp_off=strictCheckTypes \
  --jscomp_off=missingRequire \
  --jscomp_off=extraRequire \
  --jscomp_off=deprecated \
  --jscomp_off=lintChecks \
  --jscomp_off=analyzerChecks \
  --jscomp_warning=unusedLocalVariables \
  --js='./closure/goog/**.js' \
  --js='./third_party/closure/goog/**.js' \
  --js='!**_test.js' \
  --js='!**_perf.js' \
  --js='!**tester.js' \
  --js='!**promise/testsuiteadapter.js' \
  --js='!**relativecommontests.js' \
  --js='!**osapi/osapi.js' \
  --js='!**svgpan/svgpan.js' \
  --js='!**alltests.js' \
  --js='!**node_modules**.js' \
  --js='!**protractor_spec.js' \
  --js='!**protractor.conf.js' \
  --js='!**browser_capabilities.js' \
  --js='!./doc/**.js' \
  --js='!**debug_loader_integration_tests/testdata/**' \
  --js_output_file="$(mktemp)"
