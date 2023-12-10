// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @fileoverview Helper script used to load a JS module test into a WebUI page.
//
// Example usage:
//   chrome://welcome/test_loader.html?module=foo/my_test.js
//
// will load the JS module at chrome/test/data/webui/foo/my_test.js, through the
// chrome://webui-test/foo/my_test.js URL.
//
// test_loader.html and test_loader.js should be registered with the
// WebUIDataSource corresponding to the WebUI being tested, such that the
// testing code is loaded from the same origin. Also note that the
// chrome://webui-test/ data source only exists in a testing context, so using
// this script in production will result in a failed network request.

import {loadMochaAdapter, loadTestModule} from './test_loader_util.js';

async function main() {
  const mochaAdapterLoaded = await loadMochaAdapter();
  if (!mochaAdapterLoaded) {
    throw new Error('Failed to load Mocha adapter file.');
  }

  const testModuleLoaded = await loadTestModule();

  if (!testModuleLoaded) {
    throw new Error('Failed to load test module');
  }
}
main();
