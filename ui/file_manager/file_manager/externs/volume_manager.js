// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManagerCommon} from '../common/js/volume_manager_types.js';

import {EntryLocation} from './entry_location.js';
import {FilesAppDirEntry, FilesAppEntry} from './files_app_entry_interfaces.js';
import {VolumeInfo} from './volume_info.js';
import {VolumeInfoList} from './volume_info_list.js';

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 * @interface
 */
export class VolumeManager {
  constructor() {
    /**
     * The list of VolumeInfo instances for each mounted volume.
     * @type {VolumeInfoList}
     */
    this.volumeInfoList;
  }

  /**
   * Gets the 'fusebox-only' filter state: true if enabled, false if disabled.
   * The filter is only enabled by the SelectFileAsh (Lacros) file picker, and
   * implemented by {FilteredVolumeManager} override.
   * @return {boolean}
   */
  getFuseBoxOnlyFilterEnabled() {}

  /**
   * Gets the 'media-store-files-only' filter state: true if enabled, false if
   * disabled. The filter is only enabled by the Android (ARC) file picker, and
   * implemented by {FilteredVolumeManager} override.
   * @return {boolean}
   */
  getMediaStoreFilesOnlyFilterEnabled() {}

  /**
   * Disposes the instance. After the invocation of this method, any other
   * method should not be called.
   */
  dispose() {}

  /**
   * Obtains a volume info containing the passed entry.
   * @param {!Entry|!FilesAppEntry} entry Entry on the volume to be
   *     returned. Can be fake.
   * @return {?VolumeInfo} The VolumeInfo instance or null if not found.
   */
  getVolumeInfo(entry) {}

  /**
   * Returns the drive connection state.
   * @return {chrome.fileManagerPrivate.DriveConnectionState} Connection state.
   */
  getDriveConnectionState() {}

  /**
   * @param {string} fileUrl File url to the archive file.
   * @param {string=} password Password to decrypt archive file.
   * @return {!Promise<!VolumeInfo>} Fulfilled on success, otherwise rejected
   *     with a VolumeManagerCommon.VolumeError.
   */
  mountArchive(fileUrl, password) {}

  /**
   * Cancels mounting an archive.
   * @param {string} fileUrl File url to the archive file.
   * @return {!Promise<void>} Fulfilled on success, otherwise rejected
   *     with a VolumeManagerCommon.VolumeError.
   */
  cancelMounting(fileUrl) {}

  /**
   * Unmounts a volume.
   * @param {!VolumeInfo} volumeInfo Volume to be unmounted.
   * @return {!Promise<void>} Fulfilled on success, otherwise rejected with a
   *     VolumeManagerCommon.VolumeError.
   */
  unmount(volumeInfo) {}

  /**
   * Configures a volume.
   * @param {!VolumeInfo} volumeInfo Volume to be configured.
   * @return {!Promise} Fulfilled on success, otherwise rejected with an error
   *     message.
   */
  configure(volumeInfo) {}

  /**
   * Obtains volume information of the current profile.
   *
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
   * @return {?VolumeInfo} Volume info.
   */
  getCurrentProfileVolumeInfo(volumeType) {}

  /**
   * Obtains location information from an entry.
   *
   * @param {!Entry|!FilesAppEntry} entry File or directory entry. It
   *     can be a fake entry.
   * @return {?EntryLocation} Location information.
   */
  getLocationInfo(entry) {}

  /**
   * Adds an event listener to the target.
   * @param {string} type The name of the event.
   * @param {function(!Event)} handler The handler for the event. This is
   *     called when the event is dispatched.
   */
  addEventListener(type, handler) {}

  /**
   * Removes an event listener from the target.
   * @param {string} type The name of the event.
   * @param {function(!Event)} handler The handler for the event.
   */
  removeEventListener(type, handler) {}

  /**
   * Dispatches an event and calls all the listeners that are listening to
   * the type of the event.
   * @param {!Event} event The event to dispatch.
   * @return {boolean} Whether the default action was prevented. If someone
   *     calls preventDefault on the event object then this returns false.
   */
  dispatchEvent(event) {}

  /**
   * Searches the information of the volume that exists on the given device
   * path.
   * @param {string} devicePath Path of the device to search.
   * @return {?VolumeInfo} The volume's information, or null if not found.
   */
  findByDevicePath(devicePath) {}

  /**
   * Returns a promise that will be resolved when volume info, identified
   * by {@code volumeId} is created.
   *
   * @param {string} volumeId
   * @return {!Promise<!VolumeInfo>} The VolumeInfo. Will not resolve
   *     if the volume is never mounted.
   */
  whenVolumeInfoReady(volumeId) {}

  /**
   * Obtains the default display root entry.
   * @param {function(DirectoryEntry)|function(FilesAppDirEntry)} callback
   * Callback passed the default display root.
   */
  getDefaultDisplayRoot(callback) {}

  /**
   * Checks if any volumes are disabled for selection.
   * @return {boolean} Whether any volumes are disabled for selection.
   */
  hasDisabledVolumes() {}

  /**
   * Checks whether the given volume is disabled for selection.
   * @param {!VolumeManagerCommon.VolumeType} volume Volume to check.
   * @return {boolean} Whether the volume is disabled or not.
   */
  isDisabled(volume) {}

  /**
   * Checks if a volume is allowed.
   *
   * @param {!VolumeInfo} volumeInfo
   * @return {boolean}
   */
  isAllowedVolume(volumeInfo) {}
}

/**
 * Event object which is dispached with 'externally-unmounted' event.
 * @typedef {!CustomEvent<!VolumeInfo>}
 */
export let ExternallyUnmountedEvent;
