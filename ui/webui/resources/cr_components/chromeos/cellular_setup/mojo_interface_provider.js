// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
import 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/cellular_setup.mojom-lite.js';
import 'chrome://resources/mojo/chromeos/ash/services/cellular_setup/public/mojom/esim_manager.mojom-lite.js';

let cellularRemote = null;
let eSimManagerRemote = null;
let isTesting = false;

/**
 * @param {?ash.cellularSetup.mojom.CellularSetupRemote}
 *        testCellularRemote A test cellular remote
 */
export function setCellularSetupRemoteForTesting(testCellularRemote) {
  cellularRemote = testCellularRemote;
  isTesting = true;
}

/**
 * @returns {!ash.cellularSetup.mojom.CellularSetupRemote}
 */
export function getCellularSetupRemote() {
  if (cellularRemote) {
    return cellularRemote;
  }

  cellularRemote = ash.cellularSetup.mojom.CellularSetup.getRemote();

  return cellularRemote;
}

/**
 * @param {?ash.cellularSetup.mojom.ESimManagerRemote}
 *        testESimManagerRemote A test eSimManager remote
 */
export function setESimManagerRemoteForTesting(testESimManagerRemote) {
  eSimManagerRemote = testESimManagerRemote;
  isTesting = true;
}

/**
 * @returns {!ash.cellularSetup.mojom.ESimManagerRemote}
 */
export function getESimManagerRemote() {
  if (eSimManagerRemote) {
    return eSimManagerRemote;
  }

  eSimManagerRemote = ash.cellularSetup.mojom.ESimManager.getRemote();

  return eSimManagerRemote;
}

/**
 * @param {!ash.cellularSetup.mojom.ESimManagerObserverInterface} observer
 * @returns {?ash.cellularSetup.mojom.ESimManagerObserverReceiver}
 */
export function observeESimManager(observer) {
  if (isTesting) {
    getESimManagerRemote().addObserver(
        /** @type {!ash.cellularSetup.mojom.ESimManagerObserverRemote} */
        (observer));
    return null;
  }

  const receiver =
      new ash.cellularSetup.mojom.ESimManagerObserverReceiver(observer);
  getESimManagerRemote().addObserver(receiver.$.bindNewPipeAndPassRemote());
  return receiver;
}
