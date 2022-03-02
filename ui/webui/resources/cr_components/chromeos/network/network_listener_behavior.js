// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {MojoInterfaceProviderImpl} from './mojo_interface_provider.m.js';

/**
 * @fileoverview Polymer behavior for observing CrosNetworkConfigObserver
 * events.
 */

/** @polymerBehavior */
/* #export */ const NetworkListenerBehavior = {
  /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigObserver} */
  observer_: null,

  /** @override */
  attached() {
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
  onActiveNetworksChanged(activeNetworks) {},

  /** @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network */
  onNetworkStateChanged(network) {},

  onNetworkStateListChanged() {},

  onDeviceStateListChanged() {},

  onVpnProvidersChanged() {},

  onNetworkCertificatesChanged() {},

  /** @param {string} userhash */
  onPoliciesApplied(userhash) {},
};

/** @interface */
/* #export */ class NetworkListenerBehaviorInterface {
  constructor() {
    /** @private {?chromeos.networkConfig.mojom.CrosNetworkConfigObserver} */
    this.observer_;
  }

  attached() {}

  /**
   * @param {!Array<chromeos.networkConfig.mojom.NetworkStateProperties>}
   *     activeNetworks
   */
  onActiveNetworksChanged(activeNetworks) {}

  /** @param {!chromeos.networkConfig.mojom.NetworkStateProperties} network */
  onNetworkStateChanged(network) {}

  onNetworkStateListChanged() {}

  onDeviceStateListChanged() {}

  onVpnProvidersChanged() {}

  onNetworkCertificatesChanged() {}

  /** @param {!string} userhash */
  onPoliciesApplied(userhash) {}
}
