// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManagerCommon} from '../common/js/volume_manager_types.js';

import {EntryLocation} from './entry_location.js';
import {FilesAppDirEntry, FilesAppEntry} from './files_app_entry_interfaces.js';

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 * @interface
 */
export class VolumeManager {
  constructor() {
    /**
     * The list of VolumeInfo instances for each mounted volume.
     * @type {import('./volume_info_list.js').VolumeInfoList}
     */
    this.volumeInfoList;
  }

  /**
   * Gets the 'fusebox-only' filter state: true if enabled, false if disabled.
   * The filter is only enabled by the SelectFileAsh (Lacros) file picker, and
   * implemented by {FilteredVolumeManager} override.
   * @return {boolean}
   */
  getFuseBoxOnlyFilterEnabled() {
    return false;
  }

  /**
   * Gets the 'media-store-files-only' filter state: true if enabled, false if
   * disabled. The filter is only enabled by the Android (ARC) file picker, and
   * implemented by {FilteredVolumeManager} override.
   * @return {boolean}
   */
  getMediaStoreFilesOnlyFilterEnabled() {
    return false;
  }

  /**
   * Disposes the instance. After the invocation of this method, any other
   * method should not be called.
   */
  dispose() {}

  /**
   * Obtains a volume info containing the passed entry.
   * @param {!Entry|!FilesAppEntry} entry Entry on the volume to be
   *     returned. Can be fake.
   * @return {?import('./volume_info.js').VolumeInfo} The VolumeInfo instance or
   *     null if not found.
   */
  // @ts-ignore: error TS6133: 'entry' is declared but its value is never read.
  getVolumeInfo(entry) {
    return null;
  }

  /**
   * Returns the drive connection state.
   * @return {chrome.fileManagerPrivate.DriveConnectionState} Connection state.
   */
  getDriveConnectionState() {
    // @ts-ignore: error TS2322: Type 'string' is not assignable to type
    // 'DriveConnectionState'.
    return chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE;
  }

  /**
   * @param {string} fileUrl File url to the archive file.
   * @param {string=} password Password to decrypt archive file.
   * @return {!Promise<!import('./volume_info.js').VolumeInfo>} Fulfilled on
   *     success, otherwise rejected with a VolumeManagerCommon.VolumeError.
   */
  // @ts-ignore: error TS6133: 'password' is declared but its value is never
  // read.
  mountArchive(fileUrl, password) {
    return Promise.reject('to be implemented');
  }

  /**
   * Cancels mounting an archive.
   * @param {string} fileUrl File url to the archive file.
   * @return {!Promise<void>} Fulfilled on success, otherwise rejected
   *     with a VolumeManagerCommon.VolumeError.
   */
  // @ts-ignore: error TS6133: 'fileUrl' is declared but its value is never
  // read.
  cancelMounting(fileUrl) {
    return Promise.resolve();
  }

  /**
   * Unmounts a volume.
   * @param {!import('./volume_info.js').VolumeInfo} volumeInfo Volume to be
   *     unmounted.
   * @return {!Promise<void>} Fulfilled on success, otherwise rejected with a
   *     VolumeManagerCommon.VolumeError.
   */
  // @ts-ignore: error TS6133: 'volumeInfo' is declared but its value is never
  // read.
  unmount(volumeInfo) {
    return Promise.reject('to be implementend');
  }

  /**
   * Configures a volume.
   * @param {!import('./volume_info.js').VolumeInfo} volumeInfo Volume to be
   *     configured.
   * @return {!Promise<void>} Fulfilled on success, otherwise rejected with an
   *     error message.
   */
  // @ts-ignore: error TS6133: 'volumeInfo' is declared but its value is never
  // read.
  configure(volumeInfo) {
    return Promise.resolve();
  }

  /**
   * Obtains volume information of the current profile.
   *
   * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
   * @return {?import('./volume_info.js').VolumeInfo} Volume info.
   */
  // @ts-ignore: error TS6133: 'volumeType' is declared but its value is never
  // read.
  getCurrentProfileVolumeInfo(volumeType) {
    return null;
  }

  /**
   * Obtains location information from an entry.
   *
   * @param {!Entry|!FilesAppEntry} entry File or directory entry. It
   *     can be a fake entry.
   * @return {?EntryLocation} Location information.
   */
  // @ts-ignore: error TS6133: 'entry' is declared but its value is never read.
  getLocationInfo(entry) {
    return null;
  }

  /**
   * Adds an event listener to the target.
   * @param {string} type The name of the event.
   * @param {function(!Event):void} handler The handler for the event. This is
   *     called when the event is dispatched.
   */
  // @ts-ignore: error TS6133: 'handler' is declared but its value is never
  // read.
  addEventListener(type, handler) {}

  /**
   * Removes an event listener from the target.
   * @param {string} type The name of the event.
   * @param {function(!Event):void} handler The handler for the event.
   */
  // @ts-ignore: error TS6133: 'handler' is declared but its value is never
  // read.
  removeEventListener(type, handler) {}

  /**
   * Dispatches an event and calls all the listeners that are listening to
   * the type of the event.
   * @param {!Event} event The event to dispatch.
   * @return {boolean} Whether the default action was prevented. If someone
   *     calls preventDefault on the event object then this returns false.
   */
  // @ts-ignore: error TS6133: 'event' is declared but its value is never read.
  dispatchEvent(event) {
    return false;
  }

  /**
   * Searches the information of the volume that exists on the given device
   * path.
   * @param {string} devicePath Path of the device to search.
   * @return {?import('./volume_info.js').VolumeInfo} The volume's information,
   *     or null if not found.
   */
  // @ts-ignore: error TS6133: 'devicePath' is declared but its value is never
  // read.
  findByDevicePath(devicePath) {
    return null;
  }

  /**
   * Returns a promise that will be resolved when volume info, identified
   * by {@code volumeId} is created.
   *
   * @param {string} volumeId
   * @return {!Promise<!import('./volume_info.js').VolumeInfo>} The VolumeInfo.
   *     Will not resolve if the volume is never mounted.
   */
  // @ts-ignore: error TS6133: 'volumeId' is declared but its value is never
  // read.
  whenVolumeInfoReady(volumeId) {
    return Promise.reject('to be implemented');
  }

  /**
   * Obtains the default display root entry.
   * @param {function((DirectoryEntry|FilesAppDirEntry|null)):void} callback
   * Callback passed the default display root.
   */
  // @ts-ignore: error TS6133: 'callback' is declared but its value is never
  // read.
  getDefaultDisplayRoot(callback) {}

  /**
   * Checks if any volumes are disabled for selection.
   * @return {boolean} Whether any volumes are disabled for selection.
   */
  hasDisabledVolumes() {
    return false;
  }

  /**
   * Checks whether the given volume is disabled for selection.
   * @param {!VolumeManagerCommon.VolumeType} volume Volume to check.
   * @return {boolean} Whether the volume is disabled or not.
   */
  // @ts-ignore: error TS6133: 'volume' is declared but its value is never read.
  isDisabled(volume) {
    return false;
  }

  /**
   * Checks if a volume is allowed.
   *
   * @param {!import('./volume_info.js').VolumeInfo} volumeInfo
   * @return {boolean}
   */
  // @ts-ignore: error TS6133: 'volumeInfo' is declared but its value is never
  // read.
  isAllowedVolume(volumeInfo) {
    return true;
  }
}

/**
 * Event object which is dispatched with 'externally-unmounted' event.
 * @typedef {!CustomEvent<!import('./volume_info.js').VolumeInfo>}
 */
// @ts-ignore: error TS7005: Variable 'ExternallyUnmountedEvent' implicitly has
// an 'any' type.
export let ExternallyUnmountedEvent;
