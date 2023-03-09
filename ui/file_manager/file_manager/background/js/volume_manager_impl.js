// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {promisify} from '../../common/js/api.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {removeVolume} from '../../state/actions/volumes.js';
import {getStore} from '../../state/store.js';

import {EntryLocationImpl} from './entry_location_impl.js';
import {VolumeInfoListImpl} from './volume_info_list_impl.js';
import {volumeManagerUtil} from './volume_manager_util.js';

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 * @implements {VolumeManager}
 */
export class VolumeManagerImpl extends EventTarget {
  constructor() {
    super();

    /** @override */
    this.volumeInfoList = new VolumeInfoListImpl();

    /**
     * The list of archives requested to mount. We will show contents once
     * archive is mounted, but only for mounts from within this filebrowser tab.
     * @type {Object<Object>}
     * @private
     */
    this.requests_ = {};

    // The status should be merged into VolumeManager.
    // TODO(hidehiko): Remove them after the migration.
    /**
     * Connection state of the Drive.
     * @type {chrome.fileManagerPrivate.DriveConnectionState}
     * @private
     */
    this.driveConnectionState_ = {
      type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
      reason: chrome.fileManagerPrivate.DriveOfflineReason.NO_SERVICE,
      hasCellularNetworkAccess: false,
      canPinHostedFiles: false,
    };

    chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(
        this.onDriveConnectionStatusChanged_.bind(this));
    this.onDriveConnectionStatusChanged_();

    /**
     * Holds the resolver for the `waitForInitialization_` promise.
     * @private {null|function():void}
     */
    this.finishInitialization_ = null;

    /**
     * Promise used to wait for the initialize() method to finish.
     * @private {!Promise<void>}
     */
    this.waitForInitialization_ =
        new Promise(resolve => this.finishInitialization_ = resolve);

    // Subscribe to mount event as early as possible, but after the
    // waitForInitialization_ above.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
  }

  /** @override */
  getFuseBoxOnlyFilterEnabled() {
    return false;
  }

  /** @override */
  getMediaStoreFilesOnlyFilterEnabled() {
    return false;
  }

  /** @override */
  dispose() {}

  /**
   * Invoked when the drive connection status is changed.
   * @private
   */
  onDriveConnectionStatusChanged_() {
    chrome.fileManagerPrivate.getDriveConnectionState(state => {
      this.driveConnectionState_ = state;
      dispatchSimpleEvent(this, 'drive-connection-changed');
    });
  }

  /** @override */
  getDriveConnectionState() {
    return this.driveConnectionState_;
  }

  /**
   * Adds new volume info from the given volumeMetadata. If the corresponding
   * volume info has already been added, the volumeMetadata is ignored.
   * @param {!VolumeInfo} volumeInfo
   * @return {!VolumeInfo}
   * @private
   */
  addVolumeInfo_(volumeInfo) {
    const volumeType = volumeInfo.volumeType;

    // We don't show Downloads and Drive on volume list if they have
    // mount error, since users can do nothing in this situation. We
    // show Removable and Provided volumes regardless of mount error
    // so that users can unmount or format the volume.
    // TODO(fukino): Once the Files app gets ready, show erroneous
    // Drive volume so that users can see auth warning banner on the
    // volume. crbug.com/517772.
    let shouldShow = true;
    switch (volumeType) {
      case VolumeManagerCommon.VolumeType.DOWNLOADS:
      case VolumeManagerCommon.VolumeType.DRIVE:
        shouldShow = !!volumeInfo.fileSystem;
        break;
    }

    if (!shouldShow) {
      return volumeInfo;
    }

    if (this.volumeInfoList.findIndex(volumeInfo.volumeId) === -1) {
      this.volumeInfoList.add(volumeInfo);

      // Update the network connection status, because until the drive
      // is initialized, the status is set to not ready.
      // TODO(mtomasz): The connection status should be migrated into
      // chrome.fileManagerPrivate.VolumeMetadata.
      if (volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
        this.onDriveConnectionStatusChanged_();
      }
    } else if (volumeType === VolumeManagerCommon.VolumeType.REMOVABLE) {
      // Update for remounted USB external storage, because they were
      // remounted to switch read-only policy.
      this.volumeInfoList.add(volumeInfo);
    }

    return volumeInfo;
  }

  /**
   * Initializes mount points.
   * @return {!Promise<void>}
   */
  async initialize() {
    let finished = false;
    /**
     * Resolves the initialization promise to unblock any code awaiting for
     * it.
     */
    const finishInitialization = () => {
      if (finished) {
        return;
      }
      finished = true;
      console.warn('Volumes initialization finished');
      this.finishInitialization_();
    };

    try {
      console.warn('Getting volumes');
      let volumeMetadataList =
          await promisify(chrome.fileManagerPrivate.getVolumeMetadataList);
      if (!volumeMetadataList) {
        console.warn('Cannot get volumes');
        finishInitialization();
        return;
      }
      volumeMetadataList = volumeMetadataList.filter(volume => !volume.hidden);
      console.debug(`There are ${volumeMetadataList.length} volumes`);

      let counter = 0;

      // Create VolumeInfo for each volume.
      volumeMetadataList.map(async (volumeMetadata, idx) => {
        const volumeId = volumeMetadata.volumeId;
        let volumeInfo = null;
        try {
          console.debug(`Initializing volume #${idx} '${volumeId}'`);
          // createVolumeInfo() requests the filesystem and resolve its root,
          // after that it only creates a VolumeInfo.
          volumeInfo = await volumeManagerUtil.createVolumeInfo(volumeMetadata);
          // Add addVolumeInfo_() changes the VolumeInfoList which propagates
          // to the foreground.
          this.addVolumeInfo_(volumeInfo);
          console.debug(`Initialized volume #${idx} ${volumeId}'`);
        } catch (error) {
          console.warn(`Error initiliazing #${idx} ${volumeId}: ${error}`);
        } finally {
          counter += 1;
          // Finish after all volumes have been processed, or at least Downloads
          // or Drive.
          const isDriveOrDownloads = volumeInfo &&
              (volumeInfo.volumeType ==
                   VolumeManagerCommon.VolumeType.DOWNLOADS ||
               volumeInfo.volumeType == VolumeManagerCommon.VolumeType.DRIVE);
          if (counter === volumeMetadataList.length || isDriveOrDownloads) {
            finishInitialization();
          }
        }
      });

      // At this point the volumes are still initializing.
      console.warn(
          `Queued the initialization of all ` +
          `${volumeMetadataList.length} volumes`);

      if (volumeMetadataList.length === 0) {
        finishInitialization();
      }
    } catch (error) {
      finishInitialization();
      throw error;
    }
  }

  /**
   * Event handler called when some volume was mounted or unmounted.
   * @param {chrome.fileManagerPrivate.MountCompletedEvent} event
   * @private
   */
  async onMountCompleted_(event) {
    // Wait for the initialization to guarantee that the initialize() runs for
    // some volumes before any mount event, because the mounted volume can be
    // unresponsive, getting stuck when resolving the root in the method
    // createVolumeInfo(). crbug.com/504366
    await this.waitForInitialization_;

    const {eventType, status, volumeMetadata} = event;
    const {sourcePath = '', volumeId} = volumeMetadata;

    switch (eventType) {
      case 'mount': {
        const requestKey = this.makeRequestKey_('mount', sourcePath);

        switch (status) {
          case 'success':
          case VolumeManagerCommon.VolumeError.UNKNOWN_FILESYSTEM:
          case VolumeManagerCommon.VolumeError.UNSUPPORTED_FILESYSTEM: {
            console.debug(`Mounted '${sourcePath}' as '${volumeId}'`);
            if (volumeMetadata.hidden) {
              console.debug(`Mount discarded for hidden volume: '${volumeId}'`);
              this.finishRequest_(requestKey, status);
              return;
            }

            let volumeInfo;
            try {
              volumeInfo =
                  await volumeManagerUtil.createVolumeInfo(volumeMetadata);
            } catch (error) {
              console.warn(
                  'Unable to create volumeInfo for ' +
                  `${volumeId} mounted on ${sourcePath}.` +
                  `Mount status: ${status}. Error: ${error.stack || error}.`);
              this.finishRequest_(requestKey, status);
              throw (error);
            }
            this.addVolumeInfo_(volumeInfo);
            this.finishRequest_(requestKey, status, volumeInfo);
            return;
          }

          case VolumeManagerCommon.VolumeError.PATH_ALREADY_MOUNTED: {
            console.warn(
                `Cannot mount (redacted): Already mounted as '${volumeId}'`);
            console.debug(`Cannot mount '${sourcePath}': Already mounted as '${
                volumeId}'`);
            const navigationEvent =
                new Event(VolumeManagerCommon.VOLUME_ALREADY_MOUNTED);
            navigationEvent.volumeId = volumeId;
            this.dispatchEvent(navigationEvent);
            this.finishRequest_(requestKey, status);
            return;
          }

          case VolumeManagerCommon.VolumeError.NEED_PASSWORD:
          case VolumeManagerCommon.VolumeError.CANCELLED:
          default:
            console.warn('Cannot mount (redacted):', status);
            console.debug(`Cannot mount '${sourcePath}':`, status);
            this.finishRequest_(requestKey, status);
            return;
        }
      }

      case 'unmount': {
        const requestKey = this.makeRequestKey_('unmount', volumeId);
        const volumeInfoIndex = this.volumeInfoList.findIndex(volumeId);
        const volumeInfo = volumeInfoIndex !== -1 ?
            this.volumeInfoList.item(volumeInfoIndex) :
            null;

        switch (status) {
          case 'success': {
            const requested = requestKey in this.requests_;
            if (!requested && volumeInfo) {
              console.debug(`Unmounted '${volumeId}' without request`);
              this.dispatchEvent(new CustomEvent(
                  'externally-unmounted', {detail: volumeInfo}));
            } else {
              console.debug(`Unmounted '${volumeId}'`);
            }
            if (util.isFilesAppExperimental()) {
              getStore().dispatch(removeVolume({volumeId}));
            }
            this.volumeInfoList.remove(volumeId);
            this.finishRequest_(requestKey, status);
            return;
          }

          default:
            console.warn('Cannot unmount (redacted):', status);
            console.debug(`Cannot unmount '${volumeId}':`, status);
            this.finishRequest_(requestKey, status);
            return;
        }
      }
    }
  }

  /**
   * Creates string to match mount events with requests.
   * @param {string} requestType 'mount' | 'unmount'. TODO(hidehiko): Replace by
   *     enum.
   * @param {string} argument Argument describing the request, eg. source file
   *     path of the archive to be mounted, or a volumeId for unmounting.
   * @return {string} Key for |this.requests_|.
   * @private
   */
  makeRequestKey_(requestType, argument) {
    return requestType + ':' + argument;
  }

  /** @override */
  async mountArchive(fileUrl, password) {
    const path =
        await promisify(chrome.fileManagerPrivate.addMount, fileUrl, password);
    console.debug(`Mounting '${path}'`);
    const key = this.makeRequestKey_('mount', path);
    return this.startRequest_(key);
  }

  /** @override */
  async cancelMounting(fileUrl) {
    console.debug(`Cancelling mounting archive at '${fileUrl}'`);
    return promisify(chrome.fileManagerPrivate.cancelMounting, fileUrl);
  }

  /** @override */
  async unmount({volumeId}) {
    console.debug(`Unmounting '${volumeId}'`);
    const key = this.makeRequestKey_('unmount', volumeId);
    const request = this.startRequest_(key);
    await promisify(chrome.fileManagerPrivate.removeMount, volumeId);
    await request;
  }

  /** @override */
  configure(volumeInfo) {
    return promisify(
        chrome.fileManagerPrivate.configureVolume, volumeInfo.volumeId);
  }

  /** @override */
  getVolumeInfo(entry) {
    if (!entry) {
      console.warn(`Invalid entry passed to getVolumeInfo: ${entry}`);
      return null;
    }

    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.fileSystem &&
          util.isSameFileSystem(volumeInfo.fileSystem, entry.filesystem)) {
        return volumeInfo;
      }
      // Additionally, check fake entries.
      for (const key in volumeInfo.fakeEntries) {
        const fakeEntry = volumeInfo.fakeEntries[key];
        if (util.isSameEntry(fakeEntry, entry)) {
          return volumeInfo;
        }
      }
    }

    return null;
  }

  /** @override */
  getCurrentProfileVolumeInfo(volumeType) {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.profile.isCurrentProfile &&
          volumeInfo.volumeType === volumeType) {
        return volumeInfo;
      }
    }
    return null;
  }

  /** @override */
  getLocationInfo(entry) {
    if (!entry) {
      console.warn(`Invalid entry passed to getLocationInfo: ${entry}`);
      return null;
    }

    const volumeInfo = this.getVolumeInfo(entry);

    if (util.isFakeEntry(entry)) {
      // Aggregated views like RECENTS and TRASH exist as fake entries but may
      // actually defer their logic to some underlying implementation or
      // delegate to the location filesystem.
      let isReadOnly = true;
      if (entry.rootType === VolumeManagerCommon.RootType.RECENT ||
          entry.rootType === VolumeManagerCommon.RootType.TRASH) {
        isReadOnly = false;
      }
      return new EntryLocationImpl(
          volumeInfo, assert(entry.rootType),
          true /* The entry points a root directory. */, isReadOnly);
    }

    if (!volumeInfo) {
      return null;
    }

    let rootType;
    let isReadOnly;
    let isRootEntry;
    if (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
      // For Drive, the roots are /root, /team_drives, /Computers and /other,
      // instead of /. Root URLs contain trailing slashes.
      if (entry.fullPath == '/root' || entry.fullPath.indexOf('/root/') === 0) {
        rootType = VolumeManagerCommon.RootType.DRIVE;
        isReadOnly = volumeInfo.isReadOnly;
        isRootEntry = entry.fullPath === '/root';
      } else if (
          entry.fullPath == VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH ||
          entry.fullPath.indexOf(
              VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH + '/') === 0) {
        if (entry.fullPath ==
            VolumeManagerCommon.SHARED_DRIVES_DIRECTORY_PATH) {
          rootType = VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT;
          isReadOnly = true;
          isRootEntry = true;
        } else {
          rootType = VolumeManagerCommon.RootType.SHARED_DRIVE;
          if (util.isTeamDriveRoot(entry)) {
            isReadOnly = false;
            isRootEntry = true;
          } else {
            // Regular files/directories under Shared Drives.
            isRootEntry = false;
            isReadOnly = volumeInfo.isReadOnly;
          }
        }
      } else if (
          entry.fullPath == VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH ||
          entry.fullPath.indexOf(
              VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH + '/') === 0) {
        if (entry.fullPath == VolumeManagerCommon.COMPUTERS_DIRECTORY_PATH) {
          rootType = VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT;
          isReadOnly = true;
          isRootEntry = true;
        } else {
          rootType = VolumeManagerCommon.RootType.COMPUTER;
          if (util.isComputersRoot(entry)) {
            isReadOnly = true;
            isRootEntry = true;
          } else {
            // Regular files/directories under a Computer entry.
            isRootEntry = false;
            isReadOnly = volumeInfo.isReadOnly;
          }
        }
      } else if (
          entry.fullPath === '/.files-by-id' ||
          entry.fullPath.indexOf('/.files-by-id/') === 0) {
        rootType = VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME;

        // /.files-by-id/<id> is read-only, but /.files-by-id/<id>/foo is
        // read-write.
        isReadOnly = entry.fullPath.split('/').length < 4;
        isRootEntry = entry.fullPath === '/.files-by-id';
      } else if (
          entry.fullPath === '/.shortcut-targets-by-id' ||
          entry.fullPath.indexOf('/.shortcut-targets-by-id/') === 0) {
        rootType = VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME;

        // /.shortcut-targets-by-id/<id> is read-only, but
        // /.shortcut-targets-by-id/<id>/foo is read-write.
        isReadOnly = entry.fullPath.split('/').length < 4;
        isRootEntry = entry.fullPath === '/.shortcut-targets-by-id';
      } else if (
          entry.fullPath === '/.Trash-1000' ||
          entry.fullPath.indexOf('/.Trash-1000/') === 0) {
        // Drive uses "$topdir/.Trash-$uid" as the trash dir as per XDG spec.
        // User chronos is always uid 1000.
        rootType = VolumeManagerCommon.RootType.TRASH;
        isReadOnly = false;
        isRootEntry = entry.fullPath === '/.Trash-1000';
      } else {
        // Accessing Drive files outside of /drive/root and /drive/other is not
        // allowed, but can happen. Therefore returning null.
        return null;
      }
    } else {
      rootType = VolumeManagerCommon.getRootTypeFromVolumeType(
          assert(volumeInfo.volumeType));
      isRootEntry = util.isSameEntry(entry, volumeInfo.fileSystem.root);
      // Although "Play files" root directory is writable in file system level,
      // we prohibit write operations on it in the UI level to avoid confusion.
      // Users can still have write access in sub directories like
      // /Play files/Pictures, /Play files/DCIM, etc...
      if (volumeInfo.volumeType ==
              VolumeManagerCommon.VolumeType.ANDROID_FILES &&
          isRootEntry) {
        isReadOnly = true;
      } else {
        isReadOnly = volumeInfo.isReadOnly;
      }
    }

    return new EntryLocationImpl(volumeInfo, rootType, isRootEntry, isReadOnly);
  }

  /** @override */
  findByDevicePath(devicePath) {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.devicePath && volumeInfo.devicePath === devicePath) {
        return volumeInfo;
      }
    }
    return null;
  }

  /** @override */
  whenVolumeInfoReady(volumeId) {
    return new Promise((fulfill) => {
      const handler = () => {
        const index = this.volumeInfoList.findIndex(volumeId);
        if (index !== -1) {
          fulfill(this.volumeInfoList.item(index));
          this.volumeInfoList.removeEventListener('splice', handler);
        }
      };
      this.volumeInfoList.addEventListener('splice', handler);
      handler();
    });
  }

  /** @override */
  getDefaultDisplayRoot(callback) {
    console.warn('Unexpected call to VolumeManagerImpl.getDefaultDisplayRoot');
    callback(null);
  }

  /**
   * @param {string} key Key produced by |makeRequestKey_|.
   * @return {!Promise<!VolumeInfo>} Fulfilled on success, otherwise rejected
   *     with a VolumeManagerCommon.VolumeError.
   * @private
   */
  startRequest_(key) {
    return new Promise((successCallback, errorCallback) => {
      if (key in this.requests_) {
        const request = this.requests_[key];
        request.successCallbacks.push(successCallback);
        request.errorCallbacks.push(errorCallback);
      } else {
        this.requests_[key] = {
          successCallbacks: [successCallback],
          errorCallbacks: [errorCallback],

          timeout: setTimeout(
              this.onTimeout_.bind(this, key), volumeManagerUtil.TIMEOUT),
        };
      }
    });
  }

  /**
   * Called if no response received in |TIMEOUT|.
   * @param {string} key Key produced by |makeRequestKey_|.
   * @private
   */
  onTimeout_(key) {
    this.invokeRequestCallbacks_(
        this.requests_[key], VolumeManagerCommon.VolumeError.TIMEOUT);
    delete this.requests_[key];
  }

  /**
   * @param {string} key Key produced by |makeRequestKey_|.
   * @param {!VolumeManagerCommon.VolumeError|string} status Status received
   *     from the API.
   * @param {VolumeInfo=} opt_volumeInfo Volume info of the mounted volume.
   * @private
   */
  finishRequest_(key, status, opt_volumeInfo) {
    const request = this.requests_[key];
    if (!request) {
      return;
    }

    clearTimeout(request.timeout);
    this.invokeRequestCallbacks_(request, status, opt_volumeInfo);
    delete this.requests_[key];
  }

  /**
   * @param {Object} request Structure created in |startRequest_|.
   * @param {!VolumeManagerCommon.VolumeError|string} status If status ===
   *     'success' success callbacks are called.
   * @param {VolumeInfo=} opt_volumeInfo Volume info of the mounted volume.
   * @private
   */
  invokeRequestCallbacks_(request, status, opt_volumeInfo) {
    const callEach = (callbacks, self, args) => {
      for (let i = 0; i < callbacks.length; i++) {
        callbacks[i].apply(self, args);
      }
    };

    if (status === 'success') {
      callEach(request.successCallbacks, this, [opt_volumeInfo]);
    } else {
      volumeManagerUtil.validateError(status);
      callEach(request.errorCallbacks, this, [status]);
    }
  }

  /** @override */
  hasDisabledVolumes() {
    return false;
  }

  /** @override */
  isDisabled(volume) {
    return false;
  }
}
