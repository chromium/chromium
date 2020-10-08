// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import 'chrome://resources/mojo/mojo/public/js/mojo_bindings_lite.js';
// #import 'chrome://resources/mojo/url/mojom/url.mojom-lite.js';
// #import 'chrome://resources/mojo/chromeos/services/cellular_setup/public/mojom/cellular_setup.mojom-lite.js';
// clang-format on

cr.define('cellular_setup', function() {
  let cellularRemote = null;

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

  // #cr_define_end
  return {setCellularSetupRemoteForTesting, getCellularSetupRemote};
});
