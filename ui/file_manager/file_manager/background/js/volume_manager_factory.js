// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {uselessCode} Temporary suppress because of the line exporting.
 */

// #import {VolumeManagerImpl} from './volume_manager_impl.m.js';
// #import {VolumeManager} from '../../../externs/volume_manager.m.js';

// eslint-disable-next-line no-var
var volumeManagerFactory = (() => {
  /**
   * The singleton instance of VolumeManager. Initialized by the first
   * invocation of getInstance().
   * @type {?VolumeManagerImpl}
   */
  let instance = null;

  /**
   * @type {?Promise<void>}
   */
  let instanceInitialized = null;

  /**
   * Returns the VolumeManager instance asynchronously. If it has not been
   * created or is under initialization, it will waits for the finish of the
   * initialization.
   * @return {!Promise<!VolumeManager>} Promise to be fulfilled with the volume
   *     manager.
   */
  async function getInstance() {
    if (!instance) {
      instance = new VolumeManagerImpl();
      instanceInitialized = instance.initialize();
    }
    await instanceInitialized;
    return instance;
  }

  /**
   * Returns instance of VolumeManager for debug purpose.
   * This method returns VolumeManager.instance which may not be initialized.
   *
   * @return {VolumeManager} Volume manager.
   */
  function getInstanceForDebug() {
    return instance;
  }

  /**
   * Revokes the singleton instance for testing.
   */
  function revokeInstanceForTesting() {
    instanceInitialized = null;
    instance = null;
  }

  return {
    getInstance: getInstance,
    getInstanceForDebug: getInstanceForDebug,
    revokeInstanceForTesting: revokeInstanceForTesting
  };
})();

// eslint-disable-next-line semi,no-extra-semi
/* #export */ {volumeManagerFactory};
