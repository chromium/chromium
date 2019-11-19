// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class FilesToggleRipple extends PolymerElement {
  constructor() {
    /**
     * @type {boolean}
     */
    this.activated;
  }
}

class FilesToast extends PolymerElement {
  constructor() {
    /**
     * @type {boolean}
     */
    this.visible;

    /**
     * @type {number}
     */
    this.timeout;
  }
  /**
   * @param {string} text
   * @param {{text: string, callback: function()}=} opt_action
   */
  show(text, opt_action) {}

  /**
   * @return {!Promise}
   */
  hide() {}
}


class FilesQuickView extends PolymerElement {}

class FilesMetadataBox extends PolymerElement {}
