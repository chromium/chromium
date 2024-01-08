// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManager} from './volume_manager.js';


const volumeManagerFactory = (() => {
  /**
   * The singleton instance of VolumeManager. Initialized by the first
   * invocation of getInstance().
   */
  let instance: VolumeManager|null = null;

  let instanceInitialized: Promise<void>|null = null;

  /**
   * Returns the VolumeManager instance asynchronously. If it has not been
   * created or is under initialization, it will waits for the finish of the
   * initialization.
   */
  async function getInstance(): Promise<VolumeManager> {
    if (!instance) {
      instance = new VolumeManager();
      instanceInitialized = instance.initialize();
    }
    await instanceInitialized;
    return instance;
  }

  /**
   * Returns instance of VolumeManager for debug purpose.
   * This method returns VolumeManager.instance which may not be initialized.
   *
   */
  function getInstanceForDebug(): VolumeManager|null {
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
    revokeInstanceForTesting: revokeInstanceForTesting,
  };
})();

export {volumeManagerFactory};
