// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Interface on which |CommandHandler| depends.
 * @interface
 */
class CommandHandlerDeps {
  constructor() {
    /** @type {ActionsController} */
    this.actionsController;

    /** @type {DialogType} */
    this.dialogType;

    /** @type {DirectoryModel} */
    this.directoryModel;

    /** @type {DirectoryTree} */
    this.directoryTree;

    /** @type {DirectoryTreeNamingController} */
    this.directoryTreeNamingController;

    /** @type {Document} */
    this.document;

    /** @type {FileFilter} */
    this.fileFilter;

    /** @type {FileOperationManager} */
    this.fileOperationManager;

    /** @type {FileTransferController} */
    this.fileTransferController;

    /** @type {FileSelectionHandler} */
    this.selectionHandler;

    /** @type {NamingController} */
    this.namingController;

    /** @type {ProvidersModel} */
    this.providersModel;

    /** @type {SpinnerController} */
    this.spinnerController;

    /** @type {TaskController} */
    this.taskController;

    /** @type {FileManagerUI} */
    this.ui;

    /** @type {!VolumeManager} */
    this.volumeManager;

    /** @type {MetadataModel} */
    this.metadataModel;

    /** @type {Crostini} */
    this.crostini;
  }

  /** @return {DirectoryEntry|FilesAppEntry} */
  getCurrentDirectoryEntry() {}

  /** @return {FileSelection} */
  getSelection() {}

  /** @param {Object} appState App state. */
  launchFileManager(appState) {}
}
