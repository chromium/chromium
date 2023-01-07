// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppState} from '../../common/js/files_app_state.js';
import {VolumeManager} from '../volume_manager.js';

import {Crostini} from './crostini.js';
import {DriveSyncHandler} from './drive_sync_handler.js';
import {FileOperationManager} from './file_operation_manager.js';
import {ProgressCenter} from './progress_center.js';

/**
 * @interface
 */
export class FileManagerBaseInterface {
  constructor() {
    /** @type {!Object<!Window>} */
    this.dialogs;

    /**
     * @type {!DriveSyncHandler}
     */
    this.driveSyncHandler;

    /**
     * @type {!ProgressCenter}
     */
    this.progressCenter;

    /**
     * String assets.
     * @type {Object<string>}
     */
    this.stringData;

    /**
     * @type {FileOperationManager}
     */
    this.fileOperationManager;

    /**
     * @type {!Crostini}
     */
    this.crostini;
  }

  /** @return {!Promise<!VolumeManager>} */
  getVolumeManager() {}

  /**
   * Register callback to be invoked after initialization of the background
   * page. If the initialization is already done, the callback is invoked
   * immediately.
   *
   * @param {function()} callback
   */
  ready(callback) {}

  /**
   * Forces File Operation Util to return error for automated tests.
   * @param {boolean} enable
   */
  forceFileOperationErrorForTest(enable) {}

  /**
   * Registers a dialog (file picker or save as) in the background page.
   * Dialogs are opened by the browser directly and should register themselves
   * in the background page.
   * @param {!Window} window
   */
  registerDialog(window) {}

  /**
   * Launches a new File Manager window.
   *
   * @param {!FilesAppState=} appState App state.
   * @return {!Promise<void>} Resolved when the new window is opened.
   */
  async launchFileManager(appState = {}) {}
}
