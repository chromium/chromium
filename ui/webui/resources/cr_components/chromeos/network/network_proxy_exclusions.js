// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of proxy exclusions.
 * Includes UI for adding, changing, and removing entries.
 */

(function() {

Polymer({
  is: 'network-proxy-exclusions',

  behaviors: [I18nBehavior],

  properties: {
    /** Whether or not the proxy values can be edited. */
    editable: {
      type: Boolean,
      value: false,
    },

    /**
     * The list of exclusions.
     * @type {!Array<string>}
     */
    exclusions: {
      type: Array,
      value: function() {
        return [];
      },
      notify: true
    }
  },

  /**
   * Event triggered when an item is removed.
   * @param {!{model: !{index: number}}} event
   * @private
   */
  onRemoveTap_: function(event) {
    const index = event.model.index;
    this.splice('exclusions', index, 1);
    this.fire('proxy-exclusions-change');
  }
});
})();
