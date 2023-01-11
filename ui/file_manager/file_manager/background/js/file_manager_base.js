// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {resolveIsolatedEntries} from '../../common/js/api.js';
import {FilesAppState} from '../../common/js/files_app_state.js';
import {metrics} from '../../common/js/metrics.js';
import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Crostini} from '../../externs/background/crostini.js';
import {DriveSyncHandler} from '../../externs/background/drive_sync_handler.js';
import {FileManagerBaseInterface} from '../../externs/background/file_manager_base.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {CrostiniImpl} from './crostini.js';
import {DriveSyncHandlerImpl} from './drive_sync_handler.js';
import {FileOperationHandler} from './file_operation_handler.js';
import {FileOperationManagerImpl} from './file_operation_manager.js';
import {fileOperationUtil} from './file_operation_util.js';
import {launcher} from './launcher.js';
import {ProgressCenterImpl} from './progress_center.js';
import {volumeManagerFactory} from './volume_manager_factory.js';

/**
 * Root class of the former background page.
 * @implements {FileManagerBaseInterface}
 */
export class FileManagerBase {
  constructor() {
    /**
     * Map of all currently open file dialogs. The key is an app ID.
     * @type {!Object<!Window>}
     */
    this.dialogs = {};

    /**
     * Initializes the strings. This needs for the volume manager.
     * @type {?Promise}
     */
    this.initializationPromise_ = new Promise((fulfill, reject) => {
      chrome.fileManagerPrivate.getStrings(stringData => {
        if (chrome.runtime.lastError) {
          console.error(chrome.runtime.lastError.message);
          return;
        }
        if (!loadTimeData.isInitialized()) {
          loadTimeData.data = assert(stringData);
        }
        fulfill(stringData);
      });
    });

    /**
     * Progress center of the background page.
     * @type {!ProgressCenter}
     */
    this.progressCenter = new ProgressCenterImpl();

    /**
     * File operation manager.
     * @type {FileOperationManager}
     */
    this.fileOperationManager = null;

    /**
     * Event handler for progress center.
     * @private {FileOperationHandler}
     */
    this.fileOperationHandler_ = null;

    /**
     * Drive sync handler.
     * @type {!DriveSyncHandler}
     */
    this.driveSyncHandler = new DriveSyncHandlerImpl(this.progressCenter);

    /** @type {!Crostini} */
    this.crostini = new CrostiniImpl();

    /**
     * String assets.
     * @type {Object<string>}
     */
    this.stringData = null;

    // Initialize string and volume manager related stuffs.
    this.initializationPromise_.then(strings => {
      this.stringData = strings;
      this.crostini.initEnabled();

      volumeManagerFactory.getInstance().then(volumeManager => {
        volumeManager.addEventListener(
            VolumeManagerCommon.VOLUME_ALREADY_MOUNTED,
            this.handleViewEvent_.bind(this));

        this.crostini.initVolumeManager(volumeManager);
      });

      this.fileOperationManager = new FileOperationManagerImpl();
      this.fileOperationHandler_ = new FileOperationHandler(
          this.fileOperationManager, this.progressCenter);
    });

    // Handle newly mounted FSP file systems. Workaround for crbug.com/456648.
    // TODO(mtomasz): Replace this hack with a proper solution.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));

    launcher.setInitializationPromise(this.initializationPromise_);
  }

  /**
   * @return {!Promise<!VolumeManager>}
   */
  async getVolumeManager() {
    return volumeManagerFactory.getInstance();
  }

  /**
   * Register callback to be invoked after initialization.
   * If the initialization is already done, the callback is invoked immediately.
   *
   * @param {function()} callback Initialize callback to be registered.
   */
  ready(callback) {
    this.initializationPromise_.then(callback);
  }

  /**
   * Forces File Operation Util to return error for automated tests.
   * @param {boolean} enable
   */
  forceFileOperationErrorForTest(enable) {
    fileOperationUtil.forceErrorForTest = enable;
  }

  /**
   * Registers dialog window to the background page.
   *
   * @param {!Window} dialogWindow Window of the dialog.
   */
  registerDialog(dialogWindow) {
    const id = DIALOG_ID_PREFIX + (nextFileManagerDialogID++);
    this.dialogs[id] = dialogWindow;
    if (window.IN_TEST) {
      dialogWindow.IN_TEST = true;
    }
    dialogWindow.addEventListener('pagehide', () => {
      delete this.dialogs[id];
    });
  }

  /**
   * Launches a new File Manager window.
   *
   * @param {!FilesAppState=} appState App state.
   * @return {!Promise<void>} Resolved when the new window is opened.
   */
  async launchFileManager(appState = {}) {
    return launcher.launchFileManager(appState);
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param {!Event} event An event with the volumeId or
   *     devicePath.
   * @private
   */
  handleViewEvent_(event) {
    util.doIfPrimaryContext(() => {
      this.handleViewEventInternal_(event);
    });
  }

  /**
   * @param {!Event} event An event with the volumeId or
   *     devicePath.
   * @private
   */
  handleViewEventInternal_(event) {
    volumeManagerFactory.getInstance().then(
        /**
         * Retrieves the root file entry of the volume on the requested
         * device.
         * @param {!VolumeManager} volumeManager
         */
        volumeManager => {
          if (event.devicePath) {
            const volume = volumeManager.findByDevicePath(event.devicePath);
            if (volume) {
              this.navigateToVolumeRoot_(volume);
            } else {
              console.warn(
                  `Got view event with invalid volume id: ${event.devicePath}`);
            }
          } else if (event.volumeId) {
            if (event.type === VolumeManagerCommon.VOLUME_ALREADY_MOUNTED) {
              this.navigateToVolumeInFocusedWindowWhenReady_(event.volumeId);
            } else {
              this.navigateToVolumeWhenReady_(event.volumeId);
            }
          } else {
            console.warn('Got view event with no actionable destination.');
          }
        });
  }

  /**
   * Retrieves the root file entry of the volume on the requested device.
   *
   * @param {!string} volumeId ID of the volume to navigate to.
   * @return {!Promise<!VolumeInfo>}
   * @private
   */
  retrieveVolumeInfo_(volumeId) {
    return volumeManagerFactory.getInstance().then(
        (/**
          * @param {!VolumeManager} volumeManager
          */
         (volumeManager) => {
           return volumeManager.whenVolumeInfoReady(volumeId).catch((e) => {
             console.warn(
                 'Unable to find volume for id: ' + volumeId +
                 '. Error: ' + e.message);
           });
         }));
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param {!string} volumeId ID of the volume to navigate to.
   * @param {!string=} opt_directoryPath Optional path to be opened.
   * @private
   */
  navigateToVolumeWhenReady_(volumeId, opt_directoryPath) {
    this.retrieveVolumeInfo_(volumeId).then(volume => {
      this.navigateToVolumeRoot_(volume, opt_directoryPath);
    });
  }

  /**
   * Opens the volume root (or opt directoryPath) in the main UI of the focused
   * window.
   *
   * @param {!string} volumeId ID of the volume to navigate to.
   * @param {!string=} opt_directoryPath Optional path to be opened.
   * @private
   */
  navigateToVolumeInFocusedWindowWhenReady_(volumeId, opt_directoryPath) {
    this.retrieveVolumeInfo_(volumeId).then(volume => {
      this.navigateToVolumeInFocusedWindow_(volume, opt_directoryPath);
    });
  }

  /**
   * If a path was specified, retrieve that directory entry,
   * otherwise return the root entry of the volume.
   *
   * @param {!VolumeInfo} volume
   * @param {string=} opt_directoryPath Optional directory path to be opened.
   * @return {!Promise<!DirectoryEntry>}
   * @private
   */
  retrieveEntryInVolume_(volume, opt_directoryPath) {
    return volume.resolveDisplayRoot().then(root => {
      if (opt_directoryPath) {
        return new Promise(
            root.getDirectory.bind(root, opt_directoryPath, {create: false}));
      } else {
        return Promise.resolve(root);
      }
    });
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param {!VolumeInfo} volume
   * @param {string=} opt_directoryPath Optional directory path to be opened.
   * @private
   */
  navigateToVolumeRoot_(volume, opt_directoryPath) {
    this.retrieveEntryInVolume_(volume, opt_directoryPath)
        .then(
            /**
             * Launches app opened on {@code directory}.
             * @param {DirectoryEntry} directory
             */
            directory => {
              launcher.launchFileManager(
                  {currentDirectoryURL: directory.toURL()});
            });
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI of the focused
   * window.
   *
   * @param {!VolumeInfo} volume
   * @param {string=} opt_directoryPath Optional directory path to be opened.
   * @private
   */
  navigateToVolumeInFocusedWindow_(volume, opt_directoryPath) {
    this.retrieveEntryInVolume_(volume, opt_directoryPath)
        .then(function(directoryEntry) {
          if (directoryEntry) {
            volumeManagerFactory.getInstance().then(volumeManager => {
              volumeManager.dispatchEvent(
                  VolumeManagerCommon.createArchiveOpenedEvent(directoryEntry));
            });
          }
        });
  }

  /**
   * Handles mounted FSP volumes and fires the Files app. This is a quick fix
   * for crbug.com/456648.
   * @param {!Object} event Event details.
   * @private
   */
  onMountCompleted_(event) {
    util.doIfPrimaryContext(() => {
      this.onMountCompletedInternal_(event);
    });
  }

  /**
   * @param {!Object} event Event details.
   * @private
   */
  onMountCompletedInternal_(event) {
    const statusOK = event.status === 'success' ||
        event.status === VolumeManagerCommon.VolumeError.PATH_ALREADY_MOUNTED;
    const volumeTypeOK = event.volumeMetadata.volumeType ===
            VolumeManagerCommon.VolumeType.PROVIDED &&
        event.volumeMetadata.source === VolumeManagerCommon.Source.FILE;
    if (event.eventType === 'mount' && statusOK &&
        event.volumeMetadata.mountContext === 'user' && volumeTypeOK) {
      this.navigateToVolumeWhenReady_(event.volumeMetadata.volumeId);
    }
  }
}

/** @private {number} Total number of retries for the resolve entries below.*/
const MAX_RETRIES = 6;

/**
 * Retry the resolveIsolatedEntries() until we get the same number of entries
 * back.
 * @param {!Array<!Entry>} isolatedEntries Entries that need to be resolved.
 * @return {!Promise<!Array<!Entry>>} Promise resolved with the entries
 *   resolved.
 */
async function retryResolveIsolatedEntries(isolatedEntries) {
  let count = 0;
  let externalEntries = [];
  // Wait time in milliseconds between attempts. We double this value after
  // every wait.
  let waitTime = 25;

  // Total waiting time is ~1.5 second for `waitTime` starting at 25ms and total
  // of 6 attempts.
  while (count <= MAX_RETRIES) {
    externalEntries = await resolveIsolatedEntries(isolatedEntries);
    if (externalEntries.length >= isolatedEntries.length) {
      return externalEntries;
    }

    console.warn(`Failed to resolve, retrying in ${waitTime}ms...`);
    await new Promise(resolve => setTimeout(resolve, waitTime));
    waitTime = waitTime * 2;
    count += 1;
  }

  console.warn(
      `Failed to resolve: Requested ${isolatedEntries.length},` +
      ` resolved: ${externalEntries.length}.`);
  return [];
}

/**
 * Prefix for the dialog ID.
 * @type {!string}
 * @const
 */
const DIALOG_ID_PREFIX = 'dialog#';

/**
 * Value of the next file manager dialog ID.
 * @type {number}
 */
let nextFileManagerDialogID = 0;

/**
 * Singleton instance of Background object.
 * @type {!FileManagerBaseInterface}
 */
export const background = new FileManagerBase();
window.background = background;

/**
 * End recording of the background page Load.BackgroundScript metric.
 * NOTE: This call must come after the call to metrics.clearUserId.
 */
metrics.recordInterval('Load.BackgroundScript');
