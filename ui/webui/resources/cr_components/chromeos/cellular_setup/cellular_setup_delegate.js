// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// #import {addSingletonGetter} from 'chrome://resources/js/cr.m.js';

cr.define('cellular_setup', function() {
  /**
   * Interface which provides alternate UI behaviors for the Cellular Setup
   * flows.
   * @interface
   */
  /* #export */ class CellularSetupDelegate {
    /**
     * @return {boolean} true if base page title text should be visible.
     */
    shouldShowPageTitle() {}

    /**
     * @return {boolean} true if cancel button should be visible.
     */
    shouldShowCancelButton() {}
  }

  // #cr_define_end
  return {
    CellularSetupDelegate: CellularSetupDelegate,
  };
});