// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppState} from '../../common/js/files_app_state.js';

import {Crostini} from './crostini.js';
import {DriveSyncHandler} from './drive_sync_handler.js';
import {ProgressCenter} from './progress_center.js';

/**
 * @interface
 */
export class FileManagerBaseInterface {
  constructor() {
    /** @type {!Record<string, !Window>} */
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
     * @type {?Record<string, string>}
     */
    this.stringData;

    /**
     * @type {!Crostini}
     */
    this.crostini;
  }

  // @ts-ignore: error TS2355: A function whose declared type is neither 'void'
  // nor 'any' must return a value.
  /** @return {!Promise<!import('../volume_manager.js').VolumeManager>} */
  getVolumeManager() {}

  /**
   * Register callback to be invoked after initialization of the background
   * page. If the initialization is already done, the callback is invoked
   * immediately.
   *
   * @param {function():void} callback
   */
  // @ts-ignore: error TS6133: 'callback' is declared but its value is never
  // read.
  ready(callback) {}

  /**
   * Registers a dialog (file picker or save as) in the background page.
   * Dialogs are opened by the browser directly and should register themselves
   * in the background page.
   * @param {!Window} window
   */
  // @ts-ignore: error TS6133: 'window' is declared but its value is never read.
  registerDialog(window) {}

  /**
   * Launches a new File Manager window.
   *
   * @param {!FilesAppState=} appState App state.
   * @return {!Promise<void>} Resolved when the new window is opened.
   */
  // @ts-ignore: error TS2739: Type '{}' is missing the following properties
  // from type 'FilesAppState': currentDirectoryURL, selectionURL
  async launchFileManager(appState = {}) {}
}
