// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const Crostini = {};

/**
 * Set from cmd line flag 'crostini-files'.
 * @type {boolean}
 */
Crostini.IS_CROSTINI_FILES_ENABLED = false;

/**
 * Keep in sync with histograms.xml:FileBrowserCrostiniSharedPathsDepth
 * histogram_suffix.
 * @private {!Map<VolumeManagerCommon.RootType, string>}
 */
Crostini.VALID_ROOT_TYPES_FOR_SHARE = new Map([
  [VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT, 'DriveComputers'],
  [VolumeManagerCommon.RootType.COMPUTER, 'DriveComputers'],
  [VolumeManagerCommon.RootType.DOWNLOADS, 'Downloads'],
  [VolumeManagerCommon.RootType.DRIVE, 'MyDrive'],
  [VolumeManagerCommon.RootType.TEAM_DRIVE, 'TeamDrive'],
  [VolumeManagerCommon.RootType.REMOVABLE, 'Removable'],
]);

/** @private {string} */
Crostini.UMA_ROOT_TYPE_OTHER = 'Other';

/**
 * Maintains a list of paths shared with the crostini container.
 * Keyed by VolumeManagerCommon.RootType, with boolean set values
 * of string paths.  e.g. {'Downloads': {'/foo': true, '/bar': true}}.
 * @private @dict {!Object<!Object<boolean>>}
 */
Crostini.SHARED_PATHS_ = {};

/**
 * Registers an entry as a shared path.
 * @param {!Entry} entry
 * @param {!VolumeManager} volumeManager
 */
Crostini.registerSharedPath = function(entry, volumeManager) {
  const info = volumeManager.getLocationInfo(entry);
  if (!info)
    return;
  let paths = Crostini.SHARED_PATHS_[info.rootType];
  if (!paths) {
    paths = {};
    Crostini.SHARED_PATHS_[info.rootType] = paths;
  }
  paths[entry.fullPath] = true;

  // Record UMA.
  let suffix = Crostini.VALID_ROOT_TYPES_FOR_SHARE.get(info.rootType) ||
      Crostini.UMA_ROOT_TYPE_OTHER;
  metrics.recordSmallCount(
      'CrostiniSharedPaths.Depth.' + suffix,
      entry.fullPath.split('/').length - 1);
};

/**
 * Unregisters entry as a shared path.
 * @param {!Entry} entry
 * @param {!VolumeManager} volumeManager
 */
Crostini.unregisterSharedPath = function(entry, volumeManager) {
  const info = volumeManager.getLocationInfo(entry);
  if (!info)
    return;
  const paths = Crostini.SHARED_PATHS_[info.rootType];
  if (paths) {
    delete paths[entry.fullPath];
  }
};

/**
 * Returns true if entry is shared.
 * @param {!Entry} entry
 * @param {!VolumeManager} volumeManager
 * @return {boolean} True if path is shared either by a direct
 * share or from one of its ancestor directories.
 */
Crostini.isPathShared = function(entry, volumeManager) {
  const root = volumeManager.getLocationInfo(entry).rootType;
  const paths = Crostini.SHARED_PATHS_[root];
  if (!paths)
    return false;
  // Check path and all ancestor directories.
  let path = entry.fullPath;
  while (path != '') {
    if (paths[path])
      return true;
    path = path.substring(0, path.lastIndexOf('/'));
  }
  return false;
};

/**
 * @param {!Entry} entry
 * @param {!VolumeManager} volumeManager
 * @return {boolean} True if the entry is from crostini.
 */
Crostini.isCrostiniEntry = function(entry, volumeManager) {
  return volumeManager.getLocationInfo(entry).rootType ===
      VolumeManagerCommon.RootType.CROSTINI;
};

/**
 * Returns true if entry can be shared with Crostini.
 * @param {!Entry} entry
 * @param {boolean} persist If path is to be persisted.
 * @param {!VolumeManager} volumeManager
 */
Crostini.canSharePath = function(entry, persist, volumeManager) {

  // Check crostini-files flag and valid volume.
  if (!Crostini.IS_CROSTINI_FILES_ENABLED)
    return false;

  // Root of volume not allowed.
  if (entry.fullPath === '/')
    return false;

  // Only directories for persistent shares.
  if (persist && !entry.isDirectory)
    return false;

  // Allow Downloads, and Drive if DriveFS is enabled.
  const rootType = volumeManager.getLocationInfo(entry).rootType;
  return rootType === VolumeManagerCommon.RootType.DOWNLOADS ||
      (loadTimeData.getBoolean('DRIVE_FS_ENABLED') &&
       Crostini.VALID_ROOT_TYPES_FOR_SHARE.has(rootType));
};

/**
 * Returns true if task requires entries to be shared before executing task.
 * @param {!chrome.fileManagerPrivate.FileTask} task Task to run.
 * @return {boolean} true if task requires entries to be shared.
 */
Crostini.taskRequiresSharing = function(task) {
  const taskParts = task.taskId.split('|');
  const taskType = taskParts[1];
  const actionId = taskParts[2];
  return taskType === 'crostini' || actionId === 'install-linux-package';
};
