// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer element for rendering network icons based on ONC
 * state properties.
 */

Polymer({
  is: 'network-icon',

  properties: {
    /**
     * If set, the ONC properties will be used to display the icon. This may
     * either be the complete set of NetworkProperties or the subset of
     * NetworkStateProperties.
     * @type {!OncMojo.NetworkStateProperties|undefined}
     */
    networkState: Object,

    /**
     * If set, the device state for the network type. Otherwise it defaults to
     * null rather than undefined so that it does not block computed bindings.
     * @type {?OncMojo.DeviceStateProperties}
     */
    deviceState: {
      type: Object,
      value: null,
    },

    /**
     * If true, the icon is part of a list of networks and may be displayed
     * differently, e.g. the disconnected image will never be shown for
     * list items.
     */
    isListItem: {
      type: Boolean,
      value: false,
    },

    /**
     * If true, cellular technology badge is displayed in the network icon.
     */
    showTechnologyBadge: {
      type: Boolean,
      value: true,
    },
  },

  /**
   * Number of network icons for different cellular or wifi network signal
   * strengths.
   * @private @const
   */
  networkIconCount_: 5,

  /**
   * @return {string} The name of the svg icon image to show.
   * @private
   */
  getIconClass_: function() {
    if (!this.networkState) {
      return '';
    }
    const mojom = chromeos.networkConfig.mojom;
    const type = this.networkState.type;
    if (type == mojom.NetworkType.kEthernet) {
      return 'ethernet';
    }
    if (type == mojom.NetworkType.kVPN) {
      return 'vpn';
    }

    const prefix = OncMojo.networkTypeIsMobile(type) ? 'cellular-' : 'wifi-';
    if (!this.isListItem && !this.networkState.guid) {
      const device = this.deviceState;
      if (!device || device.deviceState == mojom.DeviceStateType.kEnabled ||
          device.deviceState == mojom.DeviceStateType.kEnabling) {
        return prefix + 'no-network';
      }
      return prefix + 'off';
    }

    const connectionState = this.networkState.connectionState;
    if (connectionState == mojom.ConnectionStateType.kConnecting) {
      return prefix + 'connecting';
    }

    if (!this.isListItem &&
        connectionState == mojom.ConnectionStateType.kNotConnected) {
      return prefix + 'not-connected';
    }

    const strength = OncMojo.getSignalStrength(this.networkState);
    return prefix + this.strengthToIndex_(strength).toString(10);
  },

  /**
   * @param {number} strength The signal strength from [0 - 100].
   * @return {number} An index from 0 to |this.networkIconCount_ - 1|
   * corresponding to |strength|.
   * @private
   */
  strengthToIndex_: function(strength) {
    if (strength <= 0) {
      return 0;
    }

    if (strength >= 100) {
      return this.networkIconCount_ - 1;
    }

    const zeroBasedIndex =
        Math.trunc((strength - 1) * (this.networkIconCount_ - 1) / 100);
    return zeroBasedIndex + 1;
  },

  /**
   * @return {boolean}
   * @private
   */
  showTechnology_: function() {
    return this.getTechnology_() != '' && this.showTechnologyBadge;
  },

  /**
   * @return {string}
   * @private
   */
  getTechnology_: function() {
    if (!this.networkState) {
      return '';
    }
    if (this.networkState.type ==
        chromeos.networkConfig.mojom.NetworkType.kCellular) {
      const technology = this.getTechnologyId_(
          this.networkState.typeState.cellular.networkTechnology);
      if (technology != '') {
        return 'network:' + technology;
      }
    }
    return '';
  },

  /**
   * @param {string|undefined} networkTechnology
   * @return {string}
   * @private
   */
  getTechnologyId_: function(networkTechnology) {
    switch (networkTechnology) {
      case 'CDMA1XRTT':
        return 'badge-1x';
      case 'EDGE':
        return 'badge-edge';
      case 'EVDO':
        return 'badge-evdo';
      case 'GPRS':
      case 'GSM':
        return 'badge-gsm';
      case 'HSPA':
        return 'badge-hspa';
      case 'HSPAPlus':
        return 'badge-hspa-plus';
      case 'LTE':
        return 'badge-lte';
      case 'LTEAdvanced':
        return 'badge-lte-advanced';
      case 'UMTS':
        return 'badge-3g';
    }
    return '';
  },

  /**
   * @return {boolean}
   * @private
   */
  showSecure_: function() {
    if (!this.networkState) {
      return false;
    }
    const mojom = chromeos.networkConfig.mojom;
    if (!this.isListItem &&
        this.networkState.connectionState ==
            mojom.ConnectionStateType.kNotConnected) {
      return false;
    }
    return this.networkState.type == mojom.NetworkType.kWiFi &&
        this.networkState.typeState.wifi.security != mojom.SecurityType.kNone;
  },
});
