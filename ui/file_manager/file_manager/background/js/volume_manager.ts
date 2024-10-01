// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import {getRootType, isComputersRoot, isFakeEntry, isOneDrivePlaceholder, isSameEntry, isSameFileSystem, isTeamDriveRoot} from '../../common/js/entry_utils.js';
import type {FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {type CustomEventMap, FilesEventTarget} from '../../common/js/files_event_target.js';
import {str} from '../../common/js/translations.js';
import {debug, promisify, timeoutPromise} from '../../common/js/util.js';
import type {FileSystemType, Source} from '../../common/js/volume_manager_types.js';
import {COMPUTERS_DIRECTORY_PATH, getMediaViewRootTypeFromVolumeId, getRootTypeFromVolumeType, MediaViewRootType, RootType, SHARED_DRIVES_DIRECTORY_PATH, VolumeError, VolumeType} from '../../common/js/volume_manager_types.js';
import {addVolume, removeVolume} from '../../state/ducks/volumes.js';
import {getStore} from '../../state/store.js';

import {EntryLocation} from './entry_location_impl.js';
import {VolumeInfo} from './volume_info.js';
import {VolumeInfoList} from './volume_info_list.js';

/**
 * Time in milliseconds that we wait a response for general volume operations
 * such as mount, unmount, and requestFileSystem. If no response on
 * mount/unmount received the request supposed failed.
 */
const TIMEOUT = 15 * 60 * 1000;

const TIMEOUT_STR_REQUEST_FILE_SYSTEM = 'timeout(requestFileSystem)';

/**
 * A list of RequestType
 */
enum RequestType {
  MOUNT = 'mount',
  UNMOUNT = 'unmount',
}

/**
 * Logs a warning message if the given error is not in
 * VolumeError.
 *
 * @param error Status string usually received from APIs.
 */
function validateError(error: string) {
  const found = Object.values(VolumeError).find(value => value === error);
  if (found) {
    return;
  }

  console.warn(`Invalid mount error: ${error}`);
}

/**
 * Builds the VolumeInfo data from chrome.fileManagerPrivate.VolumeMetadata.
 * @param volumeMetadata Metadata instance for the volume.
 * @return Promise settled with the VolumeInfo instance.
 */
export async function createVolumeInfo(
    volumeMetadata: chrome.fileManagerPrivate.VolumeMetadata):
    Promise<VolumeInfo> {
  let localizedLabel: string;
  switch (volumeMetadata.volumeType) {
    case VolumeType.DOWNLOADS:
      localizedLabel = str('MY_FILES_ROOT_LABEL');
      break;
    case VolumeType.DRIVE:
      localizedLabel = str('DRIVE_DIRECTORY_LABEL');
      break;
    case VolumeType.MEDIA_VIEW:
      switch (getMediaViewRootTypeFromVolumeId(volumeMetadata.volumeId)) {
        case MediaViewRootType.IMAGES:
          localizedLabel = str('MEDIA_VIEW_IMAGES_ROOT_LABEL');
          break;
        case MediaViewRootType.VIDEOS:
          localizedLabel = str('MEDIA_VIEW_VIDEOS_ROOT_LABEL');
          break;
        case MediaViewRootType.AUDIO:
          localizedLabel = str('MEDIA_VIEW_AUDIO_ROOT_LABEL');
          break;
      }
      break;
    case VolumeType.CROSTINI:
      localizedLabel = str('LINUX_FILES_ROOT_LABEL');
      break;
    case VolumeType.ANDROID_FILES:
      localizedLabel = str('ANDROID_FILES_ROOT_LABEL');
      break;
    default:
      // TODO(mtomasz): Calculate volumeLabel for all types of volumes in the
      // C++ layer.
      localizedLabel = volumeMetadata.volumeLabel ||
          volumeMetadata.volumeId.split(':', 2)[1]!;
      break;
  }

  debug(`Getting file system '${volumeMetadata.volumeId}'`);
  return timeoutPromise(
             new Promise<DirectoryEntry>((resolve, reject) => {
               chrome.fileManagerPrivate.getVolumeRoot(
                   {
                     volumeId: volumeMetadata.volumeId,
                     writable: !volumeMetadata.isReadOnly,
                   },
                   (rootDirectoryEntry: DirectoryEntry) => {
                     if (chrome.runtime.lastError) {
                       reject(chrome.runtime.lastError.message);
                     } else {
                       resolve(rootDirectoryEntry);
                     }
                   });
             }),
             TIMEOUT,
             TIMEOUT_STR_REQUEST_FILE_SYSTEM + ': ' + volumeMetadata.volumeId)
      .then(rootDirectoryEntry => {
        debug(`Got file system '${volumeMetadata.volumeId}'`);
        return new VolumeInfo(
            volumeMetadata.volumeType as VolumeType, volumeMetadata.volumeId,
            rootDirectoryEntry.filesystem, volumeMetadata.mountCondition,
            volumeMetadata.deviceType, volumeMetadata.devicePath,
            volumeMetadata.isReadOnly, volumeMetadata.isReadOnlyRemovableDevice,
            volumeMetadata.profile, localizedLabel, volumeMetadata.providerId,
            volumeMetadata.configurable, volumeMetadata.watchable,
            volumeMetadata.source as Source,
            volumeMetadata.diskFileSystemType as FileSystemType,
            volumeMetadata.iconSet, volumeMetadata.driveLabel,
            volumeMetadata.remoteMountPath, volumeMetadata.vmType);
      })
      .then(async (volumeInfo) => {
        // resolveDisplayRoot() is a promise, but instead of using await here,
        // we just pass a onSuccess function to it, because we don't want to it
        // to interfere the startup time.
        volumeInfo.resolveDisplayRoot(() => {
          getStore().dispatch(addVolume(volumeInfo, volumeMetadata));
        });
        return volumeInfo;
      })
      .catch(error => {
        console.warn(`Cannot mount file system '${volumeMetadata.volumeId}': ${
            error.stack || error}`);

        // TODO(crbug.com/41391739): Report a mount error via UMA.

        throw error;
      });
}


type RequestSuccessCallback = (volumeInfo?: VolumeInfo) => void;
type RequestErrorCallback = (error: VolumeError) => void;
interface Request {
  successCallbacks: RequestSuccessCallback[];
  errorCallbacks: RequestErrorCallback[];
  timeout: number;
}

export type DeviceConnectionChangedEvent = CustomEvent<undefined>&{
  type: 'drive-connection-changed',
};

/**
 * An event triggered when a user tries to mount the volume which is
 * already mounted. The event object must have a volumeId property.
 */
export type VolumeAlreadyMountedEvent = CustomEvent<{
  volumeId: string,
}>&{
  type: 'volume_already_mounted',
};

/**
 * An event triggered when an archive file is newly mounted, or when opened a
 * one already mounted.
 */
export type ArchiveOpenEvent = CustomEvent<{
  mountPoint: DirectoryEntry,
}>&{
  type: 'archive_opened',
};

/**
 * Event object which is dispatched with 'externally-unmounted' event.
 */
export type ExternallyUnmountedEvent = CustomEvent<VolumeInfo>&{
  type: 'externally-unmounted',
};

export interface VolumeManagerEventMap extends CustomEventMap {
  'drive-connection-changed': DeviceConnectionChangedEvent;
  'volume_already_mounted': VolumeAlreadyMountedEvent;
  'archive_opened': ArchiveOpenEvent;
  'externally-unmounted': ExternallyUnmountedEvent;
}

/**
 * VolumeManager is responsible for tracking list of mounted volumes.
 */
export class VolumeManager extends FilesEventTarget<VolumeManagerEventMap> {
  /**
   * The list of VolumeInfo instances for each mounted volume.
   */
  volumeInfoList = new VolumeInfoList();

  /**
   * The list of archives requested to mount. We will show contents once
   * archive is mounted, but only for mounts from within this filebrowser tab.
   */
  private requests_: Record<string, Request> = {};

  // The status should be merged into VolumeManager.
  // TODO(hidehiko): Remove them after the migration.
  /**
   * Connection state of the Drive.
   */
  private driveConnectionState_:
      chrome.fileManagerPrivate.DriveConnectionState = {
    type: chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
    reason: chrome.fileManagerPrivate.DriveOfflineReason.NO_SERVICE,
  };

  /**
   * Holds the resolver for the `waitForInitialization_` promise.
   */
  private finishInitialization_: (() => void)|null = null;

  /**
   * Promise used to wait for the initialize() method to finish.
   */
  private waitForInitialization_: Promise<void> =
      new Promise(resolve => this.finishInitialization_ = resolve);

  constructor(
      private createVolumeInfo_: typeof createVolumeInfo = createVolumeInfo) {
    super();

    chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(
        this.onDriveConnectionStatusChanged_.bind(this));
    this.onDriveConnectionStatusChanged_();

    // Subscribe to mount event as early as possible, but after the
    // waitForInitialization_ above.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
  }

  /**
   * Gets the 'fusebox-only' filter state: true if enabled, false if disabled.
   * The filter is only enabled by the SelectFileAsh (Lacros) file picker, and
   * implemented by {FilteredVolumeManager} override.
   */
  getFuseBoxOnlyFilterEnabled(): boolean {
    return false;
  }

  /**
   * Gets the 'media-store-files-only' filter state: true if enabled, false if
   * disabled. The filter is only enabled by the Android (ARC) file picker, and
   * implemented by {FilteredVolumeManager} override.
   */
  getMediaStoreFilesOnlyFilterEnabled(): boolean {
    return false;
  }

  /**
   * Disposes the instance. After the invocation of this method, any other
   * method should not be called.
   */
  dispose(): void {}

  /**
   * Invoked when the drive connection status is changed.
   */
  private onDriveConnectionStatusChanged_() {
    chrome.fileManagerPrivate.getDriveConnectionState(state => {
      this.driveConnectionState_ = state;
      this.dispatchEvent(new CustomEvent('drive-connection-changed'));
    });
  }

  /**
   * Returns the drive connection state.
   */
  getDriveConnectionState(): chrome.fileManagerPrivate.DriveConnectionState {
    return this.driveConnectionState_;
  }

  /**
   * Adds new volume info from the given volumeMetadata. If the corresponding
   * volume info has already been added, the volumeMetadata is ignored.
   */
  private addVolumeInfo_(volumeInfo: VolumeInfo): VolumeInfo {
    const volumeType = volumeInfo.volumeType as VolumeType;
    if (this.volumeInfoList.findIndex(volumeInfo.volumeId) === -1) {
      this.volumeInfoList.add(volumeInfo);

      // Update the network connection status, because until the drive
      // is initialized, the status is set to not ready.
      // TODO(mtomasz): The connection status should be migrated into
      // chrome.fileManagerPrivate.VolumeMetadata.
      if (volumeType === VolumeType.DRIVE) {
        this.onDriveConnectionStatusChanged_();
      }
    } else if (volumeType === VolumeType.REMOVABLE) {
      // Update for remounted USB external storage, because they were
      // remounted to switch read-only policy.
      this.volumeInfoList.add(volumeInfo);
    }

    return volumeInfo;
  }

  /**
   * Initializes mount points.
   */
  async initialize(): Promise<void> {
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
      if (this.finishInitialization_) {
        this.finishInitialization_();
      }
    };

    try {
      console.warn('Getting volumes');
      let volumeMetadataList: chrome.fileManagerPrivate.VolumeMetadata[] =
          await promisify(chrome.fileManagerPrivate.getVolumeMetadataList);
      if (!volumeMetadataList) {
        console.warn('Cannot get volumes');
        finishInitialization();
        return;
      }
      volumeMetadataList = volumeMetadataList.filter(volume => !volume.hidden);
      debug(`There are ${volumeMetadataList.length} volumes`);

      let counter = 0;

      // Create VolumeInfo for each volume.
      volumeMetadataList.map(async (volumeMetadata, idx) => {
        const volumeId = volumeMetadata.volumeId;
        let volumeInfo = null;
        try {
          debug(`Initializing volume #${idx} '${volumeId}'`);
          // createVolumeInfo() requests the filesystem and resolve its root,
          // after that it only creates a VolumeInfo.
          volumeInfo = await this.createVolumeInfo_(volumeMetadata);
          // Add addVolumeInfo_() changes the VolumeInfoList which propagates
          // to the foreground.
          this.addVolumeInfo_(volumeInfo);
          debug(`Initialized volume #${idx} ${volumeId}'`);
        } catch (error) {
          console.warn(`Error initializing #${idx} ${volumeId}: ${error}`);
        } finally {
          counter += 1;
          // Finish after all volumes have been processed, or at least Downloads
          // or Drive.
          const isDriveOrDownloads = volumeInfo &&
              (volumeInfo.volumeType === VolumeType.DOWNLOADS ||
               volumeInfo.volumeType === VolumeType.DRIVE);
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
   */
  private async onMountCompleted_(
      event: chrome.fileManagerPrivate.MountCompletedEvent) {
    // Wait for the initialization to guarantee that the initialize() runs for
    // some volumes before any mount event, because the mounted volume can be
    // unresponsive, getting stuck when resolving the root in the method
    // createVolumeInfo(). crbug.com/504366
    await this.waitForInitialization_;

    const {eventType, status, volumeMetadata} = event;
    const {sourcePath = '', volumeId} = volumeMetadata;
    const volumeError = status as string as VolumeError;

    switch (eventType) {
      case 'mount': {
        const requestKey = this.makeRequestKey_(RequestType.MOUNT, sourcePath);

        switch (volumeError) {
          case VolumeError.SUCCESS:
          case VolumeError.UNKNOWN_FILESYSTEM:
          case VolumeError.UNSUPPORTED_FILESYSTEM: {
            debug(`Mounted '${sourcePath}' as '${volumeId}'`);
            if (volumeMetadata.hidden) {
              debug(`Mount discarded for hidden volume: '${volumeId}'`);
              this.finishRequest_(requestKey, volumeError);
              return;
            }

            let volumeInfo;
            try {
              volumeInfo = await this.createVolumeInfo_(volumeMetadata);
            } catch (error: any) {
              console.warn(
                  'Unable to create volumeInfo for ' +
                  `${volumeId} mounted on ${sourcePath}.` +
                  `Mount status: ${volumeError}. Error: ${
                      error.stack || error}.`);
              this.finishRequest_(requestKey, volumeError);
              return;
            }
            this.addVolumeInfo_(volumeInfo);
            this.finishRequest_(requestKey, volumeError, volumeInfo);
            return;
          }

          case VolumeError.PATH_ALREADY_MOUNTED: {
            console.warn(
                `Cannot mount (redacted): Already mounted as '${volumeId}'`);
            debug(`Cannot mount '${sourcePath}': Already mounted as '${
                volumeId}'`);
            const navigationEvent =
                new CustomEvent('volume_already_mounted', {detail: {volumeId}});
            this.dispatchEvent(navigationEvent);
            this.finishRequest_(requestKey, volumeError);
            return;
          }

          case VolumeError.NEED_PASSWORD:
          case VolumeError.CANCELLED:
          default:
            console.warn('Cannot mount (redacted):', volumeError);
            debug(`Cannot mount '${sourcePath}':`, volumeError);
            this.finishRequest_(requestKey, volumeError);
            return;
        }
      }

      case 'unmount': {
        const requestKey = this.makeRequestKey_(RequestType.UNMOUNT, volumeId);
        const volumeInfoIndex = this.volumeInfoList.findIndex(volumeId);
        const volumeInfo = volumeInfoIndex !== -1 ?
            this.volumeInfoList.item(volumeInfoIndex) :
            null;

        switch (volumeError) {
          case VolumeError.SUCCESS: {
            const requested = requestKey in this.requests_;
            if (!requested && volumeInfo) {
              debug(`Unmounted '${volumeId}' without request`);
              this.dispatchEvent(new CustomEvent(
                  'externally-unmounted', {detail: volumeInfo}));
            } else {
              debug(`Unmounted '${volumeId}'`);
            }
            getStore().dispatch(removeVolume(volumeId));
            this.volumeInfoList.remove(volumeId);
            this.finishRequest_(requestKey, volumeError);
            return;
          }

          default:
            console.warn('Cannot unmount (redacted):', volumeError);
            debug(`Cannot unmount '${volumeId}':`, volumeError);
            this.finishRequest_(requestKey, volumeError);
            return;
        }
      }
    }
  }

  /**
   * Creates string to match mount events with requests.
   * @param requestType 'mount' | 'unmount'.
   * @param argument Argument describing the request, eg. source file
   *     path of the archive to be mounted, or a volumeId for unmounting.
   * @return Key for |this.requests_|.
   */
  private makeRequestKey_(requestType: RequestType, argument: string): string {
    return requestType + ':' + argument;
  }

  /**
   * @param fileUrl File url to the archive file.
   * @param password Password to decrypt archive file.
   * @return Fulfilled on success, otherwise rejected with a VolumeError.
   */
  async mountArchive(fileUrl: string, password?: string): Promise<VolumeInfo> {
    const path: string =
        await promisify(chrome.fileManagerPrivate.addMount, fileUrl, password);
    debug(`Mounting '${path}'`);
    const key = this.makeRequestKey_(RequestType.MOUNT, path);
    return this.startRequest_(key);
  }

  /**
   * Cancels mounting an archive.
   * @param fileUrl File url to the archive file.
   * @return Fulfilled on success, otherwise rejected with a VolumeError.
   */
  async cancelMounting(fileUrl: string): Promise<void> {
    debug(`Cancelling mounting archive at '${fileUrl}'`);
    return promisify(chrome.fileManagerPrivate.cancelMounting, fileUrl);
  }

  /**
   * Unmounts a volume.
   * @param volumeInfo Volume to be unmounted.
   * @return Fulfilled on success, otherwise rejected with a VolumeError.
   */
  async unmount({volumeId}: VolumeInfo): Promise<void> {
    debug(`Unmounting '${volumeId}'`);
    const key = this.makeRequestKey_(RequestType.UNMOUNT, volumeId);
    const request = this.startRequest_(key);
    await promisify(chrome.fileManagerPrivate.removeMount, volumeId);
    await request;
  }


  /**
   * Configures a volume.
   * @param volumeInfo Volume to be configured.
   * @return Fulfilled on success, otherwise rejected with an error message.
   */
  configure(volumeInfo: VolumeInfo): Promise<void> {
    return promisify(
        chrome.fileManagerPrivate.configureVolume, volumeInfo.volumeId);
  }

  /**
   * Obtains a volume info containing the passed entry.
   * @param entry Entry on the volume to be returned. Can be fake.
   */
  getVolumeInfo(entry: Entry|FilesAppEntry): VolumeInfo|null {
    if (!entry) {
      console.warn(`Invalid entry passed to getVolumeInfo: ${entry}`);
      return null;
    }

    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.fileSystem &&
          isSameFileSystem(volumeInfo.fileSystem, entry.filesystem)) {
        return volumeInfo;
      }
      // Additionally, check fake entries.
      for (const fakeEntry of Object.values(volumeInfo.fakeEntries)) {
        if (isSameEntry(fakeEntry, entry)) {
          return volumeInfo;
        }
      }
    }

    return null;
  }

  /**
   * Obtains volume information of the current profile.
   */
  getCurrentProfileVolumeInfo(volumeType: VolumeType): VolumeInfo|null {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.profile.isCurrentProfile &&
          volumeInfo.volumeType === volumeType) {
        return volumeInfo;
      }
    }
    return null;
  }

  /**
   * Obtains location information from an entry.
   * @param entry File or directory entry. It can be a fake entry.
   */
  getLocationInfo(entry: Entry|FilesAppEntry): EntryLocation|null {
    if (!entry) {
      console.warn(`Invalid entry passed to getLocationInfo: ${entry}`);
      return null;
    }

    const volumeInfo = this.getVolumeInfo(entry);

    if (isFakeEntry(entry)) {
      const rootType = getRootType(entry);
      assert(rootType);

      // Aggregated views like RECENTS and TRASH exist as fake entries but may
      // actually defer their logic to some underlying implementation or
      // delegate to the location filesystem.
      let isReadOnly = true;
      if (rootType === RootType.RECENT || rootType === RootType.TRASH ||
          (isOneDrivePlaceholder(entry))) {
        isReadOnly = false;
      }
      return new EntryLocation(
          volumeInfo, rootType, true /* The entry points a root directory. */,
          isReadOnly);
    }

    if (!volumeInfo) {
      return null;
    }

    let rootType;
    let isReadOnly;
    let isRootEntry;
    if (volumeInfo.volumeType === VolumeType.DRIVE) {
      // For Drive, the roots are /root, /team_drives, /Computers and /other,
      // instead of /. Root URLs contain trailing slashes.
      if (entry.fullPath === '/root' ||
          entry.fullPath.indexOf('/root/') === 0) {
        rootType = RootType.DRIVE;
        isReadOnly = volumeInfo.isReadOnly;
        isRootEntry = entry.fullPath === '/root';
      } else if (
          entry.fullPath === SHARED_DRIVES_DIRECTORY_PATH ||
          entry.fullPath.indexOf(SHARED_DRIVES_DIRECTORY_PATH + '/') === 0) {
        if (entry.fullPath === SHARED_DRIVES_DIRECTORY_PATH) {
          rootType = RootType.SHARED_DRIVES_GRAND_ROOT;
          isReadOnly = true;
          isRootEntry = true;
        } else {
          rootType = RootType.SHARED_DRIVE;
          if (isTeamDriveRoot(entry)) {
            isReadOnly = false;
            isRootEntry = true;
          } else {
            // Regular files/directories under Shared Drives.
            isRootEntry = false;
            isReadOnly = volumeInfo.isReadOnly;
          }
        }
      } else if (
          entry.fullPath === COMPUTERS_DIRECTORY_PATH ||
          entry.fullPath.indexOf(COMPUTERS_DIRECTORY_PATH + '/') === 0) {
        if (entry.fullPath === COMPUTERS_DIRECTORY_PATH) {
          rootType = RootType.COMPUTERS_GRAND_ROOT;
          isReadOnly = true;
          isRootEntry = true;
        } else {
          rootType = RootType.COMPUTER;
          if (isComputersRoot(entry)) {
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
        rootType = RootType.DRIVE_SHARED_WITH_ME;

        // /.files-by-id/<id> is read-only, but /.files-by-id/<id>/foo is
        // read-write.
        isReadOnly = entry.fullPath.split('/').length < 4;
        isRootEntry = entry.fullPath === '/.files-by-id';
      } else if (
          entry.fullPath === '/.shortcut-targets-by-id' ||
          entry.fullPath.indexOf('/.shortcut-targets-by-id/') === 0) {
        rootType = RootType.DRIVE_SHARED_WITH_ME;

        // /.shortcut-targets-by-id/<id> is read-only, but
        // /.shortcut-targets-by-id/<id>/foo is read-write.
        isReadOnly = entry.fullPath.split('/').length < 4;
        isRootEntry = entry.fullPath === '/.shortcut-targets-by-id';
      } else if (
          entry.fullPath === '/.Trash-1000' ||
          entry.fullPath.indexOf('/.Trash-1000/') === 0) {
        // Drive uses "$topdir/.Trash-$uid" as the trash dir as per XDG spec.
        // User chronos is always uid 1000.
        rootType = RootType.TRASH;
        isReadOnly = false;
        isRootEntry = entry.fullPath === '/.Trash-1000';
      } else {
        // Accessing Drive files outside of /drive/root and /drive/other is not
        // allowed, but can happen. Therefore returning null.
        return null;
      }
    } else {
      assert(volumeInfo.volumeType);
      rootType = getRootTypeFromVolumeType(volumeInfo.volumeType);
      isRootEntry = isSameEntry(entry, volumeInfo.fileSystem.root);
      // Although "Play files" root directory is writable in file system level,
      // we prohibit write operations on it in the UI level to avoid confusion.
      // Users can still have write access in sub directories like
      // /Play files/Pictures, /Play files/DCIM, etc...
      if (volumeInfo.volumeType === VolumeType.ANDROID_FILES && isRootEntry) {
        isReadOnly = true;
      } else {
        isReadOnly = volumeInfo.isReadOnly;
      }
    }

    return new EntryLocation(volumeInfo, rootType, isRootEntry, isReadOnly);
  }


  /**
   * Searches the information of the volume that exists on the given device
   * path.
   * @param devicePath Path of the device to search.
   * @return The volume's information, or null if not found.
   */
  findByDevicePath(devicePath: string): VolumeInfo|null {
    for (let i = 0; i < this.volumeInfoList.length; i++) {
      const volumeInfo = this.volumeInfoList.item(i);
      if (volumeInfo.devicePath && volumeInfo.devicePath === devicePath) {
        return volumeInfo;
      }
    }
    return null;
  }

  /**
   * Returns a promise that will be resolved when volume info, identified by
   * `volumeId` is created.
   * @return Resolved with the `VolumeInfo`. It won't resolve if the volume is
   *     never mounted.
   */
  whenVolumeInfoReady(volumeId: string): Promise<VolumeInfo> {
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

  /**
   * Obtains the default display root entry.
   * @returns Default display root promise, fulfilled when resolved
   *     successfully.
   */
  async getDefaultDisplayRoot(): Promise<DirectoryEntry|FilesAppDirEntry|null> {
    console.warn('Unexpected call to VolumeManager.getDefaultDisplayRoot');
    return null;
  }

  /**
   * @param key Key produced by |makeRequestKey_|.
   * @return Fulfilled on success, otherwise rejected with a
   *     VolumeError.
   */
  private startRequest_(key: string): Promise<VolumeInfo> {
    return new Promise((successCallback, errorCallback) => {
      if (key in this.requests_) {
        const request = this.requests_[key]!;
        request.successCallbacks.push(
            successCallback as RequestSuccessCallback);
        request.errorCallbacks.push(errorCallback);
      } else {
        this.requests_[key] = {
          successCallbacks: [successCallback as RequestSuccessCallback],
          errorCallbacks: [errorCallback],

          timeout: setTimeout(this.onTimeout_.bind(this, key), TIMEOUT),
        };
      }
    });
  }

  /**
   * Called if no response received in |TIMEOUT|.
   * @param key Key produced by |makeRequestKey_|.
   */
  private onTimeout_(key: string) {
    this.invokeRequestCallbacks_(this.requests_[key]!, VolumeError.TIMEOUT);
    delete this.requests_[key];
  }

  /**
   * @param key Key produced by |makeRequestKey_|.
   * @param status Status received from the API.
   * @param volumeInfo Volume info of the mounted volume.
   */
  private finishRequest_(
      key: string, status: VolumeError, volumeInfo?: VolumeInfo) {
    const request = this.requests_[key];
    if (!request) {
      return;
    }

    clearTimeout(request.timeout);
    this.invokeRequestCallbacks_(request, status, volumeInfo);
    delete this.requests_[key];
  }

  /**
   * @param request Structure created in |startRequest_|.
   * @param status If status === 'success' success callbacks are called.
   * @param volumeInfo Volume info of the mounted volume.
   */
  private invokeRequestCallbacks_(
      request: Request, status: VolumeError, volumeInfo?: VolumeInfo) {
    if (status === VolumeError.SUCCESS) {
      request.successCallbacks.map(cb => cb(volumeInfo));

    } else {
      validateError(status);
      request.errorCallbacks.map(cb => cb(status));
    }
  }

  /**
   * Checks if any volumes are disabled for selection.
   * See overridden implementation in `FilteredVolumeManager`.
   */
  hasDisabledVolumes(): boolean {
    return false;
  }

  /**
   * Checks whether the given volume is disabled for selection.
   * See overridden implementation in `FilteredVolumeManager`.
   * @param volume Volume to check.
   */
  isDisabled(_volume: VolumeType): boolean {
    return false;
  }

  /**
   * Checks if a volume is allowed.
   * See overridden implementation in `FilteredVolumeManager`.
   */
  isAllowedVolume(_volumeInfo: VolumeInfo): boolean {
    return true;
  }
}
