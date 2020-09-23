// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

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
   * Returns a string for the given NetworkType.
   * @private
   * @param {chromeos.networkConfig.mojom.NetworkType} type
   * @return {string}
   */
  getNetworkTypeString_(type) {
    return this.i18n('OncType' + OncMojo.getNetworkTypeString(type));
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
});
