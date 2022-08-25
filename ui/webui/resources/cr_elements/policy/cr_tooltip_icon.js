// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../icons.m.js';
import '../shared_style_css.m.js';
import '../shared_vars_css.m.js';
import '//resources/polymer/v3_0/iron-icon/iron-icon.js';
import '//resources/polymer/v3_0/paper-tooltip/paper-tooltip.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

Polymer({
  is: 'cr-tooltip-icon',

  _template: html`{__html_template__}`,

  properties: {
    iconAriaLabel: String,

    iconClass: String,

    tooltipText: String,

    /** Position of tooltip popup related to the icon. */
    tooltipPosition: {
      type: String,
      value: 'top',
    },
  },

  /** @return {!Element} */
  getFocusableElement() {
    return this.$.indicator;
  },
});
