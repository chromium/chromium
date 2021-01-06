// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {getESimManagerRemote} from './mojo_interface_provider.m.js';

/**
 * @fileoverview Polymer behavior for observing ESimManagerObserver
 * events.
 */

/** @polymerBehavior */
/* #export */ const ESimManagerListenerBehavior = {
  /** @private {?chromeos.cellularSetup.mojom.ESimManagerObserver} */
  observer_: null,

  /** @override */
  attached() {
    this.observer_ =
        new chromeos.cellularSetup.mojom.ESimManagerObserverReceiver(this);
    cellular_setup.getESimManagerRemote().addObserver(
        this.observer_.$.bindNewPipeAndPassRemote());
  },

  // ESimManagerObserver methods. Override these in the implementation.

  onAvailableEuiccListChanged() {},

  /**
   * @param {!chromeos.cellularSetup.mojom.EuiccRemote} euicc
   */
  onProfileListChanged(euicc) {},

  /**
   * @param {!chromeos.cellularSetup.mojom.EuiccRemote} euicc
   */
  onEuiccChanged(euicc) {},

  /**
   * @param {!chromeos.cellularSetup.mojom.ESimProfileRemote} profile
   */
  onProfileChanged(profile) {},
};