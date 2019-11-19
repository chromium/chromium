// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Crostini shared path state handler.
 *
 * @interface
 */
class Crostini {
  /**
   * Initialize enabled settings.
   * Must be done after loadTimeData is available.
   */
  initEnabled() {}

  /**
   * Initialize Volume Manager.
   * @param {!VolumeManager} volumeManager
   */
  initVolumeManager(volumeManager) {}

  /**
   * Register for any shared path changes.
   */
  listen() {}

  /**
   * Set whether the specified VM is enabled.
   * @param {string} vmName
   * @param {boolean} enabled
   */
  setEnabled(vmName, enabled) {}

  /**
   * Returns true if the specified VM is enabled.
   * @param {string} vmName
   * @return {boolean}
   */
  isEnabled(vmName) {}

  /**
   * Registers an entry as a shared path for the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  registerSharedPath(vmName, entry) {}

  /**
   * Unregisters entry as a shared path from the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  unregisterSharedPath(vmName, entry) {}

  /**
   * Returns true if entry is shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @return {boolean} True if path is shared either by a direct
   *   share or from one of its ancestor directories.
   */
  isPathShared(vmName, entry) {}

  /**
   * Returns true if entry can be shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @param {boolean} persist If path is to be persisted.
   */
  canSharePath(vmName, entry, persist) {}
}
