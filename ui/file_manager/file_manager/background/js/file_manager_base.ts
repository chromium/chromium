// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.js';

import {getDirectory} from '../../common/js/api.js';
import type {FilesAppState} from '../../common/js/files_app_state.js';
import {recordInterval} from '../../common/js/metrics.js';
import {isInGuestMode} from '../../common/js/util.js';
import {ARCHIVE_OPENED_EVENT_TYPE, Source, VOLUME_ALREADY_MOUNTED, VolumeType} from '../../common/js/volume_manager_types.js';

import {AppWindowWrapper} from './app_window_wrapper.js';
import {Crostini} from './crostini.js';
import {DriveSyncHandlerImpl} from './drive_sync_handler.js';
import {FileOperationHandler} from './file_operation_handler.js';
import {ProgressCenter} from './progress_center.js';
import type {VolumeInfo} from './volume_info.js';
import type {VolumeAlreadyMountedEvent, VolumeManager} from './volume_manager.js';
import {volumeManagerFactory} from './volume_manager_factory.js';

/**
 * Root class of the former background page.
 */
export class FileManagerBase {
  private initializationPromise_: Promise<Record<string, string>>;
  protected fileOperationHandler_: FileOperationHandler|null = null;

  /**
   * Map of all currently open file dialogs. The key is an app ID.
   */
  dialogs: Record<string, Window> = {};

  /**
   * Progress center of the background page.
   */
  progressCenter: ProgressCenter = new ProgressCenter();

  /**
   * Drive sync handler.
   */
  driveSyncHandler = new DriveSyncHandlerImpl(this.progressCenter);

  crostini = new Crostini();

  /**
   * String assets.
   */
  stringData: null|Record<string, string> = null;

  constructor() {
    /**
     * Initializes the strings. This needs for the volume manager.
     */
    this.initializationPromise_ = new Promise((fulfill) => {
      chrome.fileManagerPrivate.getStrings(stringData => {
        if (chrome.runtime.lastError) {
          console.error(chrome.runtime.lastError.message);
          return;
        }
        if (!loadTimeData.isInitialized()) {
          loadTimeData.data = assert(stringData);
        }
        fulfill(stringData as Record<string, string>);
      });
    });
    this.initializationPromise_.then(strings => {
      this.stringData = strings;
      this.crostini.initEnabled();

      volumeManagerFactory.getInstance().then(volumeManager => {
        volumeManager.addEventListener(
            VOLUME_ALREADY_MOUNTED, this.handleViewEvent_.bind(this));

        this.crostini.initVolumeManager(volumeManager);
      });

      this.fileOperationHandler_ =
          new FileOperationHandler(this.progressCenter);
    });

    // Handle newly mounted FSP file systems. Workaround for crbug.com/456648.
    // TODO(mtomasz): Replace this hack with a proper solution.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));
  }

  async getVolumeManager(): Promise<VolumeManager> {
    return volumeManagerFactory.getInstance();
  }

  async ready(): Promise<void> {
    await this.initializationPromise_;
  }

  /**
   * Registers dialog window to the background page.
   *
   * @param dialogWindow Window of the dialog.
   */
  registerDialog(dialogWindow: Window) {
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
   * @param appState App state.
   * @return Resolved when the new window is opened.
   */
  async launchFileManager(appState: FilesAppState = {}): Promise<void> {
    await this.initializationPromise_;

    const appWindow = new AppWindowWrapper();

    return appWindow.launch(appState || {});
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param event An event with the volumeId or
   *     devicePath.
   */
  private async handleViewEvent_(event: VolumeAlreadyMountedEvent) {
    const isPrimaryContext = await isInGuestMode();
    if (isPrimaryContext) {
      this.handleViewEventInternal_(event);
    }
  }

  /**
   * @param event An event with the volumeId.
   */
  private async handleViewEventInternal_(event: VolumeAlreadyMountedEvent):
      Promise<void> {
    await volumeManagerFactory.getInstance();
    this.navigateToVolumeInFocusedWindowWhenReady_(event.detail.volumeId);
  }

  /**
   * Retrieves the root file entry of the volume on the requested device.
   *
   * @param volumeId ID of the volume to navigate to.
   */
  private async retrieveVolumeInfo_(volumeId: string):
      Promise<VolumeInfo|void> {
    const volumeManager = await volumeManagerFactory.getInstance();
    try {
      return await volumeManager.whenVolumeInfoReady(volumeId);
    } catch (e: any) {
      console.warn(
          'Unable to find volume for id: ' + volumeId +
          '. Error: ' + e.message);
    }
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param volumeId ID of the volume to navigate to.
   * @param directoryPath Optional path to be opened.
   */
  private async navigateToVolumeWhenReady_(
      volumeId: string, directoryPath?: string): Promise<void> {
    const volume = await this.retrieveVolumeInfo_(volumeId);
    if (volume) {
      this.navigateToVolumeRoot_(volume, directoryPath);
    }
  }

  /**
   * Opens the volume root (or opt directoryPath) in the main UI of the focused
   * window.
   *
   * @param volumeId ID of the volume to navigate to.
   * @param directoryPath Optional path to be opened.
   */
  private async navigateToVolumeInFocusedWindowWhenReady_(
      volumeId: string, directoryPath?: string): Promise<void> {
    const volume = await this.retrieveVolumeInfo_(volumeId);
    if (volume) {
      this.navigateToVolumeInFocusedWindow_(volume, directoryPath);
    }
  }

  /**
   * If a path was specified, retrieve that directory entry,
   * otherwise return the root entry of the volume.
   *
   * @param directoryPath Optional directory path to be opened.
   */
  private async retrieveEntryInVolume_(
      volume: VolumeInfo, directoryPath?: string): Promise<DirectoryEntry> {
    const root = await volume.resolveDisplayRoot();
    if (directoryPath) {
      return getDirectory(root, directoryPath, {create: false});
    }
    return root;
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param directoryPath Optional directory path to be opened.
   */
  private async navigateToVolumeRoot_(
      volume: VolumeInfo, directoryPath?: string): Promise<void> {
    const directory = await this.retrieveEntryInVolume_(volume, directoryPath);
    /**
     * Launches app opened on {@code directory}.
     */
    this.launchFileManager({currentDirectoryURL: directory.toURL()});
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI of the focused
   * window.
   *
   * @param directoryPath Optional directory path to be opened.
   */
  private async navigateToVolumeInFocusedWindow_(
      volume: VolumeInfo, directoryPath?: string): Promise<void> {
    const directoryEntry =
        await this.retrieveEntryInVolume_(volume, directoryPath);
    if (directoryEntry) {
      const volumeManager = await volumeManagerFactory.getInstance();
      volumeManager.dispatchEvent(new CustomEvent(
          ARCHIVE_OPENED_EVENT_TYPE, {detail: {mountPoint: directoryEntry}}));
    }
  }

  /**
   * Handles mounted FSP volumes and fires the Files app. This is a quick fix
   * for crbug.com/456648.
   * @param event Event details.
   */
  private async onMountCompleted_(
      event: chrome.fileManagerPrivate.MountCompletedEvent) {
    const isPrimaryContext = await isInGuestMode();
    if (isPrimaryContext) {
      this.onMountCompletedInternal_(event);
    }
  }

  /**
   * @param event Event details.
   */
  private onMountCompletedInternal_(
      event: chrome.fileManagerPrivate.MountCompletedEvent) {
    const statusOK =
        event.status === chrome.fileManagerPrivate.MountError.SUCCESS ||
        event.status ===
            chrome.fileManagerPrivate.MountError.PATH_ALREADY_MOUNTED;
    const volumeTypeOK =
        event.volumeMetadata.volumeType === VolumeType.PROVIDED &&
        event.volumeMetadata.source === Source.FILE;
    if (event.eventType === 'mount' && statusOK &&
        event.volumeMetadata.mountContext === 'user' && volumeTypeOK) {
      this.navigateToVolumeWhenReady_(event.volumeMetadata.volumeId);
    }
  }
}

/**
 * Prefix for the dialog ID.
 */
const DIALOG_ID_PREFIX = 'dialog#';

/**
 * Value of the next file manager dialog ID.
 */
let nextFileManagerDialogID = 0;

/**
 * Singleton instance of Background object.
 */
export const background = new FileManagerBase();
window.background = background;

/**
 * End recording of the background page Load.BackgroundScript metric.
 */
recordInterval('Load.BackgroundScript');
