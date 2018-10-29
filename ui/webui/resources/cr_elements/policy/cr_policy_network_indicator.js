// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies based on network
 * properties.
 */

Polymer({
  is: 'cr-policy-network-indicator',

  behaviors: [CrPolicyIndicatorBehavior, CrPolicyNetworkBehavior],

  properties: {
    /**
     * Network property associated with the indicator.
     * @type {?CrOnc.ManagedProperty|undefined}
     */
    property: Object,

    /** Position of tooltip popup related to the policy indicator. */
    tooltipPosition: String,

    /**
     * Recommended value for non enforced properties.
     * @private {!CrOnc.NetworkPropertyType|undefined}
     */
    recommended_: Object,

    /** @private */
    indicatorTooltip_: {
      type: String,
      computed: 'getNetworkIndicatorTooltip_(indicatorType, property.*)',
    },
  },

  observers: ['propertyChanged_(property.*)'],

  /** @private */
  propertyChanged_: function() {
    var property = this.property;
    if (property == null || !this.isControlled(property)) {
      this.indicatorType = CrPolicyIndicatorType.NONE;
      return;
    }
    var effective = property.Effective;
    var active = property.Active;
    if (active == undefined)
      active = property[effective];

    if (property.UserEditable === true &&
        property.hasOwnProperty('UserPolicy')) {
      // We ignore UserEditable unless there is a UserPolicy.
      this.recommended_ =
          /** @type {!CrOnc.NetworkPropertyType} */ (property.UserPolicy);
      this.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
    } else if (
        property.DeviceEditable === true &&
        property.hasOwnProperty('DevicePolicy')) {
      // We ignore DeviceEditable unless there is a DevicePolicy.
      this.recommended_ =
          /** @type {!CrOnc.NetworkPropertyType} */ (property.DevicePolicy);
      this.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
    } else if (effective == 'UserPolicy') {
      this.indicatorType = CrPolicyIndicatorType.USER_POLICY;
    } else if (effective == 'DevicePolicy') {
      this.indicatorType = CrPolicyIndicatorType.DEVICE_POLICY;
    } else if (effective == 'ActiveExtension') {
      this.indicatorType = CrPolicyIndicatorType.EXTENSION;
    } else {
      this.indicatorType = CrPolicyIndicatorType.NONE;
    }
  },

  /**
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getNetworkIndicatorTooltip_: function() {
    if (this.property === undefined)
      return '';

    var matches;
    if (this.indicatorType == CrPolicyIndicatorType.RECOMMENDED &&
        this.property) {
      var value = this.property.Active;
      if (value == undefined && this.property.Effective)
        value = this.property[this.property.Effective];
      matches = value == this.recommended_;
    }
    return this.getIndicatorTooltip(this.indicatorType, '', matches);
  }
});
