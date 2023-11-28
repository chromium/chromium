// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {FileSystemType, Source, VolumeType} from '../common/js/volume_manager_types.js';

import {FakeEntry, FilesAppEntry} from './files_app_entry_interfaces.js';

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 */
export interface VolumeInfo {
  volumeType: VolumeType;
  volumeId: string;
  fileSystem: FileSystem;

  /**
   * Display root path. It is null before finishing to resolve the entry.
   */
  displayRoot: DirectoryEntry;

  /**
   * The display root path of Shared Drives directory. It is null before
   * finishing to resolve the entry. Valid only for Drive volume.
   */
  sharedDriveDisplayRoot: DirectoryEntry;

  /**
   * The display root path of Computers directory. It is null before finishing
   * to resolve the entry. Valid only for Drive volume.
   */
  computersDisplayRoot: DirectoryEntry;

  /**
   * The volume's fake entries such as Recent, Offline, Shared with me, etc...
   * in Google Drive.
   */
  fakeEntries: Record<string, FakeEntry>;

  /**
   * This represents if the mounting of the volume is successfully done or
   * not. (If error is empty string, the mount is successfully done)
   */
  error?: string;

  /**
   * The type of device. (e.g. USB, SD card, DVD etc.)
   */
  deviceType?: string;

  /**
   * If the volume is removable, devicePath is the path of the system device
   * this device's block is a part of. (e.g.
   * /sys/devices/pci0000:00/.../8:0:0:0/) Otherwise, this should be empty.
   */
  devicePath?: string;

  /**
   * Drive label of the volume. Removable partitions belonging to the same
   * device will share the same drive label.
   */
  driveLabel?: string;


  isReadOnly: boolean;

  /**
   * Whether the device is read-only removable device or not.
   */
  isReadOnlyRemovableDevice: boolean;

  profile: {displayName: string, isCurrentProfile: boolean};

  /**
   * Label for the volume if the volume is either removable or a provided file
   * system. In case of removables, if disk is a parent, then its label, else
   * parent's label (e.g. "TransMemory").
   */
  label: string;

  /**
   * ID of a provider for this volume.
   */
  providerId?: string;

  /**
   * Set of icons for this volume.
   */
  iconSet: chrome.fileManagerPrivate.IconSet;

  /**
   * True if the volume contains media.
   */
  hasMedia: boolean;

  /**
   * True if the volume is configurable.
   * See https://developer.chrome.com/apps/fileSystemProvider.
   */
  configurable: boolean;

  /**
   * True if the volume notifies about changes via file/directory watchers.
   */
  watchable: boolean;

  source: Source;
  diskFileSystemType: FileSystemType;

  /**
   * An entry to be used as prefix of this volume on breadcrumbs,
   * e.g. "My Files > Downloads",
   * "My Files" is a prefixEntry on "Downloads" VolumeInfo.
   */
  prefixEntry: FilesAppEntry|null;

  /**
   * The path on the remote host where this volume is mounted, for crostini
   * this is the user's homedir (/home/<username>).
   */
  remoteMountPath?: string;

  /**
   * If this is a GuestOS volume, the type of the VM which owns this volume.
   */
  vmType?: chrome.fileManagerPrivate.VmType;

  /**
   * Starts resolving the display root and obtains it.  It may take long time
   * for Drive. Once resolved, it is cached.
   *
   * @param onSuccess Success callback with the
   *     display root directory as an argument.
   */
  resolveDisplayRoot(
      onSuccess?: (entry: DirectoryEntry) => void,
      onFailure?: (_: any) => void): Promise<DirectoryEntry>;
}
