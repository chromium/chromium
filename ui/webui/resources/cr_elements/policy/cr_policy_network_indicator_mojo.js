// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for indicating policies based on network
 * properties.
 */
(function() {
'use strict';

Polymer({
  is: 'cr-policy-network-indicator-mojo',

  behaviors: [CrPolicyIndicatorBehavior, CrPolicyNetworkBehaviorMojo],

  properties: {
    /**
     * Network property associated with the indicator. Note: |property| may
     * be null or undefined, depending on how the properties dictionary is
     * generated.
     * @type {?chromeos.networkConfig.mojom.ManagedBoolean|
     *        ?chromeos.networkConfig.mojom.ManagedInt32|
     *        ?chromeos.networkConfig.mojom.ManagedString|undefined}
     */
    property: Object,

    /** Property forwarded to the cr-tooltip-icon element. */
    tooltipPosition: String,

    /** @private */
    indicatorTooltip_: {
      type: String,
      computed: 'getNetworkIndicatorTooltip_(indicatorType, property.*)',
    },
  },

  observers: ['propertyChanged_(property.*)'],

  /** @private */
  propertyChanged_: function() {
    const property = this.property;
    if (property === null || property === undefined ||
        !this.isControlled(property)) {
      this.indicatorType = CrPolicyIndicatorType.NONE;
      return;
    }
    const PolicySource = chromeos.networkConfig.mojom.PolicySource;
    switch (property.policySource) {
      case PolicySource.kNone:
        this.indicatorType = CrPolicyIndicatorType.NONE;
        break;
      case PolicySource.kUserPolicyEnforced:
        this.indicatorType = CrPolicyIndicatorType.USER_POLICY;
        break;
      case PolicySource.kDevicePolicyEnforced:
        this.indicatorType = CrPolicyIndicatorType.DEVICE_POLICY;
        break;
      case PolicySource.kUserPolicyRecommended:
      case PolicySource.kDevicePolicyRecommended:
        this.indicatorType = CrPolicyIndicatorType.RECOMMENDED;
        break;
      case PolicySource.kActiveExtension:
        this.indicatorType = CrPolicyIndicatorType.EXTENSION;
        break;
    }
  },

  /**
   * @return {string} The tooltip text for |type|.
   * @private
   */
  getNetworkIndicatorTooltip_: function() {
    if (this.property === undefined) {
      return '';
    }

    const matches = !!this.property &&
        this.property.activeValue == this.property.policyValue;
    return this.getIndicatorTooltip(this.indicatorType, '', matches);
  }
});
})();
