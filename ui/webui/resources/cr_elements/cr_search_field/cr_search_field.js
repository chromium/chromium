// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-search-field' is a simple implementation of a polymer component that
 * uses CrSearchFieldBehavior.
 */

import '../cr_icon_button/cr_icon_button.m.js';
import '../cr_input/cr_input.m.js';
import '../icons.m.js';
import '../shared_style_css.m.js';
import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CrSearchFieldBehavior} from './cr_search_field_behavior.js';

Polymer({
  is: 'cr-search-field',

  _template: html`{__html_template__}`,

  behaviors: [CrSearchFieldBehavior],

  properties: {
    autofocus: {
      type: Boolean,
      value: false,
    },
  },

  /** @return {!CrInputElement} */
  getSearchInput() {
    return /** @type {!CrInputElement} */ (this.$.searchInput);
  },

  /** @private */
  onTapClear_() {
    this.setValue('');
    setTimeout(() => {
      this.$.searchInput.focus();
    });
  },
});
