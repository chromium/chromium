// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.require('KeyboardHandler');

/**
 * Initializes minimal content script.
 */
function initMin() {
  if (cvox.ChromeVox.isChromeOS)
    return;

  if (cvox.ChromeVox.isClassicEnabled_ === undefined) {
    window.setTimeout(function() {
      initMin();
    }, 500);
    return;
  }

  if (cvox.ChromeVox.isClassicEnabled_)
    return;

  new KeyboardHandler();
}

initMin();
