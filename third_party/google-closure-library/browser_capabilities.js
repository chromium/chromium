// Copyright 2018 The Closure Library Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

var sauceBrowsers = require('./sauce_browsers.json');

/**
 * Returns a versioned name for the given capability object.
 * @param {!Object} browserCap
 * @return {string}
 */
function getBrowserName(browserCap) {
  var name = browserCap.browserName == 'internet explorer' ?
      'ie' :
      browserCap.browserName;
  var version = browserCap.version || '-latest';
  return name + version;
}

/**
 * Returns the travis job name for the given capability object.
 * @param {!Object} browserCap
 * @return {string}
 */
function getJobName(browserCap) {
  var browserName = getBrowserName(browserCap);

  return process.env.TRAVIS_PULL_REQUEST == 'false' ?
      'CO-' + process.env.TRAVIS_BRANCH + '-' + browserName :
      'PR-' + process.env.TRAVIS_PULL_REQUEST + '-' + browserName + '-' +
          process.env.TRAVIS_BRANCH;
}

/**
 * Adds 'name', 'build', and 'tunnel-identifier' properties to all elements,
 * based on runtime information from the environment.
 * @param {!Array<!Object>} browsers
 * @return {!Array<!Object>} The original array, whose objects are augmented.
 */
function getBrowserCapabilities(browsers) {
  for (var i = 0; i < browsers.length; i++) {
    var b = browsers[i];
    b['tunnel-identifier'] = process.env.TRAVIS_JOB_NUMBER;
    b['build'] = process.env.TRAVIS_BUILD_NUMBER;
    b['name'] = getJobName(b);
  }
  return browsers;
}

module.exports = getBrowserCapabilities(sauceBrowsers);
