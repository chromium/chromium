// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled network properties.
 */

/** @polymerBehavior */
const CrPolicyNetworkBehaviorMojo = {
  /**
   * @param {!OncMojo.ManagedProperty} property
   * @return {boolean} True if the property is controlled by network policy.
   */
  isNetworkPolicyControlled: function(property) {
    assert(property);
    const mojom = chromeos.networkConfig.mojom;
    return property.policySource != mojom.PolicySource.kNone &&
        property.policySource != mojom.PolicySource.kActiveExtension;
  },

  /**
   * @param {!OncMojo.ManagedProperty} property
   * @return {boolean} True if the property is controlled by an extension.
   */
  isExtensionControlled: function(property) {
    assert(property);
    return property.policySource ==
        chromeos.networkConfig.mojom.PolicySource.kActiveExtension;
  },

  /**
   * @param {!OncMojo.ManagedProperty} property
   * @return {boolean} True if the network property is controlled by a network
   *     policy or an extension.
   */
  isControlled: function(property) {
    assert(property);
    return property.policySource !=
        chromeos.networkConfig.mojom.PolicySource.kNone;
  },

  /**
   * @param {!OncMojo.ManagedProperty} property
   * @return {boolean} True if the network property is editable.
   */
  isEditable: function(property) {
    assert(property);
    const mojom = chromeos.networkConfig.mojom;
    return property.policySource != mojom.PolicySource.kUserPolicyEnforced &&
        property.policySource != mojom.PolicySource.kDevicePolicyEnforced &&
        property.policySource != mojom.PolicySource.kActiveExtension;
  },

  /**
   * @param {!OncMojo.ManagedProperty} property
   * @return {boolean} True if the network property is enforced by a policy.
   */
  isNetworkPolicyEnforced: function(property) {
    if (!property) {
      return false;
    }
    const mojom = chromeos.networkConfig.mojom;
    return property.policySource == mojom.PolicySource.kUserPolicyEnforced ||
        property.policySource == mojom.PolicySource.kDevicePolicyEnforced;
  },

  /**
   * @param {!OncMojo.ManagedProperty} property
   * @return {boolean} True if the network property is recommended by a policy.
   */
  isNetworkPolicyRecommended: function(property) {
    if (!property) {
      return false;
    }
    const mojom = chromeos.networkConfig.mojom;
    return property.policySource == mojom.PolicySource.kUserPolicyRecommended ||
        property.policySource == mojom.PolicySource.kDevicePolicyRecommended;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.OncSource} source
   * @return {boolean}
   * @protected
   */
  isPolicySource: function(source) {
    return source == chromeos.networkConfig.mojom.OncSource.kDevicePolicy ||
        source == chromeos.networkConfig.mojom.OncSource.kUserPolicy;
  },

  /**
   * @param {!chromeos.networkConfig.mojom.OncSource} source
   * @return {!CrPolicyIndicatorType}
   * @private
   */
  getIndicatorTypeForSource: function(source) {
    if (source == chromeos.networkConfig.mojom.OncSource.kDevicePolicy) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }
    if (source == chromeos.networkConfig.mojom.OncSource.kUserPolicy) {
      return CrPolicyIndicatorType.USER_POLICY;
    }
    return CrPolicyIndicatorType.NONE;
  },

  /**
   * Get policy indicator type for the setting at |path|.
   * @param {!OncMojo.ManagedProperty} property
   * @return {CrPolicyIndicatorType}
   */
  getPolicyIndicatorType: function(property) {
    if (!property) {
      return CrPolicyIndicatorType.NONE;
    }
    const mojom = chromeos.networkConfig.mojom;
    if (property.policySource == mojom.PolicySource.kUserPolicyEnforced ||
        property.policySource == mojom.PolicySource.kUserPolicyRecommended) {
      return CrPolicyIndicatorType.USER_POLICY;
    }
    if (property.policySource == mojom.PolicySource.kDevicePolicyEnforced ||
        property.policySource == mojom.PolicySource.kDevicePolicyRecommended) {
      return CrPolicyIndicatorType.DEVICE_POLICY;
    }
    if (property.policySource == mojom.PolicySource.kActiveExtension) {
      return CrPolicyIndicatorType.EXTENSION;
    }
    return CrPolicyIndicatorType.NONE;
  },
};
