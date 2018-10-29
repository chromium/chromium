// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

cr.define('multidevice_setup', function() {
  /**
   * Interface which provides the ability to set the host device and perform
   * related logic.
   * @interface
   */
  class MultiDeviceSetupDelegate {
    /** @return {boolean} */
    isPasswordRequiredToSetHost() {}

    /**
     * @param {string} hostDeviceId The ID of the host to set.
     * @param {string=} opt_authToken An auth token to authenticate the request;
     *     only necessary if isPasswordRequiredToSetHost() returns true.
     * @return {!Promise<{success: boolean}>}
     */
    setHostDevice(hostDeviceId, opt_authToken) {}

    /** @return {boolean} */
    shouldExitSetupFlowAfterSettingHost() {}

    /** @return {string} */
    getStartSetupCancelButtonTextId() {}
  }

  return {
    MultiDeviceSetupDelegate: MultiDeviceSetupDelegate,
  };
});
