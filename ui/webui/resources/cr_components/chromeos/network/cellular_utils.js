// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
// #import {OncMojo} from 'chrome://resources/cr_components/chromeos/network/onc_mojo.m.js';
// clang-format on

/**
 * Checks if the device has a cellular network with connectionState not
 * kNotConnected.
 * @return {!Promise<boolean>}
 */
/* #export */ function hasActiveCellularNetwork() {
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
        return response.result.some(network => {
          return network.connectionState !==
              mojom.ConnectionStateType.kNotConnected;
        });
      });
}

/**
 * Returns number of phyical SIM and eSIM slots on the current device
 * @param {!chromeos.networkConfig.mojom.DeviceStateProperties|undefined}
 *     deviceState
 * @return {!{pSimSlots: number, eSimSlots: number}}
 */
/* #export */ function getSimSlotCount(deviceState) {
  let pSimSlots = 0;
  let eSimSlots = 0;

  if (!deviceState || !deviceState.simInfos) {
    return {pSimSlots, eSimSlots};
  }

  for (const simInfo of deviceState.simInfos) {
    if (simInfo.eid) {
      eSimSlots++;
      continue;
    }
    pSimSlots++;
  }

  return {pSimSlots, eSimSlots};
}

/**
 * Checks if the device is currently connected to a WiFi, Ethernet or Tether
 * network.
 * @return {!Promise<boolean>}
 */
/* #export */ function isConnectedToNonCellularNetwork() {
  const mojom = chromeos.networkConfig.mojom;
  const networkConfig = network_config.MojoInterfaceProviderImpl.getInstance()
                            .getMojoServiceRemote();
  return networkConfig
      .getNetworkStateList({
        filter: mojom.FilterType.kActive,
        networkType: mojom.NetworkType.kAll,
        limit: mojom.NO_LIMIT,
      })
      .then((response) => {
        // Filter for connected non-cellular networks.
        return response.result.some(network => {
          return network.connectionState ===
              mojom.ConnectionStateType.kOnline &&
              network.type !== mojom.NetworkType.kCellular;
        });
      });
}

/**
 * Determines if the current network is on the active sim slot.
 * @param {?chromeos.networkConfig.mojom.NetworkStateProperties} networkState
 * @param {?chromeos.networkConfig.mojom.DeviceStateProperties} deviceState
 */
/* #export */ function isActiveSim(networkState, deviceState) {
  const mojom = chromeos.networkConfig.mojom;
  if (!networkState || networkState.type !== mojom.NetworkType.kCellular) {
    return false;
  }

  const iccid = networkState.typeState.cellular.iccid;
  if (!iccid || !deviceState || !deviceState.simInfos) {
    return false;
  }
  const isActiveSim = deviceState.simInfos.find(simInfo => {
    return simInfo.iccid === iccid && simInfo.isPrimary;
  });
  return !!isActiveSim;
}
