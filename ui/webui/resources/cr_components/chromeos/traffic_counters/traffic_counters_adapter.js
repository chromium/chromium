// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Class that provides the functionality for interacting with traffic counters.
 */

import {MojoInterfaceProviderImpl} from 'chrome://resources/cr_components/chromeos/network/mojo_interface_provider.m.js';
import {CrosNetworkConfig} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/cros_network_config.mojom-webui.js';
import {NetworkType} from 'chrome://resources/mojo/chromeos/services/network_config/public/mojom/network_types.mojom-webui.js';

/**
 * Information about a network.
 * @typedef {{
 *   guid: string,
 *   name: string,
 *   type: !chromeos.networkConfig.mojom.NetworkType,
 *   counters: !Array<!Object>,
 *   lastResetTime: ?mojoBase.mojom.Time,
 * }}
 */
let Network;

/**
 * Helper function to create a Network object.
 * @param {string} guid
 * @param {string} name
 * @param {!chromeos.networkConfig.mojom.NetworkType} type
 * @param {!Array<!Object>} counters
 * @param {?mojoBase.mojom.Time} lastResetTime
 * @return {Network} Network object
 */
function createNetwork(guid, name, type, counters, lastResetTime) {
  return {
    guid: guid,
    name: name,
    type: type,
    counters: counters,
    lastResetTime: lastResetTime,
  };
}

export class TrafficCountersAdapter {
  constructor() {
    /**
     * Network Config mojo remote.
     * @private {?chromeos.networkConfig.mojom.CrosNetworkConfigRemote}
     */
    this.networkConfig_ =
        MojoInterfaceProviderImpl.getInstance().getMojoServiceRemote();
  }

  /**
   * Requests traffic counters for active networks.
   * @return {!Promise<!Array<!Network>>}
   */
  async requestTrafficCountersForActiveNetworks() {
    const filter = {
      filter: chromeos.networkConfig.mojom.FilterType.kActive,
      networkType: chromeos.networkConfig.mojom.NetworkType.kAll,
      limit: chromeos.networkConfig.mojom.NO_LIMIT,
    };
    const networks = [];
    const networkStateList =
        await this.networkConfig_.getNetworkStateList(filter);
    for (const networkState of networkStateList.result) {
      const trafficCounters =
          await this.requestTrafficCountersForNetwork(networkState.guid);
      const lastResetTime =
          await this.requestLastResetTimeForNetwork(networkState.guid);
      networks.push(createNetwork(
          networkState.guid, networkState.name, networkState.type,
          trafficCounters, lastResetTime));
    }
    return networks;
  }

  /**
   * Resets traffic counters for the given network.
   * @param {string} guid
   */
  async resetTrafficCountersForNetwork(guid) {
    await this.networkConfig_.resetTrafficCounters(guid);
  }

  /**
   * Requests traffic counters for the given network.
   * @param {string} guid
   * @return {!Promise<!Array<!Object>>} traffic counters for network with guid
   */
  async requestTrafficCountersForNetwork(guid) {
    const trafficCountersObj =
        await this.networkConfig_.requestTrafficCounters(guid);
    return trafficCountersObj.trafficCounters;
  }

  /**
   * Requests last reset time for the given network.
   * @param {string} guid
   * @return {?Promise<?mojoBase.mojom.Time>} last reset
   * time for network with guid
   */
  async requestLastResetTimeForNetwork(guid) {
    const managedPropertiesPromise =
        await this.networkConfig_.getManagedProperties(guid);
    if (!managedPropertiesPromise || !managedPropertiesPromise.result) {
      return null;
    }
    return managedPropertiesPromise.result.trafficCounterResetTime || null;
  }
}
