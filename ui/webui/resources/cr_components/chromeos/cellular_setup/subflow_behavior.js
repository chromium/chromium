// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {assertNotReached} from 'chrome://resources/js/assert.m.js';
// #import {ButtonBarState} from "./cellular_types.m.js";

/**
 * @fileoverview Polymer behavior for dealing with Cellular setup subflows.
 * It includes some methods and property shared between subflows.
 */

/** @polymerBehavior */
/* #export */ const SubflowBehavior = {
  properties: {

    /**
     * Button bar button state.
     * @type {!cellularSetup.ButtonBarState}
     */
    buttonState: {
      type: Object,
      notify: true,
    },
  },

  /**
   * Initialize the subflow.
   */
  initSubflow() {
    assertNotReached();
  },

  /**
   * Handles forward navigation within subpage.
   */
  navigateForward() {
    assertNotReached();
  },

  /**
   * Handles backward navigation within subpage.
   * @returns {boolean} true if backward navigation was handled.
   */
  attemptBackwardNavigation() {
    assertNotReached();
  },
};