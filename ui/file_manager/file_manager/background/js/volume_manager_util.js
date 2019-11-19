// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Utilities for volume manager implementation.
 */
const volumeManagerUtil = {};

/**
 * Time in milliseconds that we wait a response for general volume operations
 * such as mount, unmount, and requestFileSystem. If no response on
 * mount/unmount received the request supposed failed.
 * @const {number}
 */
volumeManagerUtil.TIMEOUT = 15 * 60 * 1000;

/**
 * Time in milliseconds that we wait a response for
 * chrome.fileManagerPrivate.resolveIsolatedEntries.
 * @const {number}
 */
volumeManagerUtil.TIMEOUT_FOR_RESOLVE_ISOLATED_ENTRIES = 1 * 60 * 1000;

/**
 * @const {string}
 */
volumeManagerUtil.TIMEOUT_STR_REQUEST_FILE_SYSTEM =
    'timeout(requestFileSystem)';

/**
 * @const {string}
 */
volumeManagerUtil.TIMEOUT_STR_RESOLVE_ISOLATED_ENTRIES =
    'timeout(resolveIsolatedEntries)';

/**
 * Throws an Error when the given error is not in
 * VolumeManagerCommon.VolumeError.
 *
 * @param {string} error Status string usually received from APIs.
 */
volumeManagerUtil.validateError = error => {
  for (const key in VolumeManagerCommon.VolumeError) {
    if (error === VolumeManagerCommon.VolumeError[key]) {
      return;
    }
  }

  throw new Error('Invalid mount error: ' + error);
};

/**
 * Builds the VolumeInfo data from chrome.fileManagerPrivate.VolumeMetadata.
 * @param {chrome.fileManagerPrivate.VolumeMetadata} volumeMetadata Metadata
 * instance for the volume.
 * @return {!Promise<!VolumeInfo>} Promise settled with the VolumeInfo instance.
 */
volumeManagerUtil.createVolumeInfo = volumeMetadata => {
  let localizedLabel;
  switch (volumeMetadata.volumeType) {
    case VolumeManagerCommon.VolumeType.DOWNLOADS:
      localizedLabel = str('MY_FILES_ROOT_LABEL');
      break;
    case VolumeManagerCommon.VolumeType.DRIVE:
      localizedLabel = str('DRIVE_DIRECTORY_LABEL');
      break;
    case VolumeManagerCommon.VolumeType.MEDIA_VIEW:
      switch (VolumeManagerCommon.getMediaViewRootTypeFromVolumeId(
          volumeMetadata.volumeId)) {
        case VolumeManagerCommon.MediaViewRootType.IMAGES:
          localizedLabel = str('MEDIA_VIEW_IMAGES_ROOT_LABEL');
          break;
        case VolumeManagerCommon.MediaViewRootType.VIDEOS:
          localizedLabel = str('MEDIA_VIEW_VIDEOS_ROOT_LABEL');
          break;
        case VolumeManagerCommon.MediaViewRootType.AUDIO:
          localizedLabel = str('MEDIA_VIEW_AUDIO_ROOT_LABEL');
          break;
      }
      break;
    case VolumeManagerCommon.VolumeType.CROSTINI:
      localizedLabel = str('LINUX_FILES_ROOT_LABEL');
      break;
    case VolumeManagerCommon.VolumeType.ANDROID_FILES:
      localizedLabel = str('ANDROID_FILES_ROOT_LABEL');
      break;
    default:
      // TODO(mtomasz): Calculate volumeLabel for all types of volumes in the
      // C++ layer.
      localizedLabel = volumeMetadata.volumeLabel ||
          volumeMetadata.volumeId.split(':', 2)[1];
      break;
  }

  console.debug(`Getting file system '${volumeMetadata.volumeId}'`);
  return util
      .timeoutPromise(
          new Promise((resolve, reject) => {
            chrome.fileSystem.requestFileSystem(
                {
                  volumeId: volumeMetadata.volumeId,
                  writable: !volumeMetadata.isReadOnly
                },
                isolatedFileSystem => {
                  if (chrome.runtime.lastError) {
                    reject(chrome.runtime.lastError.message);
                  } else {
                    resolve(isolatedFileSystem);
                  }
                });
          }),
          volumeManagerUtil.TIMEOUT,
          volumeManagerUtil.TIMEOUT_STR_REQUEST_FILE_SYSTEM + ': ' +
              volumeMetadata.volumeId)
      .then(
          /** @param {!FileSystem} isolatedFileSystem */
          isolatedFileSystem => {
            // Since File System API works on isolated entries only, we need to
            // convert it back to external one.
            // TODO(mtomasz): Make Files app work on isolated entries.
            return util.timeoutPromise(
                new Promise((resolve, reject) => {
                  chrome.fileManagerPrivate.resolveIsolatedEntries(
                      [isolatedFileSystem.root], entries => {
                        if (chrome.runtime.lastError) {
                          reject(chrome.runtime.lastError.message);
                        } else if (!entries[0]) {
                          reject('Resolving for external context failed');
                        } else {
                          resolve(entries[0].filesystem);
                        }
                      });
                }),
                volumeManagerUtil.TIMEOUT_FOR_RESOLVE_ISOLATED_ENTRIES,
                volumeManagerUtil.TIMEOUT_STR_RESOLVE_ISOLATED_ENTRIES + ': ' +
                    volumeMetadata.volumeId);
          })
      .then(
          /** @param {!FileSystem} fileSystem */
          fileSystem => {
            console.debug(`Got file system '${volumeMetadata.volumeId}'`);
            if (volumeMetadata.volumeType ===
                VolumeManagerCommon.VolumeType.DRIVE) {
              // After file system is mounted, we "read" drive grand root
              // entry at first. This triggers full feed fetch on background.
              // Note: we don't need to handle errors here, because even if
              // it fails, accessing to some path later will just become
              // a fast-fetch and it re-triggers full-feed fetch.
              fileSystem.root.createReader().readEntries(
                  () => {/* do nothing */}, error => {
                    console.warn(
                        `Triggering full feed fetch has failed: ${error.name}`);
                  });
            }
            return new VolumeInfoImpl(
                /** @type {VolumeManagerCommon.VolumeType} */
                (volumeMetadata.volumeType), volumeMetadata.volumeId,
                fileSystem, volumeMetadata.mountCondition,
                volumeMetadata.deviceType, volumeMetadata.devicePath,
                volumeMetadata.isReadOnly,
                volumeMetadata.isReadOnlyRemovableDevice,
                volumeMetadata.profile, localizedLabel,
                volumeMetadata.providerId, volumeMetadata.hasMedia,
                volumeMetadata.configurable, volumeMetadata.watchable,
                /** @type {VolumeManagerCommon.Source} */
                (volumeMetadata.source),
                /** @type {VolumeManagerCommon.FileSystemType} */
                (volumeMetadata.diskFileSystemType), volumeMetadata.iconSet,
                (volumeMetadata.driveLabel));
          })
      .catch(
          /** @param {*} error */
          error => {
            console.error(`Cannot mount file system '${
                volumeMetadata.volumeId}': ${error.stack || error}`);

            // TODO(crbug/847729): Report a mount error via UMA.

            return new VolumeInfoImpl(
                /** @type {VolumeManagerCommon.VolumeType} */
                (volumeMetadata.volumeType), volumeMetadata.volumeId,
                null,  // File system is not found.
                volumeMetadata.mountCondition, volumeMetadata.deviceType,
                volumeMetadata.devicePath, volumeMetadata.isReadOnly,
                volumeMetadata.isReadOnlyRemovableDevice,
                volumeMetadata.profile, localizedLabel,
                volumeMetadata.providerId, volumeMetadata.hasMedia,
                volumeMetadata.configurable, volumeMetadata.watchable,
                /** @type {VolumeManagerCommon.Source} */
                (volumeMetadata.source),
                /** @type {VolumeManagerCommon.FileSystemType} */
                (volumeMetadata.diskFileSystemType), volumeMetadata.iconSet,
                (volumeMetadata.driveLabel));
          });
};
