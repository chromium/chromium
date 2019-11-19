// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @interface
 */
class FileBrowserBackgroundFull extends FileBrowserBackground {
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
     * @type {!importer.ImportRunner}
     */
    this.mediaImportHandler;

    /**
     * @type {!importer.MediaScanner}
     */
    this.mediaScanner;

    /**
     * @type {!importer.HistoryLoader}
     */
    this.historyLoader;

    /**
     * @type {!Crostini}
     */
    this.crostini;
  }
}
