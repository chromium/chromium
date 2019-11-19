// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents each volume, such as "drive", "download directory", each "USB
 * flush storage", or "mounted zip archive" etc.
 * @interface
 */
class VolumeInfo {
  constructor() {
    /** @type {VolumeManagerCommon.VolumeType} */
    this.volumeType;

    /** @type {string} */
    this.volumeId;

    /** @type {FileSystem} */
    this.fileSystem;

    /**
     * Display root path. It is null before finishing to resolve the entry.
     * @type {DirectoryEntry}
     */
    this.displayRoot;

    /**
     * The display root path of Shared Drives directory. It is null before
     * finishing to resolve the entry. Valid only for Drive volume.
     * @type {DirectoryEntry}
     */
    this.sharedDriveDisplayRoot;

    /**
     * The display root path of Computers directory. It is null before finishing
     * to resolve the entry. Valid only for Drive volume.
     * @type {DirectoryEntry}
     */
    this.computersDisplayRoot;

    /**
     * The volume's fake entries such as Recent, Offline, Shared with me, etc...
     * in Google Drive.
     * @type {Object<!FakeEntry>}}
     */
    this.fakeEntries;

    /**
     * This represents if the mounting of the volume is successfully done or
     * not. (If error is empty string, the mount is successfully done)
     * @type {(string|undefined)}
     */
    this.error;

    /**
     * The type of device. (e.g. USB, SD card, DVD etc.)
     * @type {(string|undefined)}
     */
    this.deviceType;

    /**
     * If the volume is removable, devicePath is the path of the system device
     * this device's block is a part of. (e.g.
     * /sys/devices/pci0000:00/.../8:0:0:0/) Otherwise, this should be empty.
     * @type {(string|undefined)}
     */
    this.devicePath;

    /** @type {boolean} */
    this.isReadOnly;

    /**
     * @type {boolean} Whether the device is read-only removable device or not.
     */
    this.isReadOnlyRemovableDevice;

    /** @type {!{displayName:string, isCurrentProfile:boolean}} */
    this.profile;

    /**
     * Label for the volume if the volume is either removable or a provided file
     * system. In case of removables, if disk is a parent, then its label, else
     * parent's label (e.g. "TransMemory").
     * @type {string}
     */
    this.label;

    /**
     * ID of a provider for this volume.
     * @type {(string|undefined)}
     */
    this.providerId;

    /**
     * Set of icons for this volume.
     * @type {!chrome.fileManagerPrivate.IconSet}
     */
    this.iconSet;

    /**
     * True if the volume contains media.
     * @type {boolean}
     */
    this.hasMedia;

    /**
     * True if the volume is configurable.
     * See https://developer.chrome.com/apps/fileSystemProvider.
     * @type {boolean}
     */
    this.configurable;

    /**
     * True if the volume notifies about changes via file/directory watchers.
     * @type {boolean}
     */
    this.watchable;

    /**  @type {VolumeManagerCommon.Source} */
    this.source;

    /**  @type {VolumeManagerCommon.FileSystemType} */
    this.diskFileSystemType;

    /**
     * @type {FilesAppEntry} an entry to be used as prefix of this volume on
     *     breadcrumbs, e.g. "My Files > Downloads", "My Files" is a prefixEntry
     *     on "Downloads" VolumeInfo.
     */
    this.prefixEntry;
  }

  /**
   * Starts resolving the display root and obtains it.  It may take long time
   * for Drive. Once resolved, it is cached.
   *
   * @param {function(!DirectoryEntry)=} opt_onSuccess Success callback with the
   *     display root directory as an argument.
   * @param {function(*)=} opt_onFailure Failure callback.
   * @return {!Promise<!DirectoryEntry>}
   */
  resolveDisplayRoot(opt_onSuccess, opt_onFailure) {}
}
