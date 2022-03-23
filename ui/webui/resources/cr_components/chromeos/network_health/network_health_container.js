// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/cr_elements/icons.m.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @fileoverview Polymer element for a container used in displaying network
 * health info.
 */

Polymer({
  _template: html`{__html_template__}`,
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
   * Helper function to fire the toggle event when clicked.
   * @private
   */
  onClick_() {
    this.fire('toggle-expanded');
  },
});
