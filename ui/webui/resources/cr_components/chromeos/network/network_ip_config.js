// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for displaying the IP Config properties for
 * a network state.
 */
(function() {
'use strict';

/**
 * Returns the routing prefix as a string for a given prefix length. If
 * |prefixLength| is invalid, returns undefined.
 * @param {number} prefixLength The ONC routing prefix length.
 * @return {string|undefined}
 */
const getRoutingPrefixAsNetmask = function(prefixLength) {
  'use strict';
  // Return the empty string for invalid inputs.
  if (prefixLength <= 0 || prefixLength > 32) {
    return undefined;
  }
  let netmask = '';
  for (let i = 0; i < 4; ++i) {
    let remainder = 8;
    if (prefixLength >= 8) {
      prefixLength -= 8;
    } else {
      remainder = prefixLength;
      prefixLength = 0;
    }
    if (i > 0) {
      netmask += '.';
    }
    let value = 0;
    if (remainder != 0) {
      value = ((2 << (remainder - 1)) - 1) << (8 - remainder);
    }
    netmask += value.toString();
  }
  return netmask;
};

/**
 * Returns the routing prefix length as a number from the netmask string.
 * @param {string} netmask The netmask string, e.g. 255.255.255.0.
 * @return {number} The corresponding netmask or NO_ROUTING_PREFIX if invalid.
 */
const getRoutingPrefixAsLength = function(netmask) {
  'use strict';
  let prefixLength = 0;
  const tokens = netmask.split('.');
  if (tokens.length != 4) {
    return -1;
  }
  for (let i = 0; i < tokens.length; ++i) {
    const token = tokens[i];
    // If we already found the last mask and the current one is not
    // '0' then the netmask is invalid. For example, 255.224.255.0
    if (prefixLength / 8 != i) {
      if (token != '0') {
        return chromeos.networkConfig.mojom.NO_ROUTING_PREFIX;
      }
    } else if (token == '255') {
      prefixLength += 8;
    } else if (token == '254') {
      prefixLength += 7;
    } else if (token == '252') {
      prefixLength += 6;
    } else if (token == '248') {
      prefixLength += 5;
    } else if (token == '240') {
      prefixLength += 4;
    } else if (token == '224') {
      prefixLength += 3;
    } else if (token == '192') {
      prefixLength += 2;
    } else if (token == '128') {
      prefixLength += 1;
    } else if (token == '0') {
      prefixLength += 0;
    } else {
      // mask is not a valid number.
      return chromeos.networkConfig.mojom.NO_ROUTING_PREFIX;
    }
  }
  return prefixLength;
};

Polymer({
  is: 'network-ip-config',

  behaviors: [I18nBehavior, CrPolicyNetworkBehaviorMojo],

  properties: {
    /** @private {!chromeos.networkConfig.mojom.ManagedProperties|undefined} */
    managedProperties: {
      type: Object,
      observer: 'managedPropertiesChanged_',
    },

    /**
     * State of 'Configure IP Addresses Automatically'.
     * @private
     */
    automatic_: {
      type: Boolean,
      value: true,
    },

    /**
     * The currently visible IP Config property dictionary.
     * @private {{
     *   ipv4: (OncMojo.IPConfigUIProperties|undefined),
     *   ipv6: (OncMojo.IPConfigUIProperties|undefined)
     * }|undefined}
     */
    ipConfig_: Object,

    /**
     * Array of properties to pass to the property list.
     * @private {!Array<string>}
     */
    ipConfigFields_: {
      type: Array,
      value: function() {
        return [
          'ipv4.ipAddress',
          'ipv4.routingPrefix',
          'ipv4.gateway',
          'ipv6.ipAddress',
        ];
      },
      readOnly: true
    },
  },

  /**
   * Saved static IP configuration properties when switching to 'automatic'.
   * @private {!OncMojo.IPConfigUIProperties|undefined}
   */
  savedStaticIp_: undefined,

  /** @private */
  managedPropertiesChanged_: function(newValue, oldValue) {
    if (!this.managedProperties) {
      return;
    }

    const properties = this.managedProperties;
    if (newValue.guid != (oldValue && oldValue.guid)) {
      this.savedStaticIp_ = undefined;
    }

    // Update the 'automatic' property.
    if (properties.ipAddressConfigType) {
      const ipConfigType =
          OncMojo.getActiveValue(properties.ipAddressConfigType);
      this.automatic_ = ipConfigType != 'Static';
    }

    if (properties.ipConfigs || properties.staticIpConfig) {
      // Update the 'ipConfig' property.
      const ipv4 = this.getIPConfigUIProperties_(
          OncMojo.getIPConfigForType(properties, 'IPv4'));
      let ipv6 = this.getIPConfigUIProperties_(
          OncMojo.getIPConfigForType(properties, 'IPv6'));
      // If connected and the IP address is automatic and set, show 'Loading' if
      // the ipv6 address is not set.
      if (OncMojo.connectionStateIsConnected(properties.connectionState) &&
          this.automatic_ && ipv4 && ipv4.ipAddress) {
        ipv6 = ipv6 || {};
        ipv6.ipAddress = ipv6.ipAddress || this.i18n('loading');
      }
      this.ipConfig_ = {ipv4: ipv4, ipv6: ipv6};
    } else {
      this.ipConfig_ = undefined;
    }
  },

  /**
   * Checks whether IP address config type can be changed.
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @return {boolean}
   * @private
   */
  canChangeIPConfigType_: function(managedProperties) {
    const ipConfigType = managedProperties.ipAddressConfigType;
    return !ipConfigType || !this.isNetworkPolicyEnforced(ipConfigType);
  },

  /** @private */
  onAutomaticChange_: function() {
    if (!this.automatic_) {
      const defaultIpv4 = {
        gateway: '192.168.1.1',
        ipAddress: '192.168.1.1',
        routingPrefix: '255.255.255.0',
        type: 'IPv4',
      };
      // Ensure that there is a valid IPConfig object. Copy any set properties
      // over the default properties to ensure all properties are set.
      if (this.ipConfig_) {
        this.ipConfig_.ipv4 = Object.assign(defaultIpv4, this.ipConfig_.ipv4);
      } else {
        this.ipConfig_ = {ipv4: defaultIpv4};
      }
      this.sendStaticIpConfig_();
      return;
    }

    // Save the static IP configuration when switching to automatic.
    if (this.ipConfig_) {
      this.savedStaticIp_ = this.ipConfig_.ipv4;
    }
    this.fire('ip-change', {
      field: 'ipAddressConfigType',
      value: 'DHCP',
    });
  },

  /**
   * @param {!chromeos.networkConfig.mojom.IPConfigProperties|undefined}
   *     ipconfig
   * @return {!OncMojo.IPConfigUIProperties|undefined} A new
   *     IPConfigUIProperties object with routingPrefix expressed as a string
   *     mask instead of a prefix length. Returns undefined if |ipconfig| is not
   *     defined.
   * @private
   */
  getIPConfigUIProperties_: function(ipconfig) {
    if (!ipconfig) {
      return undefined;
    }
    const result = {};
    for (const key in ipconfig) {
      const value = ipconfig[key];
      if (key == 'routingPrefix') {
        const netmask = getRoutingPrefixAsNetmask(value);
        if (netmask !== undefined) {
          result.routingPrefix = netmask;
        }
      } else {
        result[key] = value;
      }
    }
    return result;
  },

  /**
   * @param {!OncMojo.IPConfigUIProperties} ipconfig
   * @return {!chromeos.networkConfig.mojom.IPConfigProperties} A new
   *     IPConfigProperties object with RoutingPrefix expressed as a a prefix
   *     length.
   * @private
   */
  getIPConfigProperties_: function(ipconfig) {
    const result = {};
    for (const key in ipconfig) {
      const value = ipconfig[key];
      if (key == 'routingPrefix') {
        const routingPrefix = getRoutingPrefixAsLength(value);
        if (routingPrefix != chromeos.networkConfig.mojom.NO_ROUTING_PREFIX) {
          result.routingPrefix = routingPrefix;
        }
      } else {
        result[key] = value;
      }
    }
    return result;
  },

  /**
   * @return {boolean}
   * @private
   */
  hasIpConfigFields_: function() {
    if (!this.ipConfigFields_) {
      return false;
    }
    for (let i = 0; i < this.ipConfigFields_.length; ++i) {
      if (this.get(this.ipConfigFields_[i], this.ipConfig_) != undefined) {
        return true;
      }
    }
    return false;
  },

  /**
   * @param {string} path path to a property inside of |managedProperties|.
   * @return {string|undefined} Edit type to be used in network-property-list
   *     for the given path.
   * @private
   */
  getIPFieldEditType_: function(path) {
    if (!this.managedProperties) {
      return undefined;
    }
    const property = /** @type{!OncMojo.ManagedProperty|undefined}*/ (
        this.get(path, this.managedProperties));
    return (property && this.isNetworkPolicyEnforced(property)) ? undefined :
                                                                  'String';
  },

  /**
   * @return {Object} An object with the edit type for each editable field.
   * @private
   */
  getIPEditFields_: function() {
    if (this.automatic_ || !this.managedProperties) {
      return {};
    }
    return {
      'ipv4.ipAddress': this.getIPFieldEditType_('staticIpConfig.ipAddress'),
      'ipv4.routingPrefix':
          this.getIPFieldEditType_('staticIpConfig.routingPrefix'),
      'ipv4.gateway': this.getIPFieldEditType_('staticIpConfig.gateway')
    };
  },

  /**
   * Event triggered when the network property list changes.
   * @param {!CustomEvent<!{field: string, value: string}>} event The
   *     network-property-list change event.
   * @private
   */
  onIPChange_: function(event) {
    if (!this.ipConfig_) {
      return;
    }
    const field = event.detail.field;
    const value = event.detail.value;
    // Note: |field| includes the 'ipv4.' prefix.
    this.set('ipConfig_.' + field, value);
    this.sendStaticIpConfig_();
  },

  /** @private */
  sendStaticIpConfig_: function() {
    // This will also set IPAddressConfigType to STATIC.
    this.fire('ip-change', {
      field: 'staticIpConfig',
      value: this.ipConfig_.ipv4 ?
          this.getIPConfigProperties_(this.ipConfig_.ipv4) :
          {}
    });
  },
});
})();
