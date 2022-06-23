// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

async function run() {
  // Grab the JS module to test from the GET params.
  const params = new URLSearchParams(window.location.search);
  const jsModuleUrl = params.get('test_module');

  // Push all entities to global namespace to be visible to the test harness:
  // ui/webui/resources/js/webui_resource_test.js
  const TestModule = await import(jsModuleUrl);
  for (const name in TestModule) {
    window[name] = TestModule[name];
  }

  // Notify the BrowserTest to continue.
  window.domAutomationController.send('LOADED');
}

run();
