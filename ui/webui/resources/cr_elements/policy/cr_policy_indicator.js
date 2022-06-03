// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Polymer element for indicating policies by type. */
Polymer({
  is: 'cr-policy-indicator',

  behaviors: [CrPolicyIndicatorBehavior],

  properties: {
    iconAriaLabel: String,

    /** @private {string} */
    indicatorTooltip_: {
      type: String,
      computed: 'getIndicatorTooltip_(indicatorType, indicatorSourceName)',
    },
  },

  /**
   * @param {!CrPolicyIndicatorType} indicatorType
   * @param {string} indicatorSourceName The name associated with the indicator.
   *     See chrome.settingsPrivate.PrefObject.controlledByName
   * @return {string} The tooltip text for |type|.
   */
  getIndicatorTooltip_(indicatorType, indicatorSourceName) {
    return this.getIndicatorTooltip(indicatorType, indicatorSourceName);
  },
});
