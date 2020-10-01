// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/services/network/public/mojom/ip_address.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/time.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-lite.js';

// #import {assert, assertNotReached} from 'chrome://resources/js/assert.m.js';
// #import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
// cland-format on

/**
 * @fileoverview Utilities supporting network_config.mojom types. The strings
 * returned in the getFooTypeString methods are used for looking up localized
 * strings and for debugging. They are not intended to be drectly user facing.
 */

/* #export */ class OncMojo {
  /**
   * @param {number|undefined} value
   * @return {string}
   */
  static getEnumString(value) {
    if (value === undefined) {
      return 'undefined';
    }
    return value.toString();
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ActivationStateType} value
   * @return {string}
   */
  static getActivationStateTypeString(value) {
    const ActivationStateType =
        chromeos.networkConfig.mojom.ActivationStateType;
    switch (value) {
      case ActivationStateType.kUnknown:
        return 'Unknown';
      case ActivationStateType.kNotActivated:
        return 'NotActivated';
      case ActivationStateType.kActivating:
        return 'Activating';
      case ActivationStateType.kPartiallyActivated:
        return 'PartiallyActivated';
      case ActivationStateType.kActivated:
        return 'Activated';
      case ActivationStateType.kNoService:
        return 'NoService';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.ActivationStateType}
   */
  static getActivationStateTypeFromString(value) {
    const ActivationStateType =
        chromeos.networkConfig.mojom.ActivationStateType;
    switch (value) {
      case 'Unknown':
        return ActivationStateType.kUnknown;
      case 'NotActivated':
        return ActivationStateType.kNotActivated;
      case 'Activating':
        return ActivationStateType.kActivating;
      case 'PartiallyActivated':
        return ActivationStateType.kPartiallyActivated;
      case 'Activated':
        return ActivationStateType.kActivated;
      case 'NoService':
        return ActivationStateType.kNoService;
    }
    assertNotReached('Unexpected value: ' + value);
    return ActivationStateType.kUnknown;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ConnectionStateType} value
   * @return {string}
   */
  static getConnectionStateTypeString(value) {
    const ConnectionStateType =
        chromeos.networkConfig.mojom.ConnectionStateType;
    switch (value) {
      case ConnectionStateType.kOnline:
        return 'Online';
      case ConnectionStateType.kConnected:
        return 'Connected';
      case ConnectionStateType.kPortal:
        return 'Portal';
      case ConnectionStateType.kConnecting:
        return 'Connecting';
      case ConnectionStateType.kNotConnected:
        return 'NotConnected';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.ConnectionStateType}
   */
  static getConnectionStateTypeFromString(value) {
    const ConnectionStateType =
        chromeos.networkConfig.mojom.ConnectionStateType;
    switch (value) {
      case 'Online':
        return ConnectionStateType.kOnline;
      case 'Connected':
        return ConnectionStateType.kConnected;
      case 'Portal':
        return ConnectionStateType.kPortal;
      case 'Connecting':
        return ConnectionStateType.kConnecting;
      case 'NotConnected':
        return ConnectionStateType.kNotConnected;
    }
    assertNotReached('Unexpected value: ' + value);
    return ConnectionStateType.kNotConnected;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ConnectionStateType} value
   * @return {boolean}
   */
  static connectionStateIsConnected(value) {
    const ConnectionStateType =
        chromeos.networkConfig.mojom.ConnectionStateType;
    switch (value) {
      case ConnectionStateType.kOnline:
      case ConnectionStateType.kConnected:
      case ConnectionStateType.kPortal:
        return true;
      case ConnectionStateType.kConnecting:
      case ConnectionStateType.kNotConnected:
        return false;
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return false;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.DeviceStateType} value
   * @return {string}
   */
  static getDeviceStateTypeString(value) {
    const DeviceStateType = chromeos.networkConfig.mojom.DeviceStateType;
    switch (value) {
      case DeviceStateType.kUninitialized:
        return 'Uninitialized';
      case DeviceStateType.kDisabled:
        return 'Disabled';
      case DeviceStateType.kDisabling:
        return 'Disabling';
      case DeviceStateType.kEnabling:
        return 'Enabling';
      case DeviceStateType.kEnabled:
        return 'Enabled';
      case DeviceStateType.kProhibited:
        return 'Prohibited';
      case DeviceStateType.kUnavailable:
        return 'Unavailable';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {!chromeos.networkConfig.mojom.DeviceStateType} value
   * @return {boolean}
   */
  static deviceStateIsIntermediate(value) {
    const DeviceStateType = chromeos.networkConfig.mojom.DeviceStateType;
    switch (value) {
      case DeviceStateType.kUninitialized:
      case DeviceStateType.kDisabling:
      case DeviceStateType.kEnabling:
      case DeviceStateType.kUnavailable:
        return true;
      case DeviceStateType.kDisabled:
      case DeviceStateType.kEnabled:
      case DeviceStateType.kProhibited:
        return false;
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return false;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} value
   * @return {string}
   */
  static getNetworkTypeString(value) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    switch (value) {
      case NetworkType.kAll:
        return 'All';
      case NetworkType.kCellular:
        return 'Cellular';
      case NetworkType.kEthernet:
        return 'Ethernet';
      case NetworkType.kMobile:
        return 'Mobile';
      case NetworkType.kTether:
        return 'Tether';
      case NetworkType.kVPN:
        return 'VPN';
      case NetworkType.kWireless:
        return 'Wireless';
      case NetworkType.kWiFi:
        return 'WiFi';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} value
   * @return {boolean}
   */
  static networkTypeIsMobile(value) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    switch (value) {
      case NetworkType.kCellular:
      case NetworkType.kMobile:
      case NetworkType.kTether:
        return true;
      case NetworkType.kAll:
      case NetworkType.kEthernet:
      case NetworkType.kVPN:
      case NetworkType.kWireless:
      case NetworkType.kWiFi:
        return false;
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return false;
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.NetworkType}
   */
  static getNetworkTypeFromString(value) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    switch (value) {
      case 'All':
        return NetworkType.kAll;
      case 'Cellular':
        return NetworkType.kCellular;
      case 'Ethernet':
        return NetworkType.kEthernet;
      case 'Mobile':
        return NetworkType.kMobile;
      case 'Tether':
        return NetworkType.kTether;
      case 'VPN':
        return NetworkType.kVPN;
      case 'Wireless':
        return NetworkType.kWireless;
      case 'WiFi':
        return NetworkType.kWiFi;
    }
    assertNotReached('Unexpected value: ' + value);
    return NetworkType.kAll;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.OncSource} value
   * @return {string}
   */
  static getOncSourceString(value) {
    const OncSource = chromeos.networkConfig.mojom.OncSource;
    switch (value) {
      case OncSource.kNone:
        return 'None';
      case OncSource.kDevice:
        return 'Device';
      case OncSource.kDevicePolicy:
        return 'DevicePolicy';
      case OncSource.kUser:
        return 'User';
      case OncSource.kUserPolicy:
        return 'UserPolicy';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {!chromeos.networkConfig.mojom.SecurityType} value
   * @return {string}
   */
  static getSecurityTypeString(value) {
    const SecurityType = chromeos.networkConfig.mojom.SecurityType;
    switch (value) {
      case SecurityType.kNone:
        return 'None';
      case SecurityType.kWep8021x:
        return 'WEP-8021X';
      case SecurityType.kWepPsk:
        return 'WEP-PSK';
      case SecurityType.kWpaEap:
        return 'WPA-EAP';
      case SecurityType.kWpaPsk:
        return 'WPA-PSK';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.SecurityType}
   */
  static getSecurityTypeFromString(value) {
    const SecurityType = chromeos.networkConfig.mojom.SecurityType;
    switch (value) {
      case 'None':
        return SecurityType.kNone;
      case 'WEP-8021X':
        return SecurityType.kWep8021x;
      case 'WEP-PSK':
        return SecurityType.kWepPsk;
      case 'WPA-EAP':
        return SecurityType.kWpaEap;
      case 'WPA-PSK':
        return SecurityType.kWpaPsk;
    }
    assertNotReached('Unexpected value: ' + value);
    return SecurityType.kNone;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.VpnType} value
   * @return {string}
   */
  static getVpnTypeString(value) {
    const VpnType = chromeos.networkConfig.mojom.VpnType;
    switch (value) {
      case VpnType.kL2TPIPsec:
        return 'L2TP-IPsec';
      case VpnType.kOpenVPN:
        return 'OpenVPN';
      case VpnType.kExtension:
        return 'ThirdPartyVPN';
      case VpnType.kArc:
        return 'ARCVPN';
    }
    assertNotReached('Unexpected enum value: ' + OncMojo.getEnumString(value));
    return '';
  }

  /**
   * @param {string} value
   * @return {!chromeos.networkConfig.mojom.VpnType}
   */
  static getVpnTypeFromString(value) {
    const VpnType = chromeos.networkConfig.mojom.VpnType;
    switch (value) {
      case 'L2TP-IPsec':
        return VpnType.kL2TPIPsec;
      case 'OpenVPN':
        return VpnType.kOpenVPN;
      case 'ThirdPartyVPN':
        return VpnType.kExtension;
      case 'ARCVPN':
        return VpnType.kArc;
    }
    assertNotReached('Unexpected value: ' + value);
    return VpnType.kOpenVPN;
  }

  /**
   * This infers the type from |key|, casts |value| (which should be a number)
   * to the corresponding enum type, and converts it to a string. If |key| is
   * known, then |value| is expected to match an enum value. Otherwise |value|
   * is simply returned.
   * @param {string} key
   * @param {number|string} value
   * @return {number|string}
   */
  static getTypeString(key, value) {
    if (key === 'activationState') {
      return OncMojo.getActivationStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.ActivationStateType} */ (
              value));
    }
    if (key === 'connectionState') {
      return OncMojo.getConnectionStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.ConnectionStateType} */ (
              value));
    }
    if (key === 'deviceState') {
      return OncMojo.getDeviceStateTypeString(
          /** @type {!chromeos.networkConfig.mojom.DeviceStateType} */ (value));
    }
    if (key === 'type') {
      return OncMojo.getNetworkTypeString(
          /** @type {!chromeos.networkConfig.mojom.NetworkType} */ (value));
    }
    if (key === 'source') {
      return OncMojo.getOncSourceString(
          /** @type {!chromeos.networkConfig.mojom.OncSource} */ (value));
    }
    if (key === 'security') {
      return OncMojo.getSecurityTypeString(
          /** @type {!chromeos.networkConfig.mojom.SecurityType} */ (value));
    }
    return value;
  }

  /**
   * Policy indicators expect a per-property PolicySource, but sometimes we need
   * to use the per-configuration OncSource (e.g. for unmanaged intrinsic
   * properties like Security). This returns the corresponding PolicySource.
   * @param {!chromeos.networkConfig.mojom.OncSource} source
   * @return {!chromeos.networkConfig.mojom.PolicySource}
   */
  static getEnforcedPolicySourceFromOncSource(source) {
    const OncSource = chromeos.networkConfig.mojom.OncSource;
    const PolicySource = chromeos.networkConfig.mojom.PolicySource;
    switch (source) {
      case OncSource.kNone:
      case OncSource.kDevice:
      case OncSource.kUser:
        return PolicySource.kNone;
      case OncSource.kDevicePolicy:
        return PolicySource.kDevicePolicyEnforced;
      case OncSource.kUserPolicy:
        return PolicySource.kUserPolicyEnforced;
    }
    assert(source !== undefined, 'OncSource undefined');
    assertNotReached('Invalid OncSource: ' + source.toString());
    return PolicySource.kNone;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @return {string}
   */
  static getNetworkTypeDisplayName(type) {
    return loadTimeData.getStringF(
        'OncType' + OncMojo.getNetworkTypeString(type));
  }

  /**
   * @param {string} networkName
   * @param {string|undefined} providerName
   * @return {string}
   */
  static getVpnDisplayName(networkName, providerName) {
    if (providerName) {
      return loadTimeData.getStringF(
          'vpnNameTemplate', providerName, networkName);
    }
    return networkName;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network
   * @return {string}
   */
  static getNetworkStateDisplayName(network) {
    if (!network.name) {
      return OncMojo.getNetworkTypeDisplayName(network.type);
    }
    const mojom = chromeos.networkConfig.mojom;
    if (network.type === mojom.NetworkType.kVPN &&
        network.typeState.vpn.providerName) {
      return OncMojo.getVpnDisplayName(
          network.name, network.typeState.vpn.providerName);
    }
    return network.name;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} network
   * @return {string}
   */
  static getNetworkName(network) {
    if (!network.name || !network.name.activeValue) {
      return OncMojo.getNetworkTypeDisplayName(network.type);
    }
    const mojom = chromeos.networkConfig.mojom;
    if (network.type === mojom.NetworkType.kVPN &&
        network.typeProperties.vpn.providerName) {
      return OncMojo.getVpnDisplayName(
          network.name.activeValue, network.typeProperties.vpn.providerName);
    }
    return network.name.activeValue;
  }

  /**
   * Gets the SignalStrength value from |network| based on network.type.
   * @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network
   * @return {number} The signal strength value if it exists or 0.
   */
  static getSignalStrength(network) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    switch (network.type) {
      case NetworkType.kCellular:
        return network.typeState.cellular.signalStrength;
      case NetworkType.kTether:
        return network.typeState.tether.signalStrength;
      case NetworkType.kWiFi:
        return network.typeState.wifi.signalStrength;
    }
    assertNotReached();
    return 0;
  }

  /**
   * @param {string} key
   * @return {boolean}
   */
  static isTypeKey(key) {
    return key.startsWith('cellular') || key.startsWith('ethernet') ||
        key.startsWith('tether') || key.startsWith('vpn') ||
        key.startsWith('wifi');
  }

  /**
   * This is a bit of a hack. To avoid adding 'typeProperties' to every type
   * specific field name and translated string, we check for type specific
   * key names and prepend 'typeProperties' for them.
   * @param {string} key
   * @return {string}
   */
  static getManagedPropertyKey(key) {
    if (OncMojo.isTypeKey(key)) {
      key = 'typeProperties.' + key;
    }
    return key;
  }

  /**
   * Returns a NetworkStateProperties object with type set and default values.
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @param {?string=} opt_name Optional name, intended for testing.
   * @return {!chromeos.networkConfig.mojom.NetworkStateProperties}
   */
  static getDefaultNetworkState(type, opt_name) {
    const mojom = chromeos.networkConfig.mojom;
    const result = {
      connectable: false,
      connectRequested: false,
      connectionState: mojom.ConnectionStateType.kNotConnected,
      guid: opt_name ? (opt_name + '_guid') : '',
      name: opt_name || '',
      priority: 0,
      proxyMode: mojom.ProxyMode.kDirect,
      prohibitedByPolicy: false,
      source: mojom.OncSource.kNone,
      type: type,
      typeState: {},
    };
    switch (type) {
      case mojom.NetworkType.kCellular:
        result.typeState.cellular = {
          activationState: mojom.ActivationStateType.kUnknown,
          networkTechnology: '',
          roaming: false,
          signalStrength: 0,
          simLocked: false,
        };
        break;
      case mojom.NetworkType.kEthernet:
        result.typeState.ethernet = {
          authentication: mojom.AuthenticationType.kNone,
        };
        break;
      case mojom.NetworkType.kTether:
        result.typeState.tether = {
          batteryPercentage: 0,
          carrier: '',
          hasConnectedToHost: false,
          signalStrength: 0,
        };
        break;
      case mojom.NetworkType.kVPN:
        result.typeState.vpn = {
          type: mojom.VpnType.kOpenVPN,
          providerId: '',
          providerName: '',
        };
        break;
      case mojom.NetworkType.kWiFi:
        result.typeState.wifi = {
          bssid: '',
          frequency: 0,
          hexSsid: opt_name || '',
          security: mojom.SecurityType.kNone,
          signalStrength: 0,
          ssid: '',
        };
        break;
      default:
        assertNotReached();
    }
    return result;
  }

  /**
   * Converts an ManagedProperties dictionary to NetworkStateProperties.
   * Used to provide state properties to NetworkIcon.
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} properties
   * @return {!chromeos.networkConfig.mojom.NetworkStateProperties}
   */
  static managedPropertiesToNetworkState(properties) {
    const mojom = chromeos.networkConfig.mojom;
    const networkState = OncMojo.getDefaultNetworkState(properties.type);
    networkState.connectable = properties.connectable;
    networkState.connectionState = properties.connectionState;
    networkState.guid = properties.guid;
    if (properties.name) {
      networkState.name = properties.name.activeValue;
    }
    if (properties.priority) {
      networkState.priority = properties.priority.activeValue;
    }
    networkState.source = properties.source;

    switch (properties.type) {
      case mojom.NetworkType.kCellular:
        const cellularProperties = properties.typeProperties.cellular;
        networkState.typeState.cellular.activationState =
            cellularProperties.activationState;
        networkState.typeState.cellular.networkTechnology =
            cellularProperties.networkTechnology || '';
        networkState.typeState.cellular.roaming =
            cellularProperties.roamingState === 'Roaming';
        networkState.typeState.cellular.signalStrength =
            cellularProperties.signalStrength;
        break;
      case mojom.NetworkType.kEthernet:
        networkState.typeState.ethernet.authentication =
            OncMojo.getActiveValue(
                properties.typeProperties.ethernet.authentication) === '8021X' ?
            mojom.AuthenticationType.k8021x :
            mojom.AuthenticationType.kNone;
        break;
      case mojom.NetworkType.kTether:
        if (properties.typeProperties.tether) {
          networkState.typeState.tether =
              /** @type {!mojom.TetherStateProperties}*/ (
                  Object.assign({}, properties.typeProperties.tether));
        }
        break;
      case mojom.NetworkType.kVPN:
        networkState.typeState.vpn.providerName =
            properties.typeProperties.vpn.providerName;
        networkState.typeState.vpn.type = properties.typeProperties.vpn.type;
        break;
      case mojom.NetworkType.kWiFi:
        const wifiProperties = properties.typeProperties.wifi;
        networkState.typeState.wifi.bssid = wifiProperties.bssid || '';
        networkState.typeState.wifi.frequency = wifiProperties.frequency;
        networkState.typeState.wifi.hexSsid =
            OncMojo.getActiveString(wifiProperties.hexSsid);
        networkState.typeState.wifi.security = wifiProperties.security;
        networkState.typeState.wifi.signalStrength =
            wifiProperties.signalStrength;
        networkState.typeState.wifi.ssid =
            OncMojo.getActiveString(wifiProperties.ssid);
        break;
    }
    return networkState;
  }

  /**
   * Returns a ManagedProperties object with type, guid and name set, and all
   * other required properties set to their default values.
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @param {string} guid
   * @param {string} name
   * @return {!chromeos.networkConfig.mojom.ManagedProperties}
   */
  static getDefaultManagedProperties(type, guid, name) {
    const mojom = chromeos.networkConfig.mojom;
    const result = {
      connectionState: mojom.ConnectionStateType.kNotConnected,
      source: mojom.OncSource.kNone,
      type: type,
      connectable: false,
      guid: guid,
      name: OncMojo.createManagedString(name),
      restrictedConnectivity: false,
    };
    switch (type) {
      case mojom.NetworkType.kCellular:
        result.typeProperties = {
          cellular: {
            activationState: mojom.ActivationStateType.kUnknown,
            allowRoaming: false,
            signalStrength: 0,
            supportNetworkScan: false,
          }
        };
        break;
      case mojom.NetworkType.kEthernet:
        result.typeProperties = {
          ethernet: {},
        };
        break;
      case mojom.NetworkType.kTether:
        result.typeProperties = {
          tether: {
            batteryPercentage: 0,
            carrier: '',
            hasConnectedToHost: false,
            signalStrength: 0,
          }
        };
        break;
      case mojom.NetworkType.kVPN:
        result.typeProperties = {
          vpn: {
            providerName: '',
            type: mojom.VpnType.kOpenVPN,
            openVpn: {},
          }
        };
        break;
      case mojom.NetworkType.kWiFi:
        result.typeProperties = {
          wifi: {
            bssid: '',
            frequency: 0,
            ssid: OncMojo.createManagedString(''),
            security: mojom.SecurityType.kNone,
            signalStrength: 0,
            isSyncable: false,
            isConfiguredByActiveUser: false,
          }
        };
        break;
    }
    return result;
  }

  /**
   * Returns a ConfigProperties object with a default networkType struct
   * based on |type|.
   * @param {!chromeos.networkConfig.mojom.NetworkType} type
   * @return {!chromeos.networkConfig.mojom.ConfigProperties}
   */
  static getDefaultConfigProperties(type) {
    const mojom = chromeos.networkConfig.mojom;
    switch (type) {
      case mojom.NetworkType.kCellular:
        return {typeConfig: {cellular: {}}};
        break;
      case mojom.NetworkType.kEthernet:
        return {typeConfig: {ethernet: {}}};
        break;
      case mojom.NetworkType.kVPN:
        return {typeConfig: {vpn: {}}};
        break;
      case mojom.NetworkType.kWiFi:
        // Note: wifi.security can not be changed, so |security| will be ignored
        // for existing configurations.
        return {typeConfig: {wifi: {security: mojom.SecurityType.kNone}}};
        break;
    }
    assertNotReached('Unexpected type: ' + type.toString());
    return {typeConfig: {}};
  }

  /**
   * Sets the value of a property in an mojo config dictionary.
   * @param {!chromeos.networkConfig.mojom.ConfigProperties} config
   * @param {string} key The property key which may be nested, e.g. 'foo.bar'
   * @param {boolean|number|string|!Object} value The property value
   */
  static setConfigProperty(config, key, value) {
    if (OncMojo.isTypeKey(key)) {
      key = 'typeConfig.' + key;
    }
    while (true) {
      const index = key.indexOf('.');
      if (index < 0) {
        break;
      }
      const keyComponent = key.substr(0, index);
      if (!config.hasOwnProperty(keyComponent)) {
        config[keyComponent] = {};
      }
      config = config[keyComponent];
      key = key.substr(index + 1);
    }
    config[key] = value;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedBoolean|
   *         !chromeos.networkConfig.mojom.ManagedInt32|
   *         !chromeos.networkConfig.mojom.ManagedString|
   *         !chromeos.networkConfig.mojom.ManagedStringList|
   *         !chromeos.networkConfig.mojom.ManagedApnList|
   *         null|undefined} property
   * @return {boolean|number|string|!Array<string>|
   *          !Array<!chromeos.networkConfig.mojom.ApnProperties>|undefined}
   */
  static getActiveValue(property) {
    if (!property) {
      return undefined;
    }
    return property.activeValue;
  }

  /**
   * @param {?chromeos.networkConfig.mojom.ManagedString|undefined} property
   * @return {string}
   */
  static getActiveString(property) {
    if (!property) {
      return '';
    }
    return property.activeValue;
  }

  /**
   * Returns IPConfigProperties for |type|. For IPv4, these will be the static
   * properties if IPAddressConfigType is Static and StaticIPConfig is set.
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} properties
   * @param {string} desiredType Desired ip config type (IPv4 or IPv6).
   * @return {!chromeos.networkConfig.mojom.IPConfigProperties|undefined}
   */
  static getIPConfigForType(properties, desiredType) {
    const mojom = chromeos.networkConfig.mojom;
    const ipConfigs = properties.ipConfigs;
    let ipConfig;
    if (ipConfigs) {
      ipConfig = ipConfigs.find(ipconfig => ipconfig.type === desiredType);
      if (ipConfig && desiredType !== 'IPv4') {
        return ipConfig;
      }
    }

    // Only populate static ip config properties for IPv4.
    if (desiredType !== 'IPv4') {
      return undefined;
    }

    if (!ipConfig) {
      ipConfig = /** @type {!mojom.IPConfigProperties} */ ({routingPrefix: 0});
    }

    const staticIpConfig = properties.staticIpConfig;
    if (!staticIpConfig) {
      return ipConfig;
    }

    // Merge the appropriate static values into the result.
    if (properties.ipAddressConfigType &&
        properties.ipAddressConfigType.activeValue === 'Static') {
      if (staticIpConfig.gateway) {
        ipConfig.gateway = staticIpConfig.gateway.activeValue;
      }
      if (staticIpConfig.ipAddress) {
        ipConfig.ipAddress = staticIpConfig.ipAddress.activeValue;
      }
      if (staticIpConfig.routingPrefix) {
        ipConfig.routingPrefix = staticIpConfig.routingPrefix.activeValue;
      }
      if (staticIpConfig.type) {
        ipConfig.type = staticIpConfig.type.activeValue;
      }
    }
    if (properties.nameServersConfigType &&
        properties.nameServersConfigType.activeValue === 'Static') {
      if (staticIpConfig.nameServers) {
        ipConfig.nameServers = staticIpConfig.nameServers.activeValue;
      }
    }
    return ipConfig;
  }

  /**
   * Compares two IP config property dictionaries. Returns true if all
   * properties specified in the new dictionary match the values in the existing
   * dictionary.
   * @param {!chromeos.networkConfig.mojom.IPConfigProperties} staticValue
   * @param {!chromeos.networkConfig.mojom.IPConfigProperties} newValue
   * @return {boolean} True if all properties set in |newValue| are equal to
   *     the corresponding properties in |staticValue|.
   */
  static ipConfigPropertiesMatch(staticValue, newValue) {
    if (staticValue.type !== newValue.type) {
      return false;
    }
    if (newValue.gateway !== undefined &&
        (staticValue.gateway !== newValue.gateway)) {
      return false;
    }
    if (newValue.ipAddress !== undefined &&
        staticValue.ipAddress !== newValue.ipAddress) {
      return false;
    }
    if (staticValue.routingPrefix !== newValue.routingPrefix) {
      return false;
    }
    return true;
  }

  /**
   * Extracts existing ip config properties from |managedProperties| and applies
   * |newValue| to |field|. Returns a mojom.ConfigProperties object with the
   * IP Config related properties set, or null if no changes were applied.
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} managedProperties
   * @param {string} field
   * @param {string|!Array<string>|
   *     !chromeos.networkConfig.mojom.IPConfigProperties} newValue
   * @return {?chromeos.networkConfig.mojom.ConfigProperties}
   */
  static getUpdatedIPConfigProperties(managedProperties, field, newValue) {
    const mojom = chromeos.networkConfig.mojom;
    // Get an empty ONC dictionary and set just the IP Config properties that
    // need to change.
    let ipConfigType =
        OncMojo.getActiveString(managedProperties.ipAddressConfigType) ||
        'DHCP';
    let nsConfigType =
        OncMojo.getActiveString(managedProperties.nameServersConfigType) ||
        'DHCP';
    let staticIpConfig = OncMojo.getIPConfigForType(managedProperties, 'IPv4');
    let nameServers = staticIpConfig ? staticIpConfig.nameServers : undefined;
    if (field === 'ipAddressConfigType') {
      const newIpConfigType = /** @type {string} */ (newValue);
      if (newIpConfigType === ipConfigType) {
        return null;
      }
      ipConfigType = newIpConfigType;
    } else if (field === 'nameServersConfigType') {
      const newNsConfigType = /** @type {string} */ (newValue);
      if (newNsConfigType === nsConfigType) {
        return null;
      }
      nsConfigType = newNsConfigType;
    } else if (field === 'staticIpConfig') {
      const ipConfigValue =
          /** @type {!mojom.IPConfigProperties} */ (newValue);
      if (!ipConfigValue.type || !ipConfigValue.ipAddress) {
        console.error('Invalid StaticIPConfig: ' + JSON.stringify(newValue));
        return null;
      }
      if (ipConfigType === 'Static' && staticIpConfig &&
          OncMojo.ipConfigPropertiesMatch(staticIpConfig, ipConfigValue)) {
        return null;
      }
      ipConfigType = 'Static';
      staticIpConfig = ipConfigValue;
    } else if (field === 'nameServers') {
      const newNameServers = /** @type {!Array<string>} */ (newValue);
      if (!newNameServers || !newNameServers.length) {
        console.error('Invalid NameServers: ' + JSON.stringify(newValue));
      }
      if (nsConfigType === 'Static' &&
          JSON.stringify(nameServers) === JSON.stringify(newNameServers)) {
        return null;
      }
      nsConfigType = 'Static';
      nameServers = newNameServers;
    } else {
      console.error('Unexpected field: ' + field);
      return null;
    }

    // Set ONC IP config properties to existing values + new values.
    const config = OncMojo.getDefaultConfigProperties(managedProperties.type);
    config.ipAddressConfigType = ipConfigType;
    config.nameServersConfigType = nsConfigType;
    if (ipConfigType === 'Static') {
      assert(staticIpConfig && staticIpConfig.type && staticIpConfig.ipAddress);
      config.staticIpConfig = staticIpConfig;
    }
    if (nsConfigType === 'Static') {
      assert(nameServers && nameServers.length);
      config.staticIpConfig = config.staticIpConfig ||
          /** @type{!mojom.IPConfigProperties}*/ ({routingPrefix: 0});
      config.staticIpConfig.nameServers = nameServers;
    }
    return config;
  }

  /**
   * @param {!chromeos.networkConfig.mojom.ManagedProperties} properties
   * @return {chromeos.networkConfig.mojom.ManagedBoolean|undefined}
   */
  static getManagedAutoConnect(properties) {
    const NetworkType = chromeos.networkConfig.mojom.NetworkType;
    const type = properties.type;
    switch (type) {
      case NetworkType.kCellular:
        return properties.typeProperties.cellular.autoConnect;
      case NetworkType.kVPN:
        return properties.typeProperties.vpn.autoConnect;
      case NetworkType.kWiFi:
        return properties.typeProperties.wifi.autoConnect;
    }
    return undefined;
  }

  /**
   * @param {string} s
   * @return {!chromeos.networkConfig.mojom.ManagedString}
   */
  static createManagedString(s) {
    return {
      activeValue: s,
      policySource: chromeos.networkConfig.mojom.PolicySource.kNone,
      policyValue: undefined
    };
  }

  /**
   * @param {number} n
   * @return {!chromeos.networkConfig.mojom.ManagedInt32}
   */
  static createManagedInt(n) {
    return {
      activeValue: n,
      policySource: chromeos.networkConfig.mojom.PolicySource.kNone,
      policyValue: 0
    };
  }

  /**
   * @param {boolean} b
   * @return {!chromeos.networkConfig.mojom.ManagedBoolean}
   */
  static createManagedBool(b) {
    return {
      activeValue: b,
      policySource: chromeos.networkConfig.mojom.PolicySource.kNone,
      policyValue: false
    };
  }

  /**
   * Returns a string to translate for the user visible connection state.
   * @param {!chromeos.networkConfig.mojom.ConnectionStateType}
   *     connectionState
   * @return {string}
   */
  static getConnectionStateString(connectionState) {
    const mojom = chromeos.networkConfig.mojom;
    switch (connectionState) {
      case mojom.ConnectionStateType.kOnline:
      case mojom.ConnectionStateType.kConnected:
      case mojom.ConnectionStateType.kPortal:
        return 'OncConnected';
      case mojom.ConnectionStateType.kConnecting:
        return 'OncConnecting';
      case mojom.ConnectionStateType.kNotConnected:
        return 'OncNotConnected';
    }
    assertNotReached();
    return 'OncNotConnected';
  }

  /**
   * Returns true the IPAddress bytes match.
   * @param {?network.mojom.IPAddress|undefined} a
   * @param {?network.mojom.IPAddress|undefined} b
   * @return {boolean}
   */
  static ipAddressMatch(a, b) {
    if (!a || !b) {
      return !!a === !!b;
    }
    const abytes = a.addressBytes;
    const bbytes = b.addressBytes;
    if (abytes.length !== bbytes.length) {
      return false;
    }
    for (let i = 0; i < abytes.length; ++i) {
      if (abytes[i] !== bbytes[i]) {
        return false;
      }
    }
    return true;
  }

  /**
   * Returns true the SIMLockStatus properties match.
   * @param {?chromeos.networkConfig.mojom.SIMLockStatus|undefined} a
   * @param {?chromeos.networkConfig.mojom.SIMLockStatus|undefined} b
   * @return {boolean}
   */
  static simLockStatusMatch(a, b) {
    if (!a || !b) {
      return !!a === !!b;
    }
    return a.lockType === b.lockType && a.lockEnabled === b.lockEnabled &&
        a.retriesLeft === b.retriesLeft;
  }

  /**
   * Returns true if the APN properties match.
   * @param {chromeos.networkConfig.mojom.ApnProperties} a
   * @param {chromeos.networkConfig.mojom.ApnProperties} b
   * @return {boolean}
   */
  static apnMatch(a, b) {
    if (!a || !b) {
      return !!a === !!b;
    }
    return a.accessPointName === b.accessPointName &&
           a.name === b.name && a.username === b.username &&
           a.password === b.password;
  }

  /**
   * Returns true if the APN List matches.
   * @param {Array<!chromeos.networkConfig.mojom.ApnProperties>|undefined} a
   * @param {Array<!chromeos.networkConfig.mojom.ApnProperties>|undefined} b
   * @return {boolean}
   */
  static apnListMatch(a, b) {
    if (!a || !b) {
      return !!a === !!b;
    }
    if (a.length !== b.length) {
      return false;
    }
    return a.every((apn, index) => OncMojo.apnMatch(apn, b[index]));
  }
}

/** @typedef {chromeos.networkConfig.mojom.DeviceStateProperties} */
OncMojo.DeviceStateProperties;

/** @typedef {chromeos.networkConfig.mojom.NetworkStateProperties} */
OncMojo.NetworkStateProperties;

/**
 * @typedef {chromeos.networkConfig.mojom.ManagedBoolean|
 *           chromeos.networkConfig.mojom.ManagedInt32|
 *           chromeos.networkConfig.mojom.ManagedString|
 *           chromeos.networkConfig.mojom.ManagedStringList|
 *           chromeos.networkConfig.mojom.ManagedApnList}
 */
OncMojo.ManagedProperty;

/**
 * Modified version of mojom.IPConfigProperties to store routingPrefix as a
 * human-readable string instead of as a number. Used in network_ip_config.js.
 * @typedef {{
 *   gateway: (string|undefined),
 *   ipAddress: (string|undefined),
 *   nameServers: (!Array<string>|undefined),
 *   routingPrefix: (string|undefined),
 *   type: (string|undefined),
 *   webProxyAutoDiscoveryUrl: (string|undefined),
 * }}
 */
OncMojo.IPConfigUIProperties;
