// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview This file has three parts:
 *
 * 1. A dictionary of strings for network element translations.
 *
 * 2. Typedefs for network properties. Note: These 'types' define a subset of
 * ONC properties in the ONC data dictionary. The first letter is capitalized to
 * match the ONC spec and avoid an extra layer of translation.
 * See components/onc/docs/onc_spec.html for the complete spec.
 * TODO(stevenjb): Replace with chrome.networkingPrivate.NetworkStateProperties
 * once that is fully defined.
 *
 * 3. Helper functions to facilitate extracting and setting ONC properties.
 */

/**
 * Strings required for networking elements. These must be set at runtime.
 * @type {{
 *   OncTypeCellular: string,
 *   OncTypeEthernet: string,
 *   OncTypeTether: string,
 *   OncTypeVPN: string,
 *   OncTypeWiFi: string,
 *   OncTypeWiMAX: string,
 *   networkListItemConnected: string,
 *   networkListItemConnecting: string,
 *   networkListItemConnectingTo: string,
 *   networkListItemInitializing: string,
 *   networkListItemScanning: string,
 *   networkListItemNotConnected: string,
 *   networkListItemNoNetwork: string,
 *   vpnNameTemplate: string,
 * }}
 */
var CrOncStrings;

var CrOnc = {};

/** @typedef {chrome.networkingPrivate.NetworkStateProperties} */
CrOnc.NetworkStateProperties;

/** @typedef {chrome.networkingPrivate.ManagedProperties} */
CrOnc.NetworkProperties;

/** @typedef {string|number|boolean|Array<string>|Object|Array<Object>} */
CrOnc.NetworkPropertyType;

/**
 * Generic managed property type. This should match any of the basic managed
 * types in chrome.networkingPrivate, e.g. networkingPrivate.ManagedBoolean.
 * @typedef {{
 *   Active: (!CrOnc.NetworkPropertyType|undefined),
 *   Effective: (string|undefined),
 *   UserPolicy: (!CrOnc.NetworkPropertyType|undefined),
 *   DevicePolicy: (!CrOnc.NetworkPropertyType|undefined),
 *   UserSetting: (!CrOnc.NetworkPropertyType|undefined),
 *   SharedSetting: (!CrOnc.NetworkPropertyType|undefined),
 *   UserEditable: (boolean|undefined),
 *   DeviceEditable: (boolean|undefined)
 * }}
 */
CrOnc.ManagedProperty;

/** @typedef {chrome.networkingPrivate.SIMLockStatus} */
CrOnc.SIMLockStatus;

/** @typedef {chrome.networkingPrivate.APNProperties} */
CrOnc.APNProperties;

/** @typedef {chrome.networkingPrivate.CellularSimState} */
CrOnc.CellularSimState;

/** @typedef {chrome.networkingPrivate.DeviceStateProperties} */
CrOnc.DeviceStateProperties;

/** @typedef {chrome.networkingPrivate.IPConfigProperties} */
CrOnc.IPConfigProperties;

/** @typedef {chrome.networkingPrivate.ManualProxySettings} */
CrOnc.ManualProxySettings;

/** @typedef {chrome.networkingPrivate.ProxyLocation} */
CrOnc.ProxyLocation;

/** @typedef {chrome.networkingPrivate.ProxySettings} */
CrOnc.ProxySettings;

// Modified version of IPConfigProperties to store RoutingPrefix as a
// human-readable string instead of as a number.
/**
 * @typedef {{
 *   Gateway: (string|undefined),
 *   IPAddress: (string|undefined),
 *   NameServers: (!Array<string>|undefined),
 *   RoutingPrefix: (string|undefined),
 *   Type: (string|undefined),
 *   WebProxyAutoDiscoveryUrl: (string|undefined)
 * }}
 */
CrOnc.IPConfigUIProperties;

/** @typedef {chrome.networkingPrivate.PaymentPortal} */
CrOnc.PaymentPortal;

/** @enum {string} */
CrOnc.ActivationState = chrome.networkingPrivate.ActivationStateType;

/** @enum {string} */
CrOnc.ConnectionState = chrome.networkingPrivate.ConnectionStateType;

/** @enum {string} */
CrOnc.DeviceState = chrome.networkingPrivate.DeviceStateType;

/** @enum {string} */
CrOnc.IPConfigType = chrome.networkingPrivate.IPConfigType;

/** @enum {string} */
CrOnc.ProxySettingsType = chrome.networkingPrivate.ProxySettingsType;

/** @enum {string} */
CrOnc.Type = chrome.networkingPrivate.NetworkType;

/** @enum {string} */
CrOnc.Authentication = {
  NONE: 'None',
  WEP_8021X: '8021X',
};

/** @enum {string} */
CrOnc.IPsecAuthenticationType = {
  CERT: 'Cert',
  PSK: 'PSK',
};

/** @enum {string} */
CrOnc.IPType = {
  IPV4: 'IPv4',
  IPV6: 'IPv6',
};

/** @enum {string} */
CrOnc.EAPType = {
  LEAP: 'LEAP',
  PEAP: 'PEAP',
  EAP_TLS: 'EAP-TLS',
  EAP_TTLS: 'EAP-TTLS',
};

/** @enum {string} */
CrOnc.LockType = {
  NONE: '',
  PIN: 'sim-pin',
  PUK: 'sim-puk',
};

/** @enum {string} */
CrOnc.NetworkTechnology = {
  CDMA1XRTT: 'CDMA1XRTT',
  EDGE: 'EDGE',
  EVDO: 'EVDO',
  GPRS: 'GPRS',
  GSM: 'GSM',
  HSPA: 'HSPA',
  HSPA_PLUS: 'HSPAPlus',
  LTE: 'LTE',
  LTE_ADVANCED: 'LTEAdvanced',
  UMTS: 'UMTS',
  UNKNOWN: 'Unknown',
};

/** @enum {string} */
CrOnc.RoamingState = {
  HOME: 'Home',
  REQUIRED: 'Required',
  ROAMING: 'Roaming',
  UNKNOWN: 'Unknown',
};

/** @enum {string} */
CrOnc.Security = {
  NONE: 'None',
  WEP_8021X: 'WEP-8021X',
  WEP_PSK: 'WEP-PSK',
  WPA_EAP: 'WPA-EAP',
  WPA_PSK: 'WPA-PSK',
};

/** @enum {string} */
CrOnc.Source = {
  NONE: 'None',
  DEVICE: 'Device',
  DEVICE_POLICY: 'DevicePolicy',
  USER: 'User',
  USER_POLICY: 'UserPolicy',
  ACTIVE_EXTENSION: 'ActiveExtension',
};

/** @enum {string} */
CrOnc.UserAuthenticationType = {
  NONE: 'None',
  OTP: 'OTP',
  PASSWORD: 'Password',
  PASSWORD_AND_OTP: 'PasswordAndOTP',
};

/** @enum {string} */
CrOnc.VPNType = {
  L2TP_IPSEC: 'L2TP-IPsec',
  OPEN_VPN: 'OpenVPN',
  THIRD_PARTY_VPN: 'ThirdPartyVPN',
  ARCVPN: 'ARCVPN',
};

/**
 * Helper function to retrieve the active ONC property value from a managed
 * dictionary.
 * @param {!CrOnc.ManagedProperty|undefined} property The managed dictionary
 *     for the property if it exists or undefined.
 * @return {!CrOnc.NetworkPropertyType|undefined} The active property value
 *     if it exists, otherwise undefined.
 */
CrOnc.getActiveValue = function(property) {
  if (property == undefined)
    return undefined;

  if (typeof property != 'object' || Array.isArray(property)) {
    console.error(
        'getActiveValue called on non object: ' + JSON.stringify(property));
    return undefined;
  }

  // Return the Active value if it exists.
  if ('Active' in property)
    return property['Active'];

  // If no Active value is defined, return the effective value.
  if ('Effective' in property) {
    var effective = property.Effective;
    if (effective in property)
      return property[effective];
  }

  // If no Effective value, return the UserSetting or SharedSetting.
  if ('UserSetting' in property)
    return property['UserSetting'];
  if ('SharedSetting' in property)
    return property['SharedSetting'];

  console.error(
      'getActiveValue called on invalid ONC object: ' +
      JSON.stringify(property));
  return undefined;
};

/**
 * Return the active value for a managed or unmanaged string property.
 * @param {!CrOnc.ManagedProperty|string|undefined} property
 * @return {string}
 */
CrOnc.getStateOrActiveString = function(property) {
  if (property === undefined)
    return '';
  if (typeof property == 'string')
    return property;
  return /** @type {string} */ (CrOnc.getActiveValue(property));
};

/**
 * Return if the property is simple, i.e. doesn't contain any nested
 * dictionaries.
 * @param property {!Object|undefined}
 * @return {boolean}
 */
CrOnc.isSimpleProperty = function(property) {
  for (var prop of ['Active', 'Effective', 'UserSetting', 'SharedSetting']) {
    if (prop in property)
      return true;
  }
  return false;
};

/**
 * Converts a managed ONC dictionary into an unmanaged dictionary (i.e. a
 * dictionary of active values).
 * @param {!Object|undefined} properties A managed ONC dictionary
 * @return {!Object|undefined} An unmanaged version of |properties|.
 */
CrOnc.getActiveProperties = function(properties) {
  'use strict';
  if (!properties)
    return undefined;
  var result = {};
  var keys = Object.keys(properties);
  for (var i = 0; i < keys.length; ++i) {
    var k = keys[i];
    var property = properties[k];
    // Skip policy properties with no effective value.
    // TODO(nikitapodguzov@ / raleksandrov@): Remove this when crbug.com/888959
    // providing dummy values for password fields will be fixed.
    if ('Effective' in property && !(property.Effective in property))
      continue;
    var propertyValue;
    if (CrOnc.isSimpleProperty(property))
      propertyValue = CrOnc.getActiveValue(property);
    else
      propertyValue = CrOnc.getActiveProperties(property);
    if (propertyValue == undefined) {
      console.error(
          'getActiveProperties called on invalid ONC object: ' +
          JSON.stringify(properties));
      return undefined;
    }
    result[k] = propertyValue;
  }
  return result;
};

/**
 * Returns an IPConfigProperties object for |type|. For IPV4, these will be the
 * static properties if IPAddressConfigType is Static and StaticIPConfig is set.
 * @param {!CrOnc.NetworkProperties|undefined} properties The ONC properties.
 * @param {!CrOnc.IPType} type The IP Config type.
 * @return {CrOnc.IPConfigProperties|undefined} The IP Config object, or
 *     undefined if no properties for |type| are available.
 */
CrOnc.getIPConfigForType = function(properties, type) {
  'use strict';
  /** @type {!CrOnc.IPConfigProperties|undefined} */ var ipConfig = undefined;
  /** @type {!CrOnc.IPType|undefined} */ var ipType = undefined;
  var ipConfigs = properties.IPConfigs;
  if (ipConfigs) {
    for (var i = 0; i < ipConfigs.length; ++i) {
      ipConfig = ipConfigs[i];
      ipType = ipConfig.Type ? /** @type {CrOnc.IPType} */ (ipConfig.Type) :
                               undefined;
      if (ipType == type)
        break;
    }
  }
  if (type != CrOnc.IPType.IPV4)
    return type == ipType ? ipConfig : undefined;

  var staticIpConfig =
      /** @type {!CrOnc.IPConfigProperties|undefined} */ (
          CrOnc.getActiveProperties(properties.StaticIPConfig));
  if (!staticIpConfig)
    return ipConfig;

  // If there is no entry in IPConfigs for |type|, return the static config.
  if (!ipConfig)
    return staticIpConfig;

  // Otherwise, merge the appropriate static values into the result.
  if (staticIpConfig.IPAddress &&
      CrOnc.getActiveValue(properties.IPAddressConfigType) == 'Static') {
    ipConfig.Gateway = staticIpConfig.Gateway;
    ipConfig.IPAddress = staticIpConfig.IPAddress;
    ipConfig.RoutingPrefix = staticIpConfig.RoutingPrefix;
    ipConfig.Type = staticIpConfig.Type;
  }
  if (staticIpConfig.NameServers &&
      CrOnc.getActiveValue(properties.NameServersConfigType) == 'Static') {
    ipConfig.NameServers = staticIpConfig.NameServers;
  }
  return ipConfig;
};

/**
 * Gets the SignalStrength value from |properties| based on properties.Type.
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @return {number} The signal strength value if it exists or 0.
 */
CrOnc.getSignalStrength = function(properties) {
  var type = properties.Type;
  if (type == CrOnc.Type.CELLULAR && properties.Cellular)
    return properties.Cellular.SignalStrength || 0;
  if (type == CrOnc.Type.TETHER && properties.Tether)
    return properties.Tether.SignalStrength || 0;
  if (type == CrOnc.Type.WI_FI && properties.WiFi)
    return properties.WiFi.SignalStrength || 0;
  if (type == CrOnc.Type.WI_MAX && properties.WiMAX)
    return properties.WiMAX.SignalStrength || 0;
  return 0;
};

/**
 * Gets the Managed AutoConnect dictionary from |properties| based on
 * properties.Type.
 * @param {!CrOnc.NetworkProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @return {!chrome.networkingPrivate.ManagedBoolean|undefined} The AutoConnect
 *     managed dictionary or undefined.
 */
CrOnc.getManagedAutoConnect = function(properties) {
  var type = properties.Type;
  if (type == CrOnc.Type.CELLULAR && properties.Cellular)
    return properties.Cellular.AutoConnect;
  if (type == CrOnc.Type.VPN && properties.VPN)
    return properties.VPN.AutoConnect;
  if (type == CrOnc.Type.WI_FI && properties.WiFi)
    return properties.WiFi.AutoConnect;
  if (type == CrOnc.Type.WI_MAX && properties.WiMAX)
    return properties.WiMAX.AutoConnect;
  return undefined;
};

/**
 * Gets the AutoConnect value from |properties| based on properties.Type.
 * @param {!CrOnc.NetworkProperties|undefined}
 *     properties The ONC network properties properties.
 * @return {boolean} The AutoConnect value if it exists or false.
 */
CrOnc.getAutoConnect = function(properties) {
  var autoconnect = CrOnc.getManagedAutoConnect(properties);
  return !!CrOnc.getActiveValue(autoconnect);
};

/**
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @return {string} The name to display for |network|.
 */
CrOnc.getNetworkName = function(properties) {
  if (!properties)
    return '';
  var name = CrOnc.getStateOrActiveString(properties.Name);
  var type = CrOnc.getStateOrActiveString(properties.Type);
  if (!name)
    return CrOncStrings['OncType' + type];
  if (type == 'VPN' && properties.VPN) {
    var vpnType = CrOnc.getStateOrActiveString(properties.VPN.Type);
    if (vpnType == 'ThirdPartyVPN' && properties.VPN.ThirdPartyVPN) {
      var providerName = properties.VPN.ThirdPartyVPN.ProviderName;
      if (providerName) {
        return CrOncStrings.vpnNameTemplate.replace('$1', providerName)
            .replace('$2', name);
      }
    }
  }
  return name;
};

/**
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     properties The ONC network properties or state properties.
 * @return {string} The name to display for |network|.
 */
CrOnc.getEscapedNetworkName = function(properties) {
  return HTMLEscape(CrOnc.getNetworkName(properties));
};

/**
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *   properties The ONC network properties or state properties.
 * @return {boolean} True if |properties| is a Cellular network with a
 *   locked SIM.
 */
CrOnc.isSimLocked = function(properties) {
  if (!properties.Cellular)
    return false;
  var simLockStatus = properties.Cellular.SIMLockStatus;
  if (simLockStatus == undefined)
    return false;
  return simLockStatus.LockType == CrOnc.LockType.PIN ||
      simLockStatus.LockType == CrOnc.LockType.PUK;
};

/**
 * Modifies |config| to include the correct set of properties for configuring
 * a network IP Address and NameServer configuration for |state|. Existing
 * properties in |config| will be preserved unless invalid.
 * @param {!chrome.networkingPrivate.NetworkConfigProperties} config A partial
 *     ONC configuration.
 * @param {CrOnc.NetworkProperties|undefined} properties The ONC properties.
 */
CrOnc.setValidStaticIPConfig = function(config, properties) {
  if (!config.IPAddressConfigType) {
    var ipConfigType = /** @type {chrome.networkingPrivate.IPConfigType} */ (
        CrOnc.getActiveValue(properties.IPAddressConfigType));
    config.IPAddressConfigType = ipConfigType || CrOnc.IPConfigType.DHCP;
  }
  if (!config.NameServersConfigType) {
    var nsConfigType = /** @type {chrome.networkingPrivate.IPConfigType} */ (
        CrOnc.getActiveValue(properties.NameServersConfigType));
    config.NameServersConfigType = nsConfigType || CrOnc.IPConfigType.DHCP;
  }
  if (config.IPAddressConfigType != CrOnc.IPConfigType.STATIC &&
      config.NameServersConfigType != CrOnc.IPConfigType.STATIC) {
    if (config.hasOwnProperty('StaticIPConfig'))
      delete config.StaticIPConfig;
    return;
  }

  if (!config.hasOwnProperty('StaticIPConfig')) {
    config.StaticIPConfig =
        /** @type {chrome.networkingPrivate.IPConfigProperties} */ ({});
  }
  var staticIP = config.StaticIPConfig;
  var stateIPConfig = CrOnc.getIPConfigForType(properties, CrOnc.IPType.IPV4);
  if (config.IPAddressConfigType == 'Static') {
    staticIP.Gateway = staticIP.Gateway || stateIPConfig.Gateway || '';
    staticIP.IPAddress = staticIP.IPAddress || stateIPConfig.IPAddress || '';
    staticIP.RoutingPrefix =
        staticIP.RoutingPrefix || stateIPConfig.RoutingPrefix || 0;
    staticIP.Type = staticIP.Type || stateIPConfig.Type || CrOnc.IPType.IPV4;
  }
  if (config.NameServersConfigType == 'Static') {
    staticIP.NameServers =
        staticIP.NameServers || stateIPConfig.NameServers || [];
  }
};


/**
 * Sets the value of a property in an ONC dictionary.
 * @param {!chrome.networkingPrivate.NetworkConfigProperties} properties
 *     The ONC property dictionary to modify.
 * @param {string} key The property key which may be nested, e.g. 'Foo.Bar'.
 * @param {!CrOnc.NetworkPropertyType|undefined} value The property value to
 *     set. If undefined the property will be removed.
 */
CrOnc.setProperty = function(properties, key, value) {
  while (true) {
    var index = key.indexOf('.');
    if (index < 0)
      break;
    var keyComponent = key.substr(0, index);
    if (!properties.hasOwnProperty(keyComponent))
      properties[keyComponent] = {};
    properties = properties[keyComponent];
    key = key.substr(index + 1);
  }
  if (value === undefined)
    delete properties[key];
  else
    properties[key] = value;
};

/**
 * Calls setProperty with '{state.Type}.key', e.g. WiFi.AutoConnect.
 * @param {!chrome.networkingPrivate.NetworkConfigProperties} properties The
 *     ONC properties to set. properties.Type must be set already.
 * @param {string} key The type property key, e.g. 'AutoConnect'.
 * @param {!CrOnc.NetworkPropertyType|undefined} value The property value to
 *     set. If undefined the property will be removed.
 */
CrOnc.setTypeProperty = function(properties, key, value) {
  if (properties.Type == undefined) {
    console.error(
        'Type not defined in properties: ' + JSON.stringify(properties));
    return;
  }
  var typeKey = properties.Type + '.' + key;
  CrOnc.setProperty(properties, typeKey, value);
};

/**
 * Returns the routing prefix as a string for a given prefix length.
 * @param {number} prefixLength The ONC routing prefix length.
 * @return {string} The corresponding netmask.
 */
CrOnc.getRoutingPrefixAsNetmask = function(prefixLength) {
  'use strict';
  // Return the empty string for invalid inputs.
  if (prefixLength < 0 || prefixLength > 32)
    return '';
  var netmask = '';
  for (var i = 0; i < 4; ++i) {
    var remainder = 8;
    if (prefixLength >= 8) {
      prefixLength -= 8;
    } else {
      remainder = prefixLength;
      prefixLength = 0;
    }
    if (i > 0)
      netmask += '.';
    var value = 0;
    if (remainder != 0)
      value = ((2 << (remainder - 1)) - 1) << (8 - remainder);
    netmask += value.toString();
  }
  return netmask;
};

/**
 * Returns the routing prefix length as a number from the netmask string.
 * @param {string} netmask The netmask string, e.g. 255.255.255.0.
 * @return {number} The corresponding netmask or -1 if invalid.
 */
CrOnc.getRoutingPrefixAsLength = function(netmask) {
  'use strict';
  var prefixLength = 0;
  var tokens = netmask.split('.');
  if (tokens.length != 4)
    return -1;
  for (var i = 0; i < tokens.length; ++i) {
    var token = tokens[i];
    // If we already found the last mask and the current one is not
    // '0' then the netmask is invalid. For example, 255.224.255.0
    if (prefixLength / 8 != i) {
      if (token != '0')
        return -1;
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
      return -1;
    }
  }
  return prefixLength;
};

/**
 * @param {!CrOnc.ProxyLocation} a
 * @param {!CrOnc.ProxyLocation} b
 * @return {boolean}
 */
CrOnc.proxyMatches = function(a, b) {
  return a.Host == b.Host && a.Port == b.Port;
};

/**
 * @param {!CrOnc.NetworkProperties|!CrOnc.NetworkStateProperties|undefined}
 *     networkProperties The ONC network properties or state properties.
 * @return {boolean}
 */
CrOnc.shouldShowTetherDialogBeforeConnection = function(networkProperties) {
  // Only show for Tether networks.
  if (networkProperties.Type != CrOnc.Type.TETHER)
    return false;

  // Show if there are no Tether properties or if there are Tether properties
  // and they indicate that a connection has not yet occurred to this host.
  return !networkProperties.Tether ||
      !networkProperties.Tether.HasConnectedToHost;
};

/**
 * Returns a valid CrOnc.Type, or undefined.
 * @param {string} typeStr
 * @return {!CrOnc.Type|undefined}
 */
CrOnc.getValidType = function(typeStr) {
  if (Object.values(CrOnc.Type).indexOf(typeStr) >= 0)
    return /** @type {!CrOnc.Type} */ (typeStr);
  return undefined;
};
