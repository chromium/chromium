// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled network properties.
 */

/** @polymerBehavior */
var CrPolicyNetworkBehavior = {
  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by a policy
   *     (either enforced or recommended).
   */
  isNetworkPolicyControlled: function(property) {
    // If the property is not a dictionary, or does not have an Effective
    // sub-property set, then the property is not policy controlled.
    if (typeof property != 'object' || !property.Effective)
      return false;
    // Enforced
    var effective = property.Effective;
    if (effective == 'UserPolicy' || effective == 'DevicePolicy')
      return true;
    // Recommended
    if (typeof property.UserPolicy != 'undefined' ||
        typeof property.DevicePolicy != 'undefined') {
      return true;
    }
    // Neither enforced nor recommended = not policy controlled.
    return false;
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by an
   *     extension.
   */
  isExtensionControlled: function(property) {
    return typeof property == 'object' &&
        property.Effective == 'ActiveExtension';
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is controlled by a policy
   *     or an extension.
   */
  isControlled: function(property) {
    return this.isNetworkPolicyControlled(property) ||
        this.isExtensionControlled(property);
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is editable.
   */
  isEditable: function(property) {
    // If the property is not a dictionary, then the property is not editable.
    if (typeof property != 'object')
      return false;

    // If the property has a UserEditable sub-property, that determines whether
    // or not it is editable.
    if (typeof property.UserEditable != 'undefined')
      return property.UserEditable;

    // Otherwise if the property has a DeviceEditable sub-property, check that.
    if (typeof property.DeviceEditable != 'undefined')
      return property.DeviceEditable;

    // If no 'Editable' sub-property exists, the policy value is not editable.
    return false;
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is enforced by a policy.
   */
  isNetworkPolicyEnforced: function(property) {
    return this.isNetworkPolicyControlled(property) &&
        !this.isEditable(property);
  },

  /**
   * @param {!CrOnc.ManagedProperty|undefined} property
   * @return {boolean} True if the network property is recommended by a policy.
   */
  isNetworkPolicyRecommended: function(property) {
    return this.isNetworkPolicyControlled(property) &&
        this.isEditable(property);
  },

  /**
   * @param {string|undefined} source
   * @return {boolean}
   * @protected
   */
  isPolicySource: function(source) {
    return !!source &&
        (source == CrOnc.Source.DEVICE_POLICY ||
         source == CrOnc.Source.USER_POLICY);
  },

  /**
   * @param {string} source
   * @return {!CrPolicyIndicatorType}
   * @private
   */
  getIndicatorTypeForSource: function(source) {
    if (source == CrOnc.Source.DEVICE_POLICY)
      return CrPolicyIndicatorType.DEVICE_POLICY;
    if (source == CrOnc.Source.USER_POLICY)
      return CrPolicyIndicatorType.USER_POLICY;
    return CrPolicyIndicatorType.NONE;
  },
};
