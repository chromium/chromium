// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {str} from '../../common/js/translations.js';
import {timeoutPromise} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {addVolume} from '../../state/ducks/volumes.js';
import {getStore} from '../../state/store.js';

import {VolumeInfoImpl} from './volume_info_impl.js';

/**
 * Utilities for volume manager implementation.
 */
const volumeManagerUtil = {};

/**
 * Time in milliseconds that we wait a response for general volume operations
 * such as mount, unmount, and requestFileSystem. If no response on
 * mount/unmount received the request supposed failed.
 * @const @type {number}
 */
volumeManagerUtil.TIMEOUT = 15 * 60 * 1000;

/**
 * Time in milliseconds that we wait a response for
 * chrome.fileManagerPrivate.resolveIsolatedEntries.
 * @const @type {number}
 */
volumeManagerUtil.TIMEOUT_FOR_RESOLVE_ISOLATED_ENTRIES = 1 * 60 * 1000;

/**
 * @const @type {string}
 */
volumeManagerUtil.TIMEOUT_STR_REQUEST_FILE_SYSTEM =
    'timeout(requestFileSystem)';

/**
 * @const @type {string}
 */
volumeManagerUtil.TIMEOUT_STR_RESOLVE_ISOLATED_ENTRIES =
    'timeout(resolveIsolatedEntries)';

/**
 * Logs a warning message if the given error is not in
 * VolumeManagerCommon.VolumeError.
 *
 * @param {string} error Status string usually received from APIs.
 */
volumeManagerUtil.validateError = error => {
  for (const key in VolumeManagerCommon.VolumeError) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{ TIMEOUT:
    // string; UNKNOWN_ERROR: string; INTERNAL_ERROR: string; INVALID_ARGUMENT:
    // string; INVALID_PATH: string; PATH_ALREADY_MOUNTED: string;
    // PATH_NOT_MOUNTED: string; DIRECTORY_CREATION_FAILED: string; ... 9 more
    // ...; BUSY: string; }'.
    if (error === VolumeManagerCommon.VolumeError[key]) {
      return;
    }
  }

  console.warn(`Invalid mount error: ${error}`);
};

/**
 * Builds the VolumeInfo data from chrome.fileManagerPrivate.VolumeMetadata.
 * @param {chrome.fileManagerPrivate.VolumeMetadata} volumeMetadata Metadata
 * instance for the volume.
 * @return {!Promise<!import("../../externs/volume_info.js").VolumeInfo>}
 *     Promise settled with the VolumeInfo instance.
 */
volumeManagerUtil.createVolumeInfo = async volumeMetadata => {
  // @ts-ignore: error TS7034: Variable 'localizedLabel' implicitly has type
  // 'any' in some locations where its type cannot be determined.
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
  return timeoutPromise(
             new Promise((resolve, reject) => {
               chrome.fileManagerPrivate.getVolumeRoot(
                   {
                     volumeId: volumeMetadata.volumeId,
                     writable: !volumeMetadata.isReadOnly,
                   },
                   rootDirectoryEntry => {
                     if (chrome.runtime.lastError) {
                       reject(chrome.runtime.lastError.message);
                     } else {
                       resolve(rootDirectoryEntry);
                     }
                   });
             }),
             volumeManagerUtil.TIMEOUT,
             volumeManagerUtil.TIMEOUT_STR_REQUEST_FILE_SYSTEM + ': ' +
                 volumeMetadata.volumeId)
      .then(rootDirectoryEntry => {
        console.debug(`Got file system '${volumeMetadata.volumeId}'`);
        return new VolumeInfoImpl(
            /** @type {VolumeManagerCommon.VolumeType} */
            (volumeMetadata.volumeType), volumeMetadata.volumeId,
            rootDirectoryEntry.filesystem, volumeMetadata.mountCondition,
            volumeMetadata.deviceType, volumeMetadata.devicePath,
            volumeMetadata.isReadOnly, volumeMetadata.isReadOnlyRemovableDevice,
            // @ts-ignore: error TS7005: Variable 'localizedLabel' implicitly
            // has an 'any' type.
            volumeMetadata.profile, localizedLabel, volumeMetadata.providerId,
            volumeMetadata.hasMedia, volumeMetadata.configurable,
            volumeMetadata.watchable,
            /** @type {VolumeManagerCommon.Source} */
            (volumeMetadata.source),
            /** @type {VolumeManagerCommon.FileSystemType} */
            (volumeMetadata.diskFileSystemType), volumeMetadata.iconSet,
            volumeMetadata.driveLabel, volumeMetadata.remoteMountPath,
            volumeMetadata.vmType);
      })
      .then(async (volumeInfo) => {
        // resolveDisplayRoot() is a promise, but instead of using await here,
        // we just pass a onSuccess function to it, because we don't want to it
        // to interfere the startup time.
        volumeInfo.resolveDisplayRoot(() => {
          getStore().dispatch(addVolume({volumeMetadata, volumeInfo}));
        });
        return volumeInfo;
      })
      .catch(
          /** @param {*} error */
          error => {
            console.warn(`Cannot mount file system '${
                volumeMetadata.volumeId}': ${error.stack || error}`);

            // TODO(crbug/847729): Report a mount error via UMA.

            return new VolumeInfoImpl(
                /** @type {VolumeManagerCommon.VolumeType} */
                (volumeMetadata.volumeType), volumeMetadata.volumeId,
                // @ts-ignore: error TS2345: Argument of type 'null' is not
                // assignable to parameter of type 'FileSystem'.
                null,  // File system is not found.
                volumeMetadata.mountCondition, volumeMetadata.deviceType,
                volumeMetadata.devicePath, volumeMetadata.isReadOnly,
                volumeMetadata.isReadOnlyRemovableDevice,
                // @ts-ignore: error TS7005: Variable 'localizedLabel'
                // implicitly has an 'any' type.
                volumeMetadata.profile, localizedLabel,
                volumeMetadata.providerId, volumeMetadata.hasMedia,
                volumeMetadata.configurable, volumeMetadata.watchable,
                /** @type {VolumeManagerCommon.Source} */
                (volumeMetadata.source),
                /** @type {VolumeManagerCommon.FileSystemType} */
                (volumeMetadata.diskFileSystemType), volumeMetadata.iconSet,
                volumeMetadata.driveLabel, volumeMetadata.remoteMountPath,
                volumeMetadata.vmType);
          });
};

export {volumeManagerUtil};
