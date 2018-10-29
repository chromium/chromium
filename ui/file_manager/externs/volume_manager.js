// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 * @interface
 */
function VolumeManager() {}

/**
 * The list of VolumeInfo instances for each mounted volume.
 * @type {VolumeInfoList}
 */
VolumeManager.prototype.volumeInfoList;

/**
 * Obtains a volume info containing the passed entry.
 * @param {!Entry|!FilesAppEntry} entry Entry on the volume to be
 *     returned. Can be fake.
 * @return {VolumeInfo} The VolumeInfo instance or null if not found.
 */
VolumeManager.prototype.getVolumeInfo;

/**
 * Returns the drive connection state.
 * @return {VolumeManagerCommon.DriveConnectionState} Connection state.
 */
VolumeManager.prototype.getDriveConnectionState = function() {};

/**
 * @param {string} fileUrl File url to the archive file.
 * @param {function(VolumeInfo)} successCallback Success callback.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback Error
 *     callback.
 */
VolumeManager.prototype.mountArchive =
    function(fileUrl, successCallback, errorCallback) {};

/**
 * Unmounts a volume.
 * @param {!VolumeInfo} volumeInfo Volume to be unmounted.
 * @param {function()} successCallback Success callback.
 * @param {function(VolumeManagerCommon.VolumeError)} errorCallback Error
 *     callback.
 */
VolumeManager.prototype.unmount =
    function(volumeInfo, successCallback, errorCallback) {};

/**
 * Configures a volume.
 * @param {!VolumeInfo} volumeInfo Volume to be configured.
 * @return {!Promise} Fulfilled on success, otherwise rejected with an error
 *     message.
 */
VolumeManager.prototype.configure = function(volumeInfo) {};

/**
 * Obtains volume information of the current profile.
 *
 * @param {VolumeManagerCommon.VolumeType} volumeType Volume type.
 * @return {VolumeInfo} Volume info.
 */
VolumeManager.prototype.getCurrentProfileVolumeInfo = function(volumeType) {};

/**
 * Obtains location information from an entry.
 *
 * @param {!Entry|!FilesAppEntry} entry File or directory entry. It
 *     can be a fake entry.
 * @return {EntryLocation} Location information.
 */
VolumeManager.prototype.getLocationInfo = function(entry) {};

/**
 * Adds an event listener to the target.
 * @param {string} type The name of the event.
 * @param {function(!Event)} handler The handler for the event. This is
 *     called when the event is dispatched.
 */
VolumeManager.prototype.addEventListener = function(type, handler) {};

/**
 * Removes an event listener from the target.
 * @param {string} type The name of the event.
 * @param {function(!Event)} handler The handler for the event.
 */
VolumeManager.prototype.removeEventListener = function(type, handler) {};

/**
 * Dispatches an event and calls all the listeners that are listening to
 * the type of the event.
 * @param {!Event} event The event to dispatch.
 * @return {boolean} Whether the default action was prevented. If someone
 *     calls preventDefault on the event object then this returns false.
 */
VolumeManager.prototype.dispatchEvent = function(event) {};

/**
 * Searches the information of the volume that exists on the given device path.
 * @param {string} devicePath Path of the device to search.
 * @return {VolumeInfo} The volume's information, or null if not found.
 */
VolumeManager.prototype.findByDevicePath = function(devicePath) {};

/**
 * Returns a promise that will be resolved when volume info, identified
 * by {@code volumeId} is created.
 *
 * @param {string} volumeId
 * @return {!Promise<!VolumeInfo>} The VolumeInfo. Will not resolve
 *     if the volume is never mounted.
 */
VolumeManager.prototype.whenVolumeInfoReady = function(volumeId) {};

/**
 * Obtains the default display root entry.
 * @param {function(DirectoryEntry)|function(FilesAppDirEntry)} callback
 * Callback passed the default display root.
 */
VolumeManager.prototype.getDefaultDisplayRoot = function(callback) {};

/**
 * Event object which is dispached with 'externally-unmounted' event.
 * @constructor
 * @extends {Event}
 */
function ExternallyUnmountedEvent() {}

/**
 * @type {!VolumeInfo}
 */
ExternallyUnmountedEvent.prototype.volumeInfo;
