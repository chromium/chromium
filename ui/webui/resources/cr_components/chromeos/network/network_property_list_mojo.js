// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying a list of network properties
 * in a list. This also supports editing fields inline for fields listed in
 * editFieldTypes.
 */
(function() {
'use strict';

Polymer({
  is: 'network-property-list-mojo',

  behaviors: [I18nBehavior, CrPolicyNetworkBehaviorMojo],

  properties: {
    /**
     * The dictionary containing the properties to display.
     * @type {!Object|undefined}
     */
    propertyDict: {type: Object},

    /**
     * Fields to display.
     * @type {!Array<string>}
     */
    fields: {
      type: Array,
      value: function() {
        return [];
      },
    },

    /**
     * Edit type of editable fields. May contain a property for any field in
     * |fields|. Other properties will be ignored. Property values can be:
     *   'String' - A text input will be displayed.
     *   'StringArray' - A text input will be displayed that expects a comma
     *       separated list of strings.
     *   'Password' - A string with input type = password.
     * When a field changes, the 'property-change' event will be fired with
     * the field name and the new value provided in the event detail.
     * @type {!Object<string>}
     */
    editFieldTypes: {
      type: Object,
      value: function() {
        return {};
      },
    },

    /** Prefix used to look up property key translations. */
    prefix: {
      type: String,
      value: '',
    },
  },

  /**
   * Event triggered when an input field changes. Fires a 'property-change'
   * event with the field (property) name set to the target id, and the value
   * set to the target input value.
   * @param {!Event} event The input change event.
   * @private
   */
  onValueChange_: function(event) {
    if (!this.propertyDict) {
      return;
    }
    const key = event.target.id;
    let curValue = this.getProperty_(key);
    if (typeof curValue == 'object' && !Array.isArray(curValue)) {
      // Extract the property from an ONC managed dictionary.
      curValue = OncMojo.getActiveValue(
          /** @type{OncMojo.ManagedProperty} */ (curValue));
    }
    const newValue = this.getValueFromEditField_(key, event.target.value);
    if (newValue == curValue) {
      return;
    }
    this.fire('property-change', {field: key, value: newValue});
  },

  /**
   * Converts mojo keys to ONC keys. TODO(stevenjb): Remove this and update
   * string ids once everything is converted to mojo.
   * @param {string} key
   * @param {string=} opt_prefix
   * @return {string}
   * @private
   */
  getOncKey_: function(key, opt_prefix) {
    if (opt_prefix) {
      key = opt_prefix + key.charAt(0).toUpperCase() + key.slice(1);
    }
    let result = '';
    const subKeys = key.split('.');
    subKeys.forEach(subKey => {
      // Check for exceptions to CamelCase vs camelCase naming conventions.
      if (subKey == 'ipv4' || subKey == 'ipv6') {
        result += subKey;
      } else if (subKey == 'apn') {
        result += 'APN';
      } else if (subKey == 'ipAddress') {
        result += 'IPAddress';
      } else if (subKey == 'ipSec') {
        result += 'IPSec';
      } else if (subKey == 'l2tp') {
        result += 'L2TP';
      } else if (subKey == 'modelId') {
        result += 'ModelID';
      } else if (subKey == 'openVpn') {
        result += 'OpenVPN';
      } else if (subKey == 'otp') {
        result += 'OTP';
      } else if (subKey == 'ssid') {
        result += 'SSID';
      } else if (subKey == 'serverCa') {
        result += 'ServerCA';
      } else if (subKey == 'vpn') {
        result += 'VPN';
      } else if (subKey == 'wifi') {
        result += 'WiFi';
      } else {
        result += subKey.charAt(0).toUpperCase() + subKey.slice(1);
      }
      result += '-';
    });
    return 'Onc' + result.slice(0, result.length - 1);
  },

  /**
   * @param {string} key The property key.
   * @return {string} The text to display for the property label.
   * @private
   */
  getPropertyLabel_: function(key) {
    const oncKey = this.getOncKey_(key, this.prefix);
    if (this.i18nExists(oncKey)) {
      return this.i18n(oncKey);
    }
    // We do not provide translations for every possible network property key.
    // For keys specific to a type, strip the type prefix.
    const result = this.prefix + key;
    for (const type of ['cellular', 'ethernet', 'tether', 'vpn', 'wifi']) {
      if (result.startsWith(type + '.')) {
        return result.substr(type.length + 1);
      }
    }
    return result;
  },

  /**
   * Generates a filter function dependent on propertyDict and editFieldTypes.
   * @return {!Object} A filter used by dom-repeat.
   * @private
   */
  computeFilter_: function() {
    return key => {
      if (this.editFieldTypes.hasOwnProperty(key)) {
        return true;
      }
      const value = this.getPropertyValue_(key);
      return value !== '';
    };
  },

  /**
   * @param {string} key The property key.
   * @return {boolean}
   * @private
   */
  isPropertyEditable_: function(key) {
    if (!this.propertyDict) {
      return false;
    }
    const property = this.getProperty_(key);
    if (property === undefined || property === null) {
      // Unspecified properties in policy configurations are not user
      // modifiable. https://crbug.com/819837.
      const source = this.propertyDict.source;
      return source != chromeos.networkConfig.mojom.OncSource.kUserPolicy &&
          source != chromeos.networkConfig.mojom.OncSource.kDevicePolicy;
    }
    return !this.isNetworkPolicyEnforced(property);
  },

  /**
   * @param {string} key The property key.
   * @return {boolean} True if the edit type for the key is a valid type.
   * @private
   */
  isEditType_: function(key) {
    const editType = this.editFieldTypes[key];
    return editType == 'String' || editType == 'StringArray' ||
        editType == 'Password';
  },

  /**
   * @param {string} key The property key.
   * @return {boolean}
   * @private
   */
  isEditable_: function(key) {
    return this.isEditType_(key) && this.isPropertyEditable_(key);
  },

  /**
   * @param {string} key The property key.
   * @return {boolean}
   * @private
   */
  showEditable_: function(key) {
    return this.isEditable_(key);
  },

  /**
   * @param {string} key The property key.
   * @return {string}
   * @private
   */
  getEditInputType_: function(key) {
    return this.editFieldTypes[key] == 'Password' ? 'password' : 'text';
  },

  /**
   * @param {string} key The property key.
   * @return {!OncMojo.ManagedProperty|undefined}
   * @private
   */
  getProperty_: function(key) {
    if (!this.propertyDict) {
      return undefined;
    }
    key = OncMojo.getManagedPropertyKey(key);
    const property = this.get(key, this.propertyDict);
    if (property === null || property === undefined) {
      return undefined;
    }
    return /** @type{!OncMojo.ManagedProperty}*/ (property);
  },

  /**
   * @param {string} key The property key.
   * @return {*} The managed property dictionary associated with |key|.
   * @private
   */
  getIndicatorProperty_: function(key) {
    if (!this.propertyDict) {
      return undefined;
    }
    const property = this.getProperty_(key);
    if ((property === undefined || property === null) &&
        this.propertyDict.source) {
      const policySource = OncMojo.getEnforcedPolicySourceFromOncSource(
          this.propertyDict.source);
      if (policySource != chromeos.networkConfig.mojom.PolicySource.kNone) {
        // If the dictionary is policy controlled, provide an empty property
        // object with the network policy source. See https://crbug.com/819837
        // for more info.
        return /** @type{!OncMojo.ManagedProperty} */ ({
          activeValue: '',
          policySource: policySource,
        });
      }
      // Otherwise just return undefined.
    }
    return property;
  },

  /**
   * @param {string} key The property key.
   * @return {string} The text to display for the property value.
   * @private
   */
  getPropertyValue_: function(key) {
    let value = this.getProperty_(key);
    if (value === undefined || value === null) {
      return '';
    }
    if (typeof value == 'object' && !Array.isArray(value)) {
      // Extract the property from an ONC managed dictionary
      value = OncMojo.getActiveValue(
          /** @type {!OncMojo.ManagedProperty} */ (value));
    }
    if (Array.isArray(value)) {
      return value.join(', ');
    }

    const customValue = this.getCustomPropertyValue_(key, value);
    if (customValue) {
      return customValue;
    }
    if (typeof value == 'boolean') {
      return value.toString();
    }

    let valueStr;
    if (typeof value == 'number') {
      // Special case typed managed properties.
      if (key == 'cellular.activationState') {
        valueStr = OncMojo.getActivationStateTypeString(
            /** @type{!chromeos.networkConfig.mojom.ActivationStateType}*/ (
                value));
      } else if (key == 'vpn.type') {
        valueStr = OncMojo.getVpnTypeString(
            /** @type{!chromeos.networkConfig.mojom.VpnType}*/ (value));
      } else if (key == 'wifi.security') {
        valueStr = OncMojo.getSecurityTypeString(
            /** @type{!chromeos.networkConfig.mojom.SecurityType}*/ (value));
      } else {
        return value.toString();
      }
    } else {
      assert(typeof value == 'string');
      valueStr = /** @type {string} */ (value);
    }
    const oncKey = this.getOncKey_(key, this.prefix) + '_' + valueStr;
    if (this.i18nExists(oncKey)) {
      return this.i18n(oncKey);
    }
    return valueStr;
  },

  /**
   * Converts edit field values to the correct edit type.
   * @param {string} key The property key.
   * @param {*} fieldValue The value from the field.
   * @return {*}
   * @private
   */
  getValueFromEditField_(key, fieldValue) {
    const editType = this.editFieldTypes[key];
    if (editType == 'StringArray') {
      return fieldValue.toString().split(/, */);
    }
    return fieldValue;
  },

  /**
   * @param {string} key The property key.
   * @param {*} value The property value.
   * @return {string} The text to display for the property value. If the key
   *     does not correspond to a custom property, an empty string is returned.
   */
  getCustomPropertyValue_: function(key, value) {
    if (key == 'tether.batteryPercentage') {
      assert(typeof value == 'number');
      return this.i18n('OncTether-BatteryPercentage_Value', value.toString());
    }

    if (key == 'tether.signalStrength') {
      assert(typeof value == 'number');
      // Possible |signalStrength| values should be 0, 25, 50, 75, and 100. Add
      // <= checks for robustness.
      if (value <= 24) {
        return this.i18n('OncTether-SignalStrength_Weak');
      }
      if (value <= 49) {
        return this.i18n('OncTether-SignalStrength_Okay');
      }
      if (value <= 74) {
        return this.i18n('OncTether-SignalStrength_Good');
      }
      if (value <= 99) {
        return this.i18n('OncTether-SignalStrength_Strong');
      }
      return this.i18n('OncTether-SignalStrength_VeryStrong');
    }

    if (key == 'tether.carrier') {
      assert(typeof value == 'string');
      return (!value || value == 'unknown-carrier') ?
          this.i18n('tetherUnknownCarrier') :
          value;
    }

    return '';
  },
});
})();
