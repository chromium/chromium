// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Crostini} from '../../externs/background/crostini.js';
import {VolumeManager} from '../../externs/volume_manager.js';

/**
 * Implementation of Crostini shared path state handler.
 *
 * @implements {Crostini}
 */
export class CrostiniImpl {
  constructor() {
    /**
     * Map of VM name to container name to if it's enabled. False or undefined
     * means not enabled.
     * @private {!Object<!Object<boolean>>}
     */
    this.enabled_ = {};

    /**
     * Maintains a list of paths shared with VMs.
     * Keyed by entry.toURL(), maps to list of VMs.
     * @private @dict {!Object<!Array<string>>}
     */
    this.sharedPaths_ = {};

    /** @private {?VolumeManager} */
    this.volumeManager_ = null;
  }

  /**
   * Initialize enabled settings and register for any shared path changes.
   * Must be done after loadTimeData is available.
   */
  initEnabled() {
    const guests = /** @type {!Array<!Object<string>>} */ (
        loadTimeData.getValue('VMS_FOR_SHARING'));
    for (const guest of guests) {
      this.setEnabled(guest.vmName, guest.containerName, true);
    }
    chrome.fileManagerPrivate.onCrostiniChanged.addListener(
        this.onCrostiniChanged_.bind(this));
  }

  /**
   * Initialize Volume Manager.
   * @param {!VolumeManager} volumeManager
   */
  initVolumeManager(volumeManager) {
    this.volumeManager_ = volumeManager;
  }

  /**
   * Set whether the specified Guest is enabled.
   * @param {string} vmName
   * @param {string} containerName
   * @param {boolean} enabled
   */
  setEnabled(vmName, containerName, enabled) {
    if (!this.enabled_[vmName]) {
      this.enabled_[vmName] = {};
    }
    this.enabled_[vmName][containerName] = enabled;
  }

  /**
   * Returns true if the specified VM is enabled.
   * @param {string} vmName
   * @return {boolean}
   */
  isEnabled(vmName) {
    return (!!this.enabled_[vmName]) &&
        Object.values(this.enabled_[vmName]).includes(true);
  }

  /**
   * @param {!Entry} entry
   * @return {?VolumeManagerCommon.RootType}
   * @private
   */
  getRoot_(entry) {
    const info =
        this.volumeManager_ && this.volumeManager_.getLocationInfo(entry);
    return info && info.rootType;
  }

  /**
   * Registers an entry as a shared path for the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  registerSharedPath(vmName, entry) {
    const url = entry.toURL();
    // Remove any existing paths that are children of the new path.
    // These paths will still be shared as a result of a parent path being
    // shared, but if the parent is unshared in the future, these children
    // paths should not remain.
    for (const [path, vms] of Object.entries(this.sharedPaths_)) {
      if (path.startsWith(url)) {
        this.unregisterSharedPath_(vmName, path);
      }
    }
    const vms = this.sharedPaths_[url];
    if (this.sharedPaths_[url]) {
      this.sharedPaths_[url].push(vmName);
    } else {
      this.sharedPaths_[url] = [vmName];
    }
  }

  /**
   * Unregisters path as a shared path from the specified VM.
   * @param {string} vmName
   * @param {string} path
   * @private
   */
  unregisterSharedPath_(vmName, path) {
    const vms = this.sharedPaths_[path];
    if (vms) {
      const newVms = vms.filter(vm => vm != vmName);
      if (newVms.length > 0) {
        this.sharedPaths_[path] = newVms;
      } else {
        delete this.sharedPaths_[path];
      }
    }
  }

  /**
   * Unregisters entry as a shared path from the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   */
  unregisterSharedPath(vmName, entry) {
    this.unregisterSharedPath_(vmName, entry.toURL());
  }

  /**
   * Handles events for enable/disable, share/unshare.
   * @param {!chrome.fileManagerPrivate.CrostiniEvent} event
   * @private
   */
  onCrostiniChanged_(event) {
    switch (event.eventType) {
      case chrome.fileManagerPrivate.CrostiniEventType.ENABLE:
        this.setEnabled(event.vmName, event.containerName, true);
        break;
      case chrome.fileManagerPrivate.CrostiniEventType.DISABLE:
        this.setEnabled(event.vmName, event.containerName, false);
        break;
      case chrome.fileManagerPrivate.CrostiniEventType.SHARE:
        for (const entry of event.entries) {
          this.registerSharedPath(event.vmName, assert(entry));
        }
        break;
      case chrome.fileManagerPrivate.CrostiniEventType.UNSHARE:
        for (const entry of event.entries) {
          this.unregisterSharedPath(event.vmName, assert(entry));
        }
        break;
    }
  }

  /**
   * Returns true if entry is shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @return {boolean} True if path is shared either by a direct
   *   share or from one of its ancestor directories.
   */
  isPathShared(vmName, entry) {
    // Check path and all ancestor directories.
    let path = entry.toURL();
    let root = path;
    if (entry && entry.filesystem && entry.filesystem.root) {
      root = entry.filesystem.root.toURL();
    }

    while (path.length > root.length) {
      const vms = this.sharedPaths_[path];
      if (vms && vms.includes(vmName)) {
        return true;
      }
      path = path.substring(0, path.lastIndexOf('/'));
    }
    const rootVms = this.sharedPaths_[root];
    return !!rootVms && rootVms.includes(vmName);
  }

  /**
   * Returns true if entry can be shared with the specified VM.
   * @param {string} vmName
   * @param {!Entry} entry
   * @param {boolean} persist If path is to be persisted.
   * @return {boolean}
   */
  canSharePath(vmName, entry, persist) {
    if (!this.isEnabled(vmName)) {
      return false;
    }

    // Only directories for persistent shares.
    if (persist && !entry.isDirectory) {
      return false;
    }

    const root = this.getRoot_(entry);

    // TODO(crbug.com/917920): Remove when DriveFS enforces allowed write paths.
    // Disallow Computers Grand Root, and Computer Root.
    if (root === VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
        (root === VolumeManagerCommon.RootType.COMPUTER &&
         entry.fullPath.split('/').length <= 3)) {
      return false;
    }

    // TODO(crbug.com/958840): Sharing Play files root is disallowed until
    // we can ensure it will not also share Downloads.
    if (root === VolumeManagerCommon.RootType.ANDROID_FILES &&
        entry.fullPath === '/') {
      return false;
    }

    // Special case to disallow PluginVm sharing on /MyFiles/PluginVm and
    // subfolders since it gets shared by default.
    if (vmName === CrostiniImpl.PLUGIN_VM &&
        root === VolumeManagerCommon.RootType.DOWNLOADS &&
        entry.fullPath.split('/')[1] === CrostiniImpl.PLUGIN_VM) {
      return false;
    }

    // Disallow sharing LinuxFiles with itself.
    if (vmName === CrostiniImpl.DEFAULT_VM &&
        root === VolumeManagerCommon.RootType.CROSTINI) {
      return false;
    }

    // Cannot share root of Shared with me since it represents 2 dirs:
    // `.files-by-id` and `.shortcut-targets-by-id`.
    if (root === VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME &&
        entry.fullPath === '/') {
      return false;
    }

    return CrostiniImpl.VALID_ROOT_TYPES_FOR_SHARE.has(root);
  }
}

/**
 * Default Crostini VM is 'termina'.
 * @const
 */
CrostiniImpl.DEFAULT_VM = 'termina';

/**
 * Plugin VM 'PvmDefault'.
 * @const
 */
CrostiniImpl.PLUGIN_VM = 'PvmDefault';

/**
 * @type {!Map<?VolumeManagerCommon.RootType, string>}
 * @const
 */
CrostiniImpl.VALID_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.DOWNLOADS, 'Downloads'],
  [VolumeManagerCommon.RootType.REMOVABLE, 'Removable'],
  [VolumeManagerCommon.RootType.ANDROID_FILES, 'AndroidFiles'],
  [VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT, 'DriveComputers'],
  [VolumeManagerCommon.RootType.COMPUTER, 'DriveComputers'],
  [VolumeManagerCommon.RootType.DRIVE, 'MyDrive'],
  [VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT, 'TeamDrive'],
  [VolumeManagerCommon.RootType.SHARED_DRIVE, 'TeamDrive'],
  [VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME, 'SharedWithMe'],
  [VolumeManagerCommon.RootType.CROSTINI, 'Crostini'],
  [VolumeManagerCommon.RootType.GUEST_OS, 'GuestOs'],
  [VolumeManagerCommon.RootType.ARCHIVE, 'Archive'],
  [VolumeManagerCommon.RootType.SMB, 'SMB'],
]);
