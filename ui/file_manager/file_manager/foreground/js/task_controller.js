// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof, assertNotReached} from 'chrome://resources/js/assert.m.js';
import {Command} from 'chrome://resources/js/cr/ui/command.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {startIOTask} from '../../common/js/api.js';
import {DialogType} from '../../common/js/dialog_type.js';
import {strf, util} from '../../common/js/util.js';
import {Crostini} from '../../externs/background/crostini.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {DirectoryModel} from './directory_model.js';
import {FileSelection, FileSelectionHandler} from './file_selection.js';
import {FileTasks} from './file_tasks.js';
import {FileTransferController} from './file_transfer_controller.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {MetadataUpdateController} from './metadata_update_controller.js';
import {NamingController} from './naming_controller.js';
import {TaskHistory} from './task_history.js';
import {FileManagerUI} from './ui/file_manager_ui.js';

export class TaskController {
  /**
   * @param {DialogType} dialogType
   * @param {!VolumeManager} volumeManager
   * @param {!FileManagerUI} ui
   * @param {!MetadataModel} metadataModel
   * @param {!DirectoryModel} directoryModel
   * @param {!FileSelectionHandler} selectionHandler
   * @param {!MetadataUpdateController} metadataUpdateController
   * @param {!NamingController} namingController
   * @param {!Crostini} crostini
   * @param {!ProgressCenter} progressCenter
   */
  constructor(
      dialogType, volumeManager, ui, metadataModel, directoryModel,
      selectionHandler, metadataUpdateController, namingController, crostini,
      progressCenter) {
    /**
     * @private {DialogType}
     * @const
     */
    this.dialogType_ = dialogType;

    /**
     * @private {!VolumeManager}
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private {!FileManagerUI}
     * @const
     */
    this.ui_ = ui;

    /** @private {?FileTransferController} */
    this.fileTransferController_;

    /**
     * @private {!MetadataModel}
     * @const
     */
    this.metadataModel_ = metadataModel;

    /**
     * @private {!DirectoryModel}
     * @const
     */
    this.directoryModel_ = directoryModel;

    /**
     * @private {!FileSelectionHandler}
     * @const
     */
    this.selectionHandler_ = selectionHandler;

    /**
     * @type {!MetadataUpdateController}
     * @const
     * @private
     */
    this.metadataUpdateController_ = metadataUpdateController;

    /**
     * @private {!NamingController}
     * @const
     */
    this.namingController_ = namingController;

    /**
     * @type {!Crostini}
     * @const
     * @private
     */
    this.crostini_ = crostini;

    /**
     * @type {!ProgressCenter}
     * @const
     * @private
     */
    this.progressCenter_ = progressCenter;

    /**
     * @type {!TaskHistory}
     * @const
     * @private
     */
    this.taskHistory_ = new TaskHistory();

    /**
     * @private {boolean}
     */
    this.canExecuteDefaultTask_ = false;

    /**
     * @private {boolean}
     */
    this.canExecuteOpenActions_ = false;

    /**
     * @private {!Command}
     * @const
     */
    this.defaultTaskCommand_ =
        assertInstanceof(document.querySelector('#default-task'), Command);

    /**
     * More actions command that uses #open-with as selector due to the
     * open-with command used previously for the same task.
     * @private {!Command}
     * @const
     */
    this.openWithCommand_ =
        assertInstanceof(document.querySelector('#open-with'), Command);

    /**
     * @private {Promise<!FileTasks>}
     */
    this.tasks_ = null;

    /**
     * Entries that are used to generate FileTasks returned by this.tasks_.
     * @private {!Array<!Entry>}
     */
    this.tasksEntries_ = [];

    /**
     * Map used to track extract IOTasks in progress.
     * @private @const {Map}
     */
    this.extractTasks_ = new Map();

    /**
     * Selected entries from the last time onSelectionChanged_ was called.
     * @private {!Array<!Entry>}
     */
    this.lastSelectedEntries_ = [];

    ui.taskMenuButton.addEventListener(
        'select', this.onTaskItemClicked_.bind(this));
    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE,
        this.onSelectionChanged_.bind(this));
    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE_THROTTLED,
        this.updateTasks_.bind(this));
    this.taskHistory_.addEventListener(
        TaskHistory.EventType.UPDATE, this.updateTasks_.bind(this));
    chrome.fileManagerPrivate.onAppsUpdated.addListener(
        this.updateTasks_.bind(this));
  }

  /**
   * @param {?FileTransferController} fileTransferController
   */
  setFileTransferController(fileTransferController) {
    this.fileTransferController_ = fileTransferController;
  }

  /**
   * Task combobox handler.
   *
   * @param {Object} event Event containing task which was clicked.
   * @private
   */
  onTaskItemClicked_(event) {
    // If the clicked target has an associated command, the click event should
    // not be handled here since it is handled as a command.
    if (event.target && event.target.command) {
      return;
    }

    // 'select' event from ComboButton has the item as event.item.
    // 'activate' event from MenuButton has the item as event.target.data.
    const item = event.item || event.target.data;
    this.getFileTasks()
        .then(tasks => {
          switch (item.type) {
            case FileTasks.TaskMenuButtonItemType.ShowMenu:
              this.ui_.taskMenuButton.showMenu(false);
              break;
            case FileTasks.TaskMenuButtonItemType.RunTask:
              tasks.execute(item.task);
              break;
            case FileTasks.TaskMenuButtonItemType.ChangeDefaultTask:
              const selection = this.selectionHandler_.selection;
              const extensions = [];

              for (let i = 0; i < selection.entries.length; i++) {
                const match = /\.(\w+)$/g.exec(selection.entries[i].toURL());
                if (match) {
                  const ext = match[1].toUpperCase();
                  if (extensions.indexOf(ext) == -1) {
                    extensions.push(ext);
                  }
                }
              }

              let format = '';

              if (extensions.length == 1) {
                format = extensions[0];
              }

              // Change default was clicked. We should open "change default"
              // dialog.
              tasks.showTaskPicker(
                  this.ui_.defaultTaskPicker,
                  loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM'),
                  strf('CHANGE_DEFAULT_CAPTION', format),
                  this.changeDefaultTask_.bind(this, selection),
                  FileTasks.TaskPickerType.ChangeDefault);
              break;
            default:
              assertNotReached('Unknown task.');
          }
        })
        .catch(error => {
          if (error) {
            console.warn(error.stack || error);
          }
        });
  }

  /**
   * Sets the given task as default, when this task is applicable.
   *
   * @param {!FileSelection} selection File selection.
   * @param {Object} task Task to set as default.
   * @private
   */
  changeDefaultTask_(selection, task) {
    const entries = selection.entries;

    Promise.all(entries.map((entry) => this.getMimeType_(entry)))
        .then(mimeTypes => {
          chrome.fileManagerPrivate.setDefaultTask(
              task.descriptor, entries, mimeTypes, util.checkAPIError);
          this.metadataUpdateController_.refreshCurrentDirectoryMetadata();

          // Update task menu button unless the task button was updated other
          // selection.
          if (this.selectionHandler_.selection === selection) {
            this.tasks_ = null;
            this.getFileTasks()
                .then(tasks => {
                  tasks.display(this.ui_.taskMenuButton);
                })
                .catch(error => {
                  if (error) {
                    console.warn(error.stack || error);
                  }
                });
          }
          this.selectionHandler_.onFileSelectionChanged();
        });
  }

  /**
   * Executes default task.
   */
  executeDefaultTask() {
    this.getFileTasks()
        .then(tasks => {
          const task = {
            descriptor:
                /** @type {!chrome.fileManagerPrivate.FileTaskDescriptor} */ (
                    this.ui_.defaultTaskMenuItem.descriptor),
            title: /** @type {string} */ (this.ui_.defaultTaskMenuItem.label),
            get iconUrl() {
              assert(false);
              return '';
            },
            get isDefault() {
              assert(false);
              return false;
            },
            get isGenericFileHandler() {
              assert(false);
              return false;
            },
          };
          tasks.execute(task);
        })
        .catch(error => {
          if (error) {
            console.warn(error.stack || error);
          }
        });
  }

  /**
   * Get MIME type for an entry. This method first tries to obtain the MIME type
   * from metadata. If it fails, this falls back to obtain the MIME type from
   * its content or name.
   *
   * @param {!Entry} entry An entry to obtain its mime type.
   * @return {!Promise}
   * @private
   */
  getMimeType_(entry) {
    return this.metadataModel_.get([entry], ['contentMimeType'])
        .then(properties => {
          if (properties[0].contentMimeType) {
            return properties[0].contentMimeType;
          }
          return new Promise((fulfill, reject) => {
            chrome.fileManagerPrivate.getMimeType(entry, mimeType => {
              if (!chrome.runtime.lastError) {
                fulfill(mimeType);
              } else {
                reject(chrome.runtime.lastError);
              }
            });
          });
        });
  }

  /**
   * Handles change of selection and clears context menu.
   * @private
   */
  onSelectionChanged_() {
    if (window.IN_TEST) {
      this.ui_.taskMenuButton.removeAttribute('get-tasks-completed');
    }
    const selection = this.selectionHandler_.selection;
    // Caller of update context menu task items.
    // FileSelectionHandler.EventType.CHANGE
    if (this.dialogType_ === DialogType.FULL_PAGE &&
        (selection.directoryCount > 0 || selection.fileCount > 0)) {
      // Compare entries while ignoring changes inside directories.
      if (!util.isSameEntries(this.lastSelectedEntries_, selection.entries)) {
        // Update the context menu if selection changed.
        this.updateContextMenuTaskItems_([]);
      }
    } else {
      // Update context menu.
      this.updateContextMenuTaskItems_([]);
    }
    this.lastSelectedEntries_ = selection.entries;
  }

  /**
   * Updates available tasks opened from context menu or the open button.
   * @private
   */
  updateTasks_() {
    const selection = this.selectionHandler_.selection;
    if (this.dialogType_ === DialogType.FULL_PAGE &&
        (selection.directoryCount > 0 || selection.fileCount > 0)) {
      this.getFileTasks()
          .then(tasks => {
            tasks.display(this.ui_.taskMenuButton);
            this.updateContextMenuTaskItems_(tasks.getOpenTaskItems());
            if (window.IN_TEST) {
              this.ui_.taskMenuButton.toggleAttribute(
                  'get-tasks-completed', true);
            }
          })
          .catch(error => {
            if (error) {
              console.warn(error.stack || error);
            }
          });
    } else {
      this.ui_.taskMenuButton.hidden = true;
      if (window.IN_TEST) {
        this.ui_.taskMenuButton.toggleAttribute('get-tasks-completed', true);
      }
    }
  }

  /**
   * @return {!Promise<!FileTasks>}
   * @public
   */
  getFileTasks() {
    const selection = this.selectionHandler_.selection;
    if (this.tasks_ &&
        util.isSameEntries(this.tasksEntries_, selection.entries)) {
      return this.tasks_;
    }
    this.tasksEntries_ = selection.entries;
    this.tasks_ = selection.computeAdditional(this.metadataModel_).then(() => {
      if (this.selectionHandler_.selection !== selection) {
        if (util.isSameEntries(this.tasksEntries_, selection.entries)) {
          this.tasks_ = null;
        }
        return Promise.reject();
      }
      return FileTasks
          .create(
              this.volumeManager_, this.metadataModel_, this.directoryModel_,
              this.ui_, this.fileTransferController_, selection.entries,
              assert(selection.mimeTypes), this.taskHistory_,
              this.namingController_, this.crostini_, this.progressCenter_)
          .then(tasks => {
            if (this.selectionHandler_.selection !== selection) {
              if (util.isSameEntries(this.tasksEntries_, selection.entries)) {
                this.tasks_ = null;
              }
              return Promise.reject();
            }
            return tasks;
          });
    });
    return this.tasks_;
  }

  /**
   * Returns whether default task command can be executed or not.
   * @return {boolean} True if default task command is executable.
   */
  canExecuteDefaultTask() {
    return this.canExecuteDefaultTask_;
  }

  /**
   * Returns whether open with command can be executed or not.
   * @return {boolean} True if open with command is executable.
   */
  canExecuteOpenActions() {
    return this.canExecuteOpenActions_;
  }

  /**
   * Updates tasks menu item to match passed task items.
   *
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} openTasks List of OPEN
   *     tasks.
   * @private
   */
  updateContextMenuTaskItems_(openTasks) {
    const defaultTask = FileTasks.getDefaultTask(openTasks, this.taskHistory_);
    if (defaultTask) {
      const menuItem = this.ui_.defaultTaskMenuItem;
      /**
       * Menu icon can be controlled by either `iconEndImage` or
       * `iconEndFileType`, since the default task menu item DOM is shared,
       * before updating it, we should remove the previous one, e.g. reset both
       * `iconEndImage` and `iconEndFileType`.
       */
      menuItem.iconEndImage = '';
      menuItem.removeIconEndFileType();

      menuItem.setIconEndHidden(false);
      if (defaultTask.iconType) {
        menuItem.iconEndFileType = defaultTask.iconType;
      } else if (defaultTask.iconUrl) {
        menuItem.iconEndImage = 'url(' + defaultTask.iconUrl + ')';
      } else {
        menuItem.setIconEndHidden(true);
      }

      menuItem.label = defaultTask.label || defaultTask.title;
      menuItem.disabled = !!defaultTask.disabled;
      menuItem.descriptor = defaultTask.descriptor;
    }

    this.canExecuteDefaultTask_ = defaultTask != null;
    this.defaultTaskCommand_.canExecuteChange(this.ui_.listContainer.element);

    this.canExecuteOpenActions_ = openTasks.length > 1;
    this.openWithCommand_.canExecuteChange(this.ui_.listContainer.element);

    this.ui_.tasksSeparator.hidden = openTasks.length === 0;
  }

  /**
   * Return the tasks for the FileEntry |entry|.
   *
   * @param {FileEntry} entry
   * @return {!Promise<!FileTasks>}
   */
  getEntryFileTasks(entry) {
    return this.metadataModel_.get([entry], ['contentMimeType']).then(props => {
      return FileTasks.create(
          this.volumeManager_, this.metadataModel_, this.directoryModel_,
          this.ui_, this.fileTransferController_, [entry],
          [props[0].contentMimeType || null], this.taskHistory_,
          this.namingController_, this.crostini_, this.progressCenter_);
    });
  }

  /**
   * @param {FileEntry} entry
   */
  executeEntryTask(entry) {
    this.getEntryFileTasks(entry).then(tasks => {
      tasks.executeDefault();
    });
  }

  /**
   * Stores the task ID and parameters for an extract archive task.
   */
  storeExtractTaskDetails(taskId, selectionEntries, parameters) {
    this.extractTasks_.set(
        taskId, {'entries': selectionEntries, 'params': parameters});
  }

  /**
   * Removes information about an extract archive task.
   */
  deleteExtractTaskDetails(taskId) {
    this.extractTasks_.delete(taskId);
  }

  /**
   * Starts extraction for a single entry and stores the task details.
   * @private
   */
  async startExtractTask_(entry, params) {
    let taskId;
    try {
      taskId = await startIOTask(
          chrome.fileManagerPrivate.IOTaskType.EXTRACT, [entry], params);
      this.storeExtractTaskDetails(taskId, [entry], params);
    } catch (e) {
      console.warn('Error getting extract taskID', e);
    }
  }

  /**
   * Triggers a password dialog and starts an extract task with the
   * password (unless cancel is clicked on the dialog).
   * @private
   */
  async startGetPasswordThenExtractTask_(entry, params) {
    /** @type {?string} */ let password = null;
    // Ask for password.
    try {
      password = await this.ui_.passwordDialog.askForPassword(
          entry.fullPath, password);
    } catch (error) {
      console.warn('User cancelled password fetch ', error);
      return;
    }

    params['password'] = password;
    await this.startExtractTask_(entry, params);
  }

  /**
   * If an extract operation has finished due to missing password,
   * see if we have the operation stored and if so, pop up a password
   * dialog and try to restart another IO operation for it.
   */
  handleMissingPassword(taskId) {
    const existingOperation = this.extractTasks_.get(taskId);
    if (existingOperation) {
      // If we have multiple entries (from a multi-select extract) then
      // we need to start a new task for each of them individually so
      // that the password dialog is presented once for every file
      // that's encrypted.
      const selectionEntries = existingOperation['entries'];
      const params = existingOperation['params'];
      if (selectionEntries.length == 1) {
        this.startGetPasswordThenExtractTask_(
            existingOperation['entries'][0], params);
      } else {
        for (const entry of selectionEntries) {
          this.startExtractTask_(entry, params);
        }
      }
    }
    // Remove the failed operation reference since it's finished.
    this.deleteExtractTaskDetails(taskId);
  }
}
