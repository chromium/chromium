// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @fileoverview Helper script used to load a JS module test into a WebUI page.
//
// Example usage:
//   chrome://welcome/test_loader.html?module=my_test.js
//
// will load the JS module at chrome/test/data/webui/my_test.js
//
// test_loader.html and test_loader.js should be registered with the
// WebUIDataSource corresponding to the WebUI being tested, such that the
// testing code is loaded from the same origin. Also note that the
// chrome://test/ data source only exists in a testing context, so using this
// script in production will result in a failed network request.

(function() {
const params = new URLSearchParams(window.location.search);
const module = params.get('module');
const script = document.createElement('script');
script.type = 'module';
script.src = `chrome://test/${module}`;
document.body.appendChild(script);
})();
