// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeType} from '../common/js/volume_manager_types.js';

import type {EntryLocation} from './entry_location.js';
import type {FilesAppDirEntry, FilesAppEntry} from './files_app_entry_interfaces.js';
import type {VolumeInfo} from './volume_info.js';
import type {VolumeInfoList} from './volume_info_list.js';

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 */
export interface VolumeManager {
  /**
   * The list of VolumeInfo instances for each mounted volume.
   */
  volumeInfoList: VolumeInfoList;

  /**
   * Gets the 'fusebox-only' filter state: true if enabled, false if disabled.
   * The filter is only enabled by the SelectFileAsh (Lacros) file picker, and
   * implemented by {FilteredVolumeManager} override.
   */
  getFuseBoxOnlyFilterEnabled(): boolean;

  /**
   * Gets the 'media-store-files-only' filter state: true if enabled, false if
   * disabled. The filter is only enabled by the Android (ARC) file picker, and
   * implemented by {FilteredVolumeManager} override.
   */
  getMediaStoreFilesOnlyFilterEnabled(): boolean;

  /**
   * Disposes the instance. After the invocation of this method, any other
   * method should not be called.
   */
  dispose();

  /**
   * Obtains a volume info containing the passed entry.
   * @param entry Entry on the volume to be returned. Can be fake.
   * @return The VolumeInfo instance or null if not found.
   */
  getVolumeInfo(entry: Entry|FilesAppEntry): VolumeInfo|null;

  /**
   * Returns the drive connection state.
   * @return Connection state.
   */
  getDriveConnectionState(): chrome.fileManagerPrivate.DriveConnectionState;

  /**
   * @param fileUrl File url to the archive file.
   * @param password Password to decrypt archive file.
   * @return Fulfilled on success, otherwise rejected with a
   *     VolumeError.
   */
  mountArchive(fileUrl: string, password?: string): Promise<VolumeInfo>;

  /**
   * Cancels mounting an archive.
   * @param fileUrl File url to the archive file.
   * @return Fulfilled on success, otherwise rejected with a
   *     VolumeError.
   */
  cancelMounting(fileUrl: string): Promise<void>;

  /**
   * Unmounts a volume.
   * @param volumeInfo Volume to be unmounted.
   * @return Fulfilled on success, otherwise rejected with a
   *     VolumeError.
   */
  unmount(volumeInfo: VolumeInfo): Promise<void>;

  /**
   * Configures a volume.
   * @param volumeInfo Volume to be configured.
   * @return Fulfilled on success, otherwise rejected with an error message.
   */
  configure(volumeInfo: VolumeInfo): Promise<void>;

  /**
   * Obtains volume information of the current profile.
   *
   * @param volumeType Volume type.
   * @return Volume info.
   */
  getCurrentProfileVolumeInfo(volumeType: VolumeType): VolumeInfo|null;

  /**
   * Obtains location information from an entry.
   *
   * @param entry File or directory entry. It can be a fake entry.
   * @return Location information.
   */
  getLocationInfo(entry: Entry|FilesAppEntry): EntryLocation|null;

  /**
   * Adds an event listener to the target.
   * @param type The name of the event.
   * @param handler The handler for the event. This is called when the event is
   *     dispatched.
   */
  addEventListener(type: string, handler: (arg0: Event) => void): void;

  /**
   * Removes an event listener from the target.
   * @param type The name of the event.
   * @param handler The handler for the event.
   */
  removeEventListener(type: string, handler: (arg0: Event) => void): void;

  /**
   * Dispatches an event and calls all the listeners that are listening to
   * the type of the event.
   * @param event The event to dispatch.
   * @return Whether the default action was prevented. If someone
   *     calls preventDefault on the event object then this returns false.
   */
  dispatchEvent(event: Event): boolean;

  /**
   * Searches the information of the volume that exists on the given device
   * path.
   * @param devicePath Path of the device to search.
   * @return The volume's information, or null if not found.
   */
  findByDevicePath(devicePath: string): VolumeInfo|null;

  /**
   * Returns a promise that will be resolved when volume info, identified
   * by {@code volumeId} is created.
   *
   * @return The VolumeInfo Will not resolve if the volume is never mounted.
   */
  whenVolumeInfoReady(volumeId: string): Promise<VolumeInfo>;

  /**
   * Obtains the default display root entry.
   * @param callback Callback passed the default display root.
   */
  getDefaultDisplayRoot(
      callback: (arg0: DirectoryEntry|FilesAppDirEntry|null) => void): void;

  /**
   * Checks if any volumes are disabled for selection.
   * @return Whether any volumes are disabled for selection.
   */
  hasDisabledVolumes(): boolean;

  /**
   * Checks whether the given volume is disabled for selection.
   * @param volume Volume to check.
   * @return Whether the volume is disabled or not.
   */
  isDisabled(volume: VolumeType): boolean;

  /**
   * Checks if a volume is allowed.
   */
  isAllowedVolume(volumeInfo: VolumeInfo): boolean;
}

/**
 * Event object which is dispatched with 'externally-unmounted' event.
 */
export type ExternallyUnmountedEvent = CustomEvent<VolumeInfo>;
