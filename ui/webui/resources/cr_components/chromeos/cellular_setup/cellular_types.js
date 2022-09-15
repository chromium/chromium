// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Constants used in cellular setup flow.
 */

/** @enum {string} */
export const CellularSetupPageName = {
  ESIM_FLOW_UI: 'esim-flow-ui',
  PSIM_FLOW_UI: 'psim-flow-ui',
};

/** @enum {number} */
export const ButtonState = {
  ENABLED: 1,
  DISABLED: 2,
  HIDDEN: 3,
};

/** @enum {number} */
export const Button = {
  BACKWARD: 1,
  CANCEL: 2,
  FORWARD: 3,
};

/**
 * @typedef {{
 *   backward: (!ButtonState|undefined),
 *   cancel: (!ButtonState|undefined),
 *   forward: (!ButtonState|undefined),
 * }}
 */
export let ButtonBarState;
