// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Behavior for policy controlled indicators.
 * TODO(michaelpg): Since extensions can also control settings and be indicated,
 * rework the "policy" naming scheme throughout this directory.
 */

/**
 * Strings required for policy indicators. These must be set at runtime.
 * Chrome OS only strings may be undefined.
 * @type {{
 *   controlledSettingExtension: string,
 *   controlledSettingExtensionWithoutName: string,
 *   controlledSettingPolicy: string,
 *   controlledSettingRecommendedMatches: string,
 *   controlledSettingRecommendedDiffers: string,
 *   controlledSettingShared: (string|undefined),
 *   controlledSettingWithOwner: string,
 *   controlledSettingNoOwner: string,
 *   controlledSettingParent: string,
 *   controlledSettingChildRestriction: string,
 * }}
 */
// eslint-disable-next-line no-var
var CrPolicyStrings;

/** @enum {string} */
const CrPolicyIndicatorType = {
  DEVICE_POLICY: 'devicePolicy',
  EXTENSION: 'extension',
  NONE: 'none',
  OWNER: 'owner',
  PRIMARY_USER: 'primary_user',
  RECOMMENDED: 'recommended',
  USER_POLICY: 'userPolicy',
  PARENT: 'parent',
  CHILD_RESTRICTION: 'childRestriction',
};

/** @polymerBehavior */
const CrPolicyIndicatorBehavior = {
  // Properties exposed to all policy indicators.
  properties: {
    /**
     * Which indicator type to show (or NONE).
     * @type {CrPolicyIndicatorType}
     */
    indicatorType: {
      type: String,
      value: CrPolicyIndicatorType.NONE,
    },

    /**
     * The name associated with the policy source. See
     * chrome.settingsPrivate.PrefObject.controlledByName.
     */
    indicatorSourceName: {
      type: String,
      value: '',
    },

    // Computed properties based on indicatorType and indicatorSourceName.
    // Override to provide different values.

    indicatorVisible: {
      type: Boolean,
      computed: 'getIndicatorVisible_(indicatorType)',
    },

    indicatorIcon: {
      type: String,
      computed: 'getIndicatorIcon_(indicatorType)',
    },
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @return {boolean} True if the indicator should be shown.
   * @private
   */
  getIndicatorVisible_: function(type) {
    return type != CrPolicyIndicatorType.NONE;
  },

  /**
   * @param {CrPolicyIndicatorType} type
   * @return {string} The iron-icon icon name.
   * @private
   */
  getIndicatorIcon_: function(type) {
    switch (type) {
      case CrPolicyIndicatorType.EXTENSION:
        return 'cr:extension';
      case CrPolicyIndicatorType.NONE:
        return '';
      case CrPolicyIndicatorType.PRIMARY_USER:
        return 'cr:group';
      case CrPolicyIndicatorType.OWNER:
        return 'cr:person';
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
      case CrPolicyIndicatorType.RECOMMENDED:
        return 'cr20:domain';
      case CrPolicyIndicatorType.PARENT:
      case CrPolicyIndicatorType.CHILD_RESTRICTION:
        return 'cr20:kite';
      default:
        assertNotReached();
    }
  },

  /**
   * @param {!CrPolicyIndicatorType} type
   * @param {string} name The name associated with the indicator. See
   *     chrome.settingsPrivate.PrefObject.controlledByName
   * @param {boolean=} opt_matches For RECOMMENDED only, whether the indicator
   *     value matches the recommended value.
   * @return {string} The tooltip text for |type|.
   */
  getIndicatorTooltip: function(type, name, opt_matches) {
    if (!CrPolicyStrings) {
      return '';
    }  // Tooltips may not be defined, e.g. in OOBE.
    switch (type) {
      case CrPolicyIndicatorType.EXTENSION:
        return name.length > 0 ?
            CrPolicyStrings.controlledSettingExtension.replace('$1', name) :
            CrPolicyStrings.controlledSettingExtensionWithoutName;
      case CrPolicyIndicatorType.PRIMARY_USER:
        return CrPolicyStrings.controlledSettingShared.replace('$1', name);
      case CrPolicyIndicatorType.OWNER:
        return name.length > 0 ?
            CrPolicyStrings.controlledSettingWithOwner.replace('$1', name) :
            CrPolicyStrings.controlledSettingNoOwner;
      case CrPolicyIndicatorType.USER_POLICY:
      case CrPolicyIndicatorType.DEVICE_POLICY:
        return CrPolicyStrings.controlledSettingPolicy;
      case CrPolicyIndicatorType.RECOMMENDED:
        return opt_matches ?
            CrPolicyStrings.controlledSettingRecommendedMatches :
            CrPolicyStrings.controlledSettingRecommendedDiffers;
      case CrPolicyIndicatorType.PARENT:
        return CrPolicyStrings.controlledSettingParent;
      case CrPolicyIndicatorType.CHILD_RESTRICTION:
        return CrPolicyStrings.controlledSettingChildRestriction;
    }
    return '';
  },
};
