// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 */
class FileBrowserBackground {
  constructor() {
    /** @type {!Object<!Window>} */
    this.dialogs;
  }
  /**
   * @param {function()} callback
   */
  ready(callback) {}
}
