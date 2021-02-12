// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const TechnologyIcons = {
  CELLULAR: 'cellular_0.svg',
  ETHERNET: 'ethernet.svg',
  VPN: 'vpn.svg',
  WIFI: 'wifi_0.svg',
};

/**
 * @fileoverview Polymer element for displaying NetworkHealth properties.
 */
Polymer({
  is: 'network-health-summary',

  behaviors: [
    I18nBehavior,
  ],

  /**
   * Network Health State object.
   * @private
   * @type {chromeos.networkHealth.mojom.NetworkHealthState}
   */
  networkHealthState_: null,

  /**
   * Network Health mojo remote.
   * @private
   * @type {?chromeos.networkHealth.mojom.NetworkHealthServiceRemote}
   */
  networkHealth_: null,

  /** @override */
  created() {
    this.networkHealth_ =
        chromeos.networkHealth.mojom.NetworkHealthService.getRemote();
  },

  /** @override */
  attached() {
    this.requestNetworkHealth_();

    // Automatically refresh Network Health every second.
    window.setInterval(() => {
      this.requestNetworkHealth_();
    }, 1000);
  },

  /**
   * Requests the NetworkHealthState and updates the page.
   * @private
   */
  requestNetworkHealth_() {
    this.networkHealth_.getHealthSnapshot().then(result => {
      this.networkHealthState_ = result.state;
    });
  },

  /**
   * Returns a string for the given NetworkState.
   * @private
   * @param {chromeos.networkHealth.mojom.NetworkState} state
   * @return {string}
   */
  getNetworkStateString_(state) {
    switch (state) {
      case chromeos.networkHealth.mojom.NetworkState.kUninitialized:
        return this.i18n('NetworkHealthStateUninitialized');
      case chromeos.networkHealth.mojom.NetworkState.kDisabled:
        return this.i18n('NetworkHealthStateDisabled');
      case chromeos.networkHealth.mojom.NetworkState.kProhibited:
        return this.i18n('NetworkHealthStateProhibited');
      case chromeos.networkHealth.mojom.NetworkState.kNotConnected:
        return this.i18n('NetworkHealthStateNotConnected');
      case chromeos.networkHealth.mojom.NetworkState.kConnecting:
        return this.i18n('NetworkHealthStateConnecting');
      case chromeos.networkHealth.mojom.NetworkState.kPortal:
        return this.i18n('NetworkHealthStatePortal');
      case chromeos.networkHealth.mojom.NetworkState.kConnected:
        return this.i18n('NetworkHealthStateConnected');
      case chromeos.networkHealth.mojom.NetworkState.kOnline:
        return this.i18n('NetworkHealthStateOnline');
    }

    assertNotReached('Unexpected enum value');
    return '';
  },

  /**
   * Returns a boolean flag to show the PortalState attribute. The information
   * is not meaningful in all cases and should be hidden to prevent confusion.
   * @private
   * @param {chromeos.networkHealth.mojom.Network} network
   * @return {boolean}
   */
  showPortalState_(network) {
    const NetworkState = chromeos.networkHealth.mojom.NetworkState;
    const PortalState = chromeos.networkConfig.mojom.PortalState;

    if (network.state === NetworkState.kOnline &&
        network.portalState === PortalState.kOnline) {
      return false;
    }

    const notApplicableStates = [
      NetworkState.kUninitialized,
      NetworkState.kDisabled,
      NetworkState.kProhibited,
      NetworkState.kConnecting,
      NetworkState.kNotConnected,
    ];
    if (notApplicableStates.includes(network.state)) {
      return false;
    }

    return true;
  },

  /**
   * Returns a string for the given PortalState.
   * @private
   * @param {chromeos.networkConfig.mojom.PortalState} state
   * @return {string}
   */
  getPortalStateString_(state) {
    return this.i18n('OncPortalState' + OncMojo.getPortalStateString(state));
  },

  /**
   * Returns a string for the given NetworkType.
   * @private
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @return {string}
   */
  getNetworkTypeString_(type) {
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
  },

  /**
   * Returns a icon for the given NetworkType.
   * @private
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @return {string}
   */
  getNetworkTypeIcon_(type) {
    switch (type) {
      case chromeos.networkConfig.mojom.NetworkType.kEthernet:
        return TechnologyIcons.ETHERNET;
      case chromeos.networkConfig.mojom.NetworkType.kWiFi:
        return TechnologyIcons.WIFI;
      case chromeos.networkConfig.mojom.NetworkType.kVPN:
        return TechnologyIcons.VPN;
      case chromeos.networkConfig.mojom.NetworkType.kTether:
      case chromeos.networkConfig.mojom.NetworkType.kMobile:
      case chromeos.networkConfig.mojom.NetworkType.kCellular:
        return TechnologyIcons.CELLULAR;
      default:
        return '';
    }
  },

  /**
   * Returns a string for the given signal strength.
   * @private
   * @param {?chromeos.networkHealth.mojom.UInt32Value} signalStrength
   * @return {string}
   */
  getSignalStrengthString_(signalStrength) {
    return signalStrength ? signalStrength.value.toString() : '';
  },

  /**
   * Returns a boolean flag if the open to settings link should be shown.
   * @private
   * @param {chromeos.networkHealth.mojom.Network} network
   * @return {boolean}
   */
  showSettingsLink_(network) {
    const validStates = [
      chromeos.networkHealth.mojom.NetworkState.kConnected,
      chromeos.networkHealth.mojom.NetworkState.kConnecting,
      chromeos.networkHealth.mojom.NetworkState.kPortal,
      chromeos.networkHealth.mojom.NetworkState.kOnline
    ];
    return validStates.includes(network.state);
  },

  /**
   * Returns a URL for the network's settings page.
   * @private
   * @param {chromeos.networkHealth.mojom.Network} network
   * @return {string}
   */
  getNetworkUrl_(network) {
    return 'chrome://os-settings/networkDetail?guid=' + network.guid;
  },

  /**
   * Returns a concatenated list of strings.
   * @private
   * @param {!Array<string>} addresses
   * @return {string}
   */
  joinAddresses_(addresses) {
    return addresses.join(', ');
  },
});
