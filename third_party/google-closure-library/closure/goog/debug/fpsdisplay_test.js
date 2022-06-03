// Copyright 2011 The Closure Library Authors. All Rights Reserved.
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

goog.module('goog.debug.FpsDisplayTest');
goog.setTestOnly();

const FpsDisplay = goog.require('goog.debug.FpsDisplay');
const TestCase = goog.require('goog.testing.TestCase');
const Timer = goog.require('goog.Timer');
const testSuite = goog.require('goog.testing.testSuite');

let fpsDisplay;

function shouldRunTests() {
  // Disable tests when being run as a part of open-source repo as the test
  // mysteriously times out way before 5s. See http://b/26132213.
  return !(/closure\/goog\/ui/.test(location.pathname));
}

testSuite({
  setUpPage() {
    TestCase.getActiveTestCase().promiseTimeout = 5000;  // 5s
  },

  setUp() {
    fpsDisplay = new FpsDisplay();
  },

  tearDown() {
    goog.dispose(fpsDisplay);
  },

  testRendering() {
    fpsDisplay.render();

    const elem = fpsDisplay.getElement();
    assertHTMLEquals('', elem.innerHTML);

    return Timer.promise(2000).then(() => {
      const fps = parseInt(elem.innerHTML, 10);
      assertTrue(`FPS of ${fps} should be non-negative`, fps >= 0);
      assertTrue(`FPS of ${fps} too big`, fps < 1000);
    });
  },
});
