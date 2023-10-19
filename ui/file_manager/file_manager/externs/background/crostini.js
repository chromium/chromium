// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManager} from '../volume_manager.js';

/**
 * Crostini shared path state handler.
 *
 * @interface
 */
export class Crostini {
  /**
   * Initialize enabled settings and register for any shared path changes.
   * Must be done after loadTimeData is available.
   */
  initEnabled() {}

  /**
   * Initialize Volume Manager.
   * @param {!VolumeManager} volumeManager
   */
  // @ts-ignore: error TS6133: 'volumeManager' is declared but its value is
  // never read.
  initVolumeManager(volumeManager) {}

  /**
   * Set whether the specified Guest is enabled.
   * @param {string} vmName
   * @param {string} containerName
   * @param {boolean} enabled
   */
  // @ts-ignore: error TS6133: 'enabled' is declared but its value is never
  // read.
  setEnabled(vmName, containerName, enabled) {}

  /**
   * Returns true if the specified VM is enabled.
   * @param {string} vmName
   * @return {boolean}
   */
  // @ts-ignore: error TS6133: 'vmName' is declared but its value is never read.
  isEnabled(vmName) {
    return false;
  }

  /**
   * Registers an entry as a shared path for the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  // @ts-ignore: error TS6133: 'entry' is declared but its value is never read.
  registerSharedPath(vmName, entry) {}

  /**
   * Unregisters entry as a shared path from the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  // @ts-ignore: error TS6133: 'entry' is declared but its value is never read.
  unregisterSharedPath(vmName, entry) {}

  /**
   * Returns true if entry is shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @return {boolean} True if path is shared either by a direct
   *   share or from one of its ancestor directories.
   */
  // @ts-ignore: error TS6133: 'entry' is declared but its value is never read.
  isPathShared(vmName, entry) {
    return false;
  }

  /**
   * Returns true if entry can be shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @param {boolean} persist If path is to be persisted.
   * @return {boolean}
   */
  // @ts-ignore: error TS6133: 'persist' is declared but its value is never
  // read.
  canSharePath(vmName, entry, persist) {
    return false;
  }
}
