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

// See https://github.com/angular/protractor/blob/master/docs/referenceConf.js
// for full protractor config reference.
var browserCapabilities = require('./browser_capabilities');

var SAUCE_ACCESS_KEY =
    process.env.SAUCE_ACCESS_KEY.split('').reverse().join('');

exports.config = {
  sauceUser: process.env.SAUCE_USERNAME,

  sauceKey: SAUCE_ACCESS_KEY,

  multiCapabilities: browserCapabilities,

  // Testing framework used for spec file.
  framework: 'jasmine2',

  // Relative path to spec (i.e., tests).
  specs: ['protractor_spec.js'],

  jasmineNodeOpts: {
    // Timeout in ms before a test fails.
    defaultTimeoutInterval: 30 * 60 * 1000
  }
};
