// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior to be used by Polymer elements that want to
 * automatically remove WebUI listeners when detached.
 */

// #import {WebUIListener, addWebUIListener, removeWebUIListener} from './cr.m.js';

/** @polymerBehavior */
// eslint-disable-next-line no-var
/* #export */ var WebUIListenerBehavior = {
  properties: {
    /**
     * Holds WebUI listeners that need to be removed when this element is
     * destroyed.
     * @private {!Array<!WebUIListener>}
     */
    webUIListeners_: {
      type: Array,
      value: function() {
        return [];
      },
    },
  },

  /**
   * Adds a WebUI listener and registers it for automatic removal when this
   * element is detached.
   * Note: Do not use this method if you intend to remove this listener
   * manually (use cr.addWebUIListener directly instead).
   *
   * @param {string} eventName The event to listen to.
   * @param {!Function} callback The callback run when the event is fired.
   */
  addWebUIListener: function(eventName, callback) {
    this.webUIListeners_.push(cr.addWebUIListener(eventName, callback));
  },

  /** @override */
  detached: function() {
    while (this.webUIListeners_.length > 0) {
      cr.removeWebUIListener(this.webUIListeners_.pop());
    }
  },
};
