// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Trash configuration of volumes which have trash, and where it
 * is located.
 */

/**
 * Configuration for where Trash is stored in a volume.
 */
class TrashConfig {
  /**
   * @param {VolumeManagerCommon.VolumeType} volumeType
   * @param {string} topDir Top directory of volume. Must end with a slash to
   *     make comparisons simpler.
   * @param {string} trashDir Trash directory. Must end with a slash to make
   *     comparisons simpler.
   * @param {boolean=} prefixPathWithRemoteMount Optional, if true, 'Path=' in
   *     *.trashinfo is prefixed with the volume.remoteMountPath. For crostini,
   *     this is the user's homedir (/home/<username>).
   */
  constructor(volumeType, topDir, trashDir, prefixPathWithRemoteMount = false) {
    this.id = `${volumeType}-${topDir}`;
    this.volumeType = volumeType;
    this.topDir = topDir;
    this.trashDir = trashDir;
    this.prefixPathWithRemoteMount = prefixPathWithRemoteMount;
    this.pathPrefix = '';
  }
}

/**
 * Volumes supported for Trash, and location of Trash dir. Items will be
 * searched in order.
 *
 * @type {!Array<!TrashConfig>}
 */
TrashConfig.CONFIG = [
  // MyFiles/Downloads is a separate volume on a physical device, and doing a
  // move from MyFiles/Downloads/<path> to MyFiles/.Trash actually does a
  // copy across volumes, so we have a dedicated MyFiles/Downloads/.Trash.
  new TrashConfig(
      VolumeManagerCommon.VolumeType.DOWNLOADS, '/Downloads/',
      '/Downloads/.Trash/'),
  new TrashConfig(VolumeManagerCommon.VolumeType.DOWNLOADS, '/', '/.Trash/'),
  new TrashConfig(
      VolumeManagerCommon.VolumeType.CROSTINI, '/', '/.local/share/Trash/',
      /*prefixPathWithRemoteMount=*/ true),
];

/**
 * Wrapper for /.Trash/files and /.Trash/info directories.
 */
class TrashDirs {
  /**
   * @param {!DirectoryEntry} files /.Trash/files directory entry.
   * @param {!DirectoryEntry} info /.Trash/info directory entry.
   */
  constructor(files, info) {
    this.files = files;
    this.info = info;
  }

  /**
   * Promise wrapper for FileSystemDirectoryEntry.getDirectory(). Creates dir if
   * it does not exist.
   *
   * @param {!DirectoryEntry} dirEntry current directory.
   * @param {string} path name of directory within dirEntry.
   * @return {!Promise<!DirectoryEntry>} Promise which resolves with
   *     <dirEntry>/<path>.
   */
  static getDirectory(dirEntry, path) {
    return new Promise((resolve, reject) => {
      dirEntry.getDirectory(path, {create: true}, resolve, reject);
    });
  }

  /**
   * Get trash dirs from file system as specified in config.
   *
   * @param {!FileSystem} fileSystem File system from volume with trash.
   * @param {!TrashConfig} config Config specifying trash dir location.
   * @return {!Promise<!TrashDirs>} Promise which resolves with trash dirs.
   */
  static async getTrashDirs(fileSystem, config) {
    let trashRoot = fileSystem.root;
    const parts = config.trashDir.split('/');
    for (const part of parts) {
      if (part) {
        trashRoot = await TrashDirs.getDirectory(trashRoot, part);
      }
    }
    const trashFiles = await TrashDirs.getDirectory(trashRoot, 'files');
    const trashInfo = await TrashDirs.getDirectory(trashRoot, 'info');
    return new TrashDirs(trashFiles, trashInfo);
  }
}
