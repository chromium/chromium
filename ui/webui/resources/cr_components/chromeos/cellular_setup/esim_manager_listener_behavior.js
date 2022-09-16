// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Polymer behavior for observing ESimManagerObserver
 * events.
 */

import {observeESimManager} from './mojo_interface_provider.js';

/** @polymerBehavior */
export const ESimManagerListenerBehavior = {
  /** @private {?ash.cellularSetup.mojom.ESimManagerObserver} */
  observer_: null,

  /** @override */
  attached() {
    observeESimManager(this);
  },

  // ESimManagerObserver methods. Override these in the implementation.

  onAvailableEuiccListChanged() {},

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   */
  onProfileListChanged(euicc) {},

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   */
  onEuiccChanged(euicc) {},

  /**
   * @param {!ash.cellularSetup.mojom.ESimProfileRemote} profile
   */
  onProfileChanged(profile) {},
};

/** @interface */
export class ESimManagerListenerBehaviorInterface {
  onAvailableEuiccListChanged() {}

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   */
  onProfileListChanged(euicc) {}

  /**
   * @param {!ash.cellularSetup.mojom.EuiccRemote} euicc
   */
  onEuiccChanged(euicc) {}

  /**
   * @param {!ash.cellularSetup.mojom.ESimProfileRemote} profile
   */
  onProfileChanged(profile) {}
}
