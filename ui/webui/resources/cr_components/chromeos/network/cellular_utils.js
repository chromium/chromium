// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// clang-format on

/**
 * Checks if the device is currently connected to an eSIM network.
 * @return {!Promise<boolean>}
 */
/* #export */ function isConnectedToESimNetwork() {
  const mojom = chromeos.networkConfig.mojom;
  const networkConfig = network_config.MojoInterfaceProviderImpl.getInstance()
                            .getMojoServiceRemote();
  return networkConfig
      .getNetworkStateList({
        filter: mojom.FilterType.kActive,
        networkType: mojom.NetworkType.kCellular,
        limit: mojom.NO_LIMIT,
      })
      .then((response) => {
        // Filter for connected networks and check if they are eSIM.
        return Promise.all(response.result
                               .filter(network => {
                                 return network.connectionState ===
                                     mojom.ConnectionStateType.kConnected;
                               })
                               .map(networkIsESim_));
      })
      .then((networkIsESimResults) => {
        return networkIsESimResults.some((isESimNetwork) => isESimNetwork);
      });
}

/**
 * Returns whether a network is an eSIM network or not.
 * @private
 * @param {OncMojo.NetworkStateProperties} network
 * @return {!Promise<boolean>}
 */
function networkIsESim_(network) {
  const networkConfig = network_config.MojoInterfaceProviderImpl.getInstance()
                            .getMojoServiceRemote();
  return networkConfig.getManagedProperties(network.guid).then((response) => {
    return response.result.typeProperties.cellular.eid !== null;
  });
}
