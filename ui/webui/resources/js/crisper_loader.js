// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * Unlike when using native HTML Imports, the HTML Imports polyfill does not
 * block execution of a script until all files have been imported, resulting in
 * errors if the script references functions (e.g. Polymer()) from files that
 * have not been imported yet. crisper_loader.js is used as a replacement for
 * the main script in optimized Web UI pages that are set to use the polyfill
 * instead of native imports, and is responsible for replacing itself with the
 * main JS file after the polyfill is ready.
 */

(function() {
const thisScript = document.currentScript;
const scriptName = thisScript.dataset.scriptName;
const parentEl = thisScript.parentElement;
const script = document.createElement('script');
script.setAttribute('src', scriptName);
HTMLImports.whenReady(() => {
  parentEl.appendChild(script);
  thisScript.remove();
});
})();
