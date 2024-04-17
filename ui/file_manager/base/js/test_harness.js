// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Mark the test as fully loaded. The Browser Test reads this.
window.__TEST_LOADED__ = false;
window.IN_TEST = true;

import 'chrome://webui-test/strings.m.js';

async function run() {
  // Grab the JS module to test from the GET params.
  const params = new URLSearchParams(window.location.search);
  const jsModuleUrl = params.get('test_module');

  // Push all entities to global namespace to be visible to the test harness:
  // ash/webui/common/resources/webui_resource_test.js
  const TestModule = await import(jsModuleUrl);
  for (const name in TestModule) {
    window[name] = TestModule[name];
  }

  // Notify the BrowserTest to continue.
  window.__TEST_LOADED__ = true;
  window.domAutomationController.send('LOADED');
}

run();
