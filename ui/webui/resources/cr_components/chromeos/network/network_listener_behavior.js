// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for observing CrosNetworkConfigObserver
 * events.
 */

/** @polymerBehavior */
const NetworkListenerBehavior = {
  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigObserver} */
  observer_: null,

  /** @override */
  attached: function() {
    this.observer_ =
        new chromeos.networkConfig.mojom.CrosNetworkConfigObserverReceiver(
            this);
    network_config.MojoInterfaceProviderImpl.getInstance()
        .getMojoServiceRemote()
        .addObserver(this.observer_.$.bindNewPipeAndPassRemote());
  },

  // CrosNetworkConfigObserver methods. Override these in the implementation.

  /**
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     activeNetworks
   */
  onActiveNetworksChanged: function(activeNetworks) {},

  /** @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network */
  onNetworkStateChanged: function(network) {},

  onNetworkStateListChanged: function() {},

  onDeviceStateListChanged: function() {},

  onVpnProvidersChanged: function() {},

  onNetworkCertificatesChanged: function() {},
};
