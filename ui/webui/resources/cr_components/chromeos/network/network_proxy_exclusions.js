// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of proxy exclusions.
 * Includes UI for adding, changing, and removing entries.
 */

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/polymer/v3_0/iron-flex-layout/iron-flex-layout-classes.js';
import './network_shared_css.js';

import {I18nBehavior} from '//resources/cr_elements/i18n_behavior.js';
import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  _template: html`{__html_template__}`,
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
      value() {
        return [];
      },
      notify: true,
    },
  },

  /**
   * Event triggered when an item is removed.
   * @param {!{model: !{index: number}}} event
   * @private
   */
  onRemoveTap_(event) {
    const index = event.model.index;
    this.splice('exclusions', index, 1);
    this.fire('proxy-exclusions-change');
  },
});
