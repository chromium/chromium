// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This script is injected into a page to run the isProbablyReaderable() check.
 * It assumes Readability-readerable.js has already been injected.
 */
(() => {
  try {
    // isProbablyReaderable() is a standalone function in Readability-readerable.js.
    return isProbablyReaderable(document);
  } catch (e) {
    console.error('Error during isProbablyReaderable() check: ' + e);
    if (e.stack) {
      console.error(e.stack);
    }
    return false;
  }
})();
