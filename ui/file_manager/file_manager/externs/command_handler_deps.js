// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../common/js/dialog_type.js';
import {FilesAppState} from '../common/js/files_app_state.js';
import {ActionsController} from '../foreground/js/actions_controller.js';
import {FileFilter} from '../foreground/js/directory_contents.js';
import {DirectoryModel} from '../foreground/js/directory_model.js';
import {DirectoryTreeNamingController} from '../foreground/js/directory_tree_naming_controller.js';
import {FileSelection, FileSelectionHandler} from '../foreground/js/file_selection.js';
import {FileTransferController} from '../foreground/js/file_transfer_controller.js';
import {MetadataModel} from '../foreground/js/metadata/metadata_model.js';
import {NamingController} from '../foreground/js/naming_controller.js';
import {ProvidersModel} from '../foreground/js/providers_model.js';
import {SpinnerController} from '../foreground/js/spinner_controller.js';
import {TaskController} from '../foreground/js/task_controller.js';
import {DirectoryTree} from '../foreground/js/ui/directory_tree.js';
import {FileManagerUI} from '../foreground/js/ui/file_manager_ui.js';

import {Crostini} from './background/crostini.js';
import {FileOperationManager} from './background/file_operation_manager.js';
import {ProgressCenter} from './background/progress_center.js';
import {FilesAppEntry} from './files_app_entry_interfaces.js';
import {VolumeManager} from './volume_manager.js';


/**
 * Interface on which |CommandHandler| depends.
 * @interface
 */
export class CommandHandlerDeps {
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

    /** @type {boolean} */
    this.guestMode;

    /** @type {boolean} */
    this.trashEnabled;
  }

  /** @return {DirectoryEntry|FilesAppEntry} */
  getCurrentDirectoryEntry() {}

  /** @return {FileSelection} */
  getSelection() {}

  /** @param {!FilesAppState=} appState App state. */
  launchFileManager(appState) {}
}
