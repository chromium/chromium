// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 */
class FileBrowserBackgroundFull extends BackgroundBase {
  constructor() {
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
     * @type {!mediaImportInterfaces.ImportRunner}
     */
    this.mediaImportHandler;

    /**
     * @type {!mediaScannerInterfaces.MediaScanner}
     */
    this.mediaScanner;

    /**
     * @type {!importerHistoryInterfaces.HistoryLoader}
     */
    this.historyLoader;

    /**
     * @type {!Crostini}
     */
    this.crostini;
  }

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
}
