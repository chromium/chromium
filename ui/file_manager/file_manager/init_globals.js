// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Runtime setup before main.js is executed.

/**
 * Sets window.IN_TEST if this code is run in the test environment. We
 * detect this by checking for presence of domAutomationController.
 * @const @type {boolean}
 */
window.IN_TEST = window.IN_TEST || (() => {
                   return window.domAutomationController ? true : undefined;
                 })();
