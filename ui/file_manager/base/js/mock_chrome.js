// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Installs a mock object to replace window.chrome in a unit test.
 * @param {Object} mockChrome
 */
function installMockChrome(mockChrome) {
  /** @suppress {const|checkTypes} */
  chrome = mockChrome;
}

/**
 * Mocks chrome.commandLinePrivate.
 * @constructor
 */
function MockCommandLinePrivate() {
  this.flags_ = {};
  if (!chrome)
    installMockChrome({});

  if (!chrome.commandLinePrivate) {
    /** @suppress {checkTypes} */
    chrome.commandLinePrivate = {};
  }
  chrome.commandLinePrivate.hasSwitch = (name, callback) => {
    window.setTimeout(() => {
      callback(name in this.flags_);
    }, 0);
  };
}

/**
 * Add a switch.
 * @param {string} name of the switch to add.
 */
MockCommandLinePrivate.prototype.addSwitch = function(name) {
  this.flags_[name] = true;
};
