// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppState} from '../common/js/files_app_state.js';
import {DialogType} from '../externs/ts/state.js';
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
import {FileManagerUI} from '../foreground/js/ui/file_manager_ui.js';

import {Crostini} from './background/crostini.js';
import {ProgressCenter} from './background/progress_center.js';
import {FilesAppEntry} from './files_app_entry_interfaces.js';


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

    /** @type {DirectoryTreeNamingController} */
    this.directoryTreeNamingController;

    /** @type {Document} */
    this.document;

    /** @type {FileFilter} */
    this.fileFilter;

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

    /** @type {!import('./volume_manager.js').VolumeManager} */
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

  // @ts-ignore: error TS2355: A function whose declared type is neither 'void'
  // nor 'any' must return a value.
  /** @return {DirectoryEntry|FilesAppEntry} */
  getCurrentDirectoryEntry() {}

  // @ts-ignore: error TS2355: A function whose declared type is neither 'void'
  // nor 'any' must return a value.
  /** @return {FileSelection} */
  getSelection() {}

  /** @param {!FilesAppState=} appState App state. */
  // @ts-ignore: error TS6133: 'appState' is declared but its value is never
  // read.
  launchFileManager(appState) {}
}
