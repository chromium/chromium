// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @constructor
 * @extends {Window}
 */
class BackgroundWindow {
  constructor() {
    /**
     * @type {FileBrowserBackground}
     */
    this.background;

    /**
     * @type {!Object}
     */
    this.launcher = {};
  }
  /**
   * @param {Window} window
   */
  registerDialog(window) {}
}
