// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import {ActionsController} from '../file_manager/foreground/js/actions_controller.m.js';
// #import {FilesAppEntry} from './files_app_entry_interfaces.m.js';
// #import {Crostini} from './background/crostini.m.js';
// #import {MetadataModel} from '../file_manager/foreground/js/metadata/metadata_model.m.js';
// #import {VolumeManager} from './volume_manager.m.js';
// #import {FileManagerUI} from '../file_manager/foreground/js/ui/file_manager_ui.m.js';
// #import {TaskController} from '../file_manager/foreground/js/task_controller.m.js';
// #import {SpinnerController} from '../file_manager/foreground/js/spinner_controller.m.js';
// #import {ProvidersModel} from '../file_manager/foreground/js/providers_model.m.js';
// #import {ProgressCenter} from './background/progress_center.m.js';
// #import {NamingController} from '../file_manager/foreground/js/naming_controller.m.js';
// #import {FileSelectionHandler, FileSelection} from '../file_manager/foreground/js/file_selection.m.js';
// #import {FileTransferController} from '../file_manager/foreground/js/file_transfer_controller.m.js';
// #import {FileOperationManager} from './background/file_operation_manager.m.js';
// #import {FileFilter} from '../file_manager/foreground/js/directory_contents.m.js';
// #import {DirectoryTreeNamingController} from '../file_manager/foreground/js/directory_tree_naming_controller.m.js';
// #import {DirectoryTree} from '../file_manager/foreground/js/ui/directory_tree.m.js';
// #import {DirectoryModel} from '../file_manager/foreground/js/directory_model.m.js';
// #import {DialogType} from '../file_manager/foreground/js/dialog_type.m.js';
// clang-format on


/**
 * Interface on which |CommandHandler| depends.
 * @interface
 */
/* #export */ class CommandHandlerDeps {
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

    /** @type {!ProgressCenter} */
    this.progressCenter;

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
