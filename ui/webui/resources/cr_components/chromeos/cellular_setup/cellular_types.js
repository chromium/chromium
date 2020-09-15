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
    HIDDEN: 1,
    SHOWN_AND_ENABLED: 2,
    SHOWN_BUT_DISABLED: 3,
  };

  /** @enum {number} */
  /* #export */ const Button = {
    BACKWARD: 1,
    CANCEL: 2,
    DONE: 3,
    NEXT: 4,
    TRY_AGAIN: 5,
  };

  /**
   * @typedef {{
   *   backward: (!cellularSetup.ButtonState|undefined),
   *   cancel: (!cellularSetup.ButtonState|undefined),
   *   done: (!cellularSetup.ButtonState|undefined),
   *   next: (!cellularSetup.ButtonState|undefined),
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
