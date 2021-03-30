// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for a container used in displaying network
 * health info.
 */

Polymer({
  is: 'network-health-container',

  properties: {
    /**
     * Boolean flag if the container is expanded.
     */
    expanded: {
      type: Boolean,
      value: false,
    },

    /**
     * Container label.
     */
    label: {
      type: String,
      value: '',
    },
  },

  /**
   * Returns the correct arrow icon depending on if the container is expanded.
   * @param {boolean} expanded
   */
  getArrowIcon_(expanded) {
    return expanded ? 'cr:expand-less' : 'cr:expand-more';
  },

  /**
   * Helper function to toggle the expanded properties when the routine group
   * is clicked.
   * @private
   */
  onClick_() {
    this.set('expanded', !this.expanded);
  },
});
