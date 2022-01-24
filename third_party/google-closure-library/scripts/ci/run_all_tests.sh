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

# Run all tests.

set -x

# TODO: For the time being, external testing is disabled.
exit 1

# Serve the files in closure-library at http://localhost:8080.
# We use a Python server at the moment, as the http-server npm module
# rejects POST requests.
./scripts/http/simple_http_server.py 2> /dev/null & sleep 5

# Connect to Sauce.
./scripts/ci/sauce_connect.sh

# Install Selenium.
"$(npm bin)/webdriver-manager" update

# Invoke protractor to run tests.
"$(npm bin)/protractor" protractor.conf.js
