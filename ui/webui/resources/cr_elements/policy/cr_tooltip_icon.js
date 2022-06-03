// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Polymer({
  is: 'cr-tooltip-icon',

  properties: {
    iconAriaLabel: String,

    iconClass: String,

    tooltipText: String,

    /** Position of tooltip popup related to the icon. */
    tooltipPosition: {
      type: String,
      value: 'top',
    }
  },

  /** @return {!Element} */
  getFocusableElement() {
    return this.$.indicator;
  },
});