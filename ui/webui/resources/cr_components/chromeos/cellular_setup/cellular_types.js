// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Constants used in cellular setup flow.
 */
cr.define('cellularSetup', function() {
  /** @enum {string} */
  /* #export */ const CellularSetupPageName = {
    ESIM_FLOW_UI: 'esim-flow-ui',
    PSIM_FLOW_UI: 'psim-flow-ui',
    SETUP_FLOW_SELECTION: 'setup-selection-flow'
  };

  /** @enum {number} */
  /* #export */ const ButtonState = {
    ENABLED: 1,
    DISABLED: 2,
  };

  /** @enum {number} */
  /* #export */ const Button = {
    BACKWARD: 1,
    CANCEL: 2,
    FORWARD: 3,
    TRY_AGAIN: 4,
  };

  /**
   * @typedef {{
   *   backward: (!cellularSetup.ButtonState|undefined),
   *   cancel: (!cellularSetup.ButtonState|undefined),
   *   forward: (!cellularSetup.ButtonState|undefined),
   *   tryAgain: (!cellularSetup.ButtonState|undefined),
   * }}
   */
  /* #export */ let ButtonBarState;

  // #cr_define_end
  return {
    ButtonState: ButtonState,
    Button: Button,
    ButtonBarState: ButtonBarState,
    CellularSetupPageName: CellularSetupPageName,
  };
});
