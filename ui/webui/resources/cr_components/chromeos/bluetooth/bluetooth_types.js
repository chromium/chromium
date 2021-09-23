// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @enum {number} */
export const ButtonState = {
  ENABLED: 1,
  DISABLED: 2,
  HIDDEN: 3,
};

/** @enum {number} */
export const ButtonName = {
  CANCEL: 1,
  PAIR: 2,
};

/**
 * @typedef {{
 *   cancel: (!ButtonState),
 *   pair: (!ButtonState),
 * }}
 */
export let ButtonBarState;