// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/big_buffer.mojom-lite.js';
// #import 'chrome://resources/mojo/mojo/public/mojom/base/string16.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/cellular_setup/public/mojom/esim_manager.mojom-lite.js';
// clang-format on

cr.define('cellular_setup', function() {
  let cellularRemote = null;
  let eSimManagerRemote = null;

  /**
   * @param {?chromeos.cellularSetup.mojom.CellularSetupRemote}
   *        testCellularRemote A test cellular remote
   */
  /* #export */ function setCellularSetupRemoteForTesting(testCellularRemote) {
    cellularRemote = testCellularRemote;
  }

  /**
   * @returns {!chromeos.cellularSetup.mojom.CellularSetupRemote}
   */
  /* #export */ function getCellularSetupRemote() {
    if (cellularRemote) {
      return cellularRemote;
    }

    cellularRemote = chromeos.cellularSetup.mojom.CellularSetup.getRemote();

    return cellularRemote;
  }

  /**
   * @param {?chromeos.cellularSetup.mojom.ESimManagerRemote}
   *        testESimManagerRemote A test eSimManager remote
   */
  /* #export */ function setESimManagerRemoteForTesting(testESimManagerRemote) {
    eSimManagerRemote = testESimManagerRemote;
  }

  /**
   * @returns {!chromeos.cellularSetup.mojom.ESimManagerRemote}
   */
  /* #export */ function getESimManagerRemote() {
    if (eSimManagerRemote) {
      return eSimManagerRemote;
    }

    eSimManagerRemote = chromeos.cellularSetup.mojom.ESimManager.getRemote();

    return eSimManagerRemote;
  }

  // #cr_define_end
  return {
    setCellularSetupRemoteForTesting,
    getCellularSetupRemote,
    setESimManagerRemoteForTesting,
    getESimManagerRemote
  };
});
