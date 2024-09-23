// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof, assertNotReached} from 'chrome://resources/ash/common/assert.js';

import type {Crostini} from '../../background/js/crostini.js';
import type {ProgressCenter} from '../../background/js/progress_center.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {getMimeType, startIOTask} from '../../common/js/api.js';
import {unwrapEntry} from '../../common/js/entry_utils.js';
import {type AnnotatedTask, getDefaultTask} from '../../common/js/file_tasks.js';
import type {FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {recordDirectoryListLoadWithTolerance, startInterval} from '../../common/js/metrics.js';
import {str, strf} from '../../common/js/translations.js';
import {checkAPIError} from '../../common/js/util.js';
import {fetchFileTasks} from '../../state/ducks/current_directory.js';
import {type FileData, type FileKey, type FileTasks as StoreFileTasks, PropStatus, type State} from '../../state/state.js';
import {getFilesData, getStore, type Store, waitForState} from '../../state/store.js';
import type {XfPasswordDialog} from '../../widgets/xf_password_dialog.js';

import type {DirectoryModel} from './directory_model.js';
import type {FileSelection, FileSelectionHandler} from './file_selection.js';
import {FileTasks, TaskPickerType} from './file_tasks.js';
import type {FileTransferController} from './file_transfer_controller.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import type {MetadataUpdateController} from './metadata_update_controller.js';
import {EventType, TaskHistory} from './task_history.js';
import type {ComboButtonSelectEvent} from './ui/combobutton.js';
import {Command} from './ui/command.js';
import type {FileManagerUI} from './ui/file_manager_ui.js';

/**
 * Type of the object stashed in the Map extractTasks_.
 */
interface ExtractingTasks {
  entries: Array<Entry|FilesAppEntry>;
  params: chrome.fileManagerPrivate.IoTaskParams;
}

export class TaskController {
  private fileTransferController_: FileTransferController|null = null;
  private taskHistory_: TaskHistory;
  private canExecuteDefaultTask_: boolean = false;
  private shouldHideDefaultTask_: boolean = true;
  private canExecuteOpenActions_: boolean = false;
  private defaultTaskCommand_: Command;
  /**
   * More actions command that uses #open-with as selector due to the
   * open-with command used previously for the same task.
   */
  private openWithCommand_: Command;
  /**
   * Cached promise used to avoid initializing the same FileTasks
   * multiple times.
   */
  private tasks_: Promise<FileTasks>|null = null;
  /** Map used to track extract IOTasks in progress.  */
  private extractTasks_: Map<number, ExtractingTasks> = new Map();
  private store_: Store;
  private selectionFilesData_: FileData[] = [];
  private selectionKeys_: FileKey[] = [];
  private selectionTasks_: StoreFileTasks|undefined;

  constructor(
      private volumeManager_: VolumeManager, private ui_: FileManagerUI,
      private metadataModel_: MetadataModel,
      private directoryModel_: DirectoryModel,
      private selectionHandler_: FileSelectionHandler,
      private metadataUpdateController_: MetadataUpdateController,
      private crostini_: Crostini, private progressCenter_: ProgressCenter) {
    this.taskHistory_ = new TaskHistory();
    this.defaultTaskCommand_ =
        assertInstanceof(document.querySelector('#default-task'), Command);
    this.openWithCommand_ =
        assertInstanceof(document.querySelector('#open-with'), Command);
    this.store_ = getStore();

    this.store_.subscribe(this);

    ui_.taskMenuButton.addEventListener(
        'combobutton-select', this.onTaskItemClicked_.bind(this));
    // TODO: Move the following events to the Store.
    this.taskHistory_.addEventListener(
        EventType.UPDATE, this.updateTasks_.bind(this));
    chrome.fileManagerPrivate.onIOTaskProgressStatus.addListener(
        this.onIoTaskProgressStatus_.bind(this));
    chrome.fileManagerPrivate.onAppsUpdated.addListener(
        this.clearCacheAndUpdateTasks_.bind(this));
  }

  onStateChanged(newState: State) {
    const keys = newState.currentDirectory?.selection.keys ?? [];
    const tasks = newState.currentDirectory?.selection.fileTasks;
    // If the selection changed.
    if (keys !== this.selectionKeys_ &&
        (keys.length > 0 || this.selectionKeys_.length > 0)) {
      this.selectionKeys_ = keys;
      this.selectionFilesData_ = getFilesData(newState, keys ?? []);
      // Kickoff the async/ActionsProducer to fetch the tasks for the new
      // selection. If the new selection is empty, still need to update the
      // store so no old file task lingers.
      this.tasks_ = null;
      this.store_.dispatch(fetchFileTasks(this.selectionFilesData_));
      // Hides the button while fetching the tasks.
      this.maybeHideButton();
    }
    // If the file tasks changed.
    if (tasks !== this.selectionTasks_) {
      this.selectionTasks_ = tasks;
      if (tasks?.status === PropStatus.SUCCESS) {
        this.updateTasks_();
      }
    }
  }

  setFileTransferController(fileTransferController: FileTransferController) {
    this.fileTransferController_ = fileTransferController;
  }

  /**
   * Exposes the TaskHistory instance for the ActionsProducer.
   *
   * NOTE: This is a temporary workaround until the TaskHistory is migrated to
   * the store.
   */
  get taskHistory(): TaskHistory {
    return this.taskHistory_;
  }

  /**
   * Task combobox handler.
   *
   * @param event Event containing task which was clicked.
   */
  private async onTaskItemClicked_(event: ComboButtonSelectEvent) {
    // If the clicked target has an associated command, the click event should
    // not be handled here since it is handled as a command.
    // TODO(lucmult): Add TS definition for these events instead of using any.
    if (event.target && (event.target as any).command) {
      return;
    }

    const item: null|DropdownItem = event.detail;
    if (!item) {
      return;
    }

    try {
      const tasks = await this.getFileTasks();
      switch (item.type) {
        case TaskMenuItemType.SHOW_MENU:
          this.ui_.taskMenuButton.showMenu(false);
          break;
        case TaskMenuItemType.RUN_TASK:
          tasks.execute(item.task!);
          break;
        case TaskMenuItemType.CHANGE_DEFAULT_TASK:
          const selection = this.selectionHandler_.selection;
          const extensions = [];

          for (let i = 0; i < selection.entries.length; i++) {
            const match = /\.(\w+)$/g.exec(selection.entries[i]!.toURL());
            if (match) {
              const ext = match[1]!.toUpperCase();
              if (extensions.indexOf(ext) === -1) {
                extensions.push(ext);
              }
            }
          }

          let format = '';

          if (extensions.length === 1) {
            format = extensions[0]!;
          }

          // Change default was clicked. We should open "change default"
          // dialog.
          tasks.showTaskPicker(
              this.ui_.defaultTaskPicker, str('CHANGE_DEFAULT_MENU_ITEM'),
              strf('CHANGE_DEFAULT_CAPTION', format),
              this.changeDefaultTask_.bind(this, selection),
              TaskPickerType.ChangeDefault);
          break;
        default:
          assertNotReached('Unknown task.');
      }
    } catch (error: any) {
      if (error) {
        console.warn(error.stack || error);
      }
    }
  }

  /**
   * Sets the given task as default, when this task is applicable.
   *
   * @param selection File selection.
   * @param task Task to set as default.
   */
  private async changeDefaultTask_(
      selection: FileSelection, task: chrome.fileManagerPrivate.FileTask) {
    const entries =
        selection.entries.map(entry => unwrapEntry(entry)) as Entry[];

    const mimeTypes =
        await Promise.all(entries.map(entry => this.getMimeType_(entry)));
    chrome.fileManagerPrivate.setDefaultTask(
        task.descriptor, entries, mimeTypes, checkAPIError);
    this.metadataUpdateController_.refreshCurrentDirectoryMetadata();

    // Update task menu button unless the task button was updated by other
    // selection.
    if (this.selectionHandler_.selection === selection) {
      this.tasks_ = null;
      try {
        const tasks = await this.getFileTasks();
        this.display_(tasks);
      } catch (error: any) {
        if (error) {
          console.warn(error.stack || error);
        }
      }
    }
    this.selectionHandler_.onFileSelectionChanged();
  }

  /** Displays the list of tasks in a open task picker combobutton. */
  private display_(fileTasks: FileTasks) {
    this.updateTasksDropdown_(fileTasks);
  }

  /**
   * Populate the #tasks-menu with the open-with tasks. The menu is managed by
   * the top task menu Open combobutton, but it is also used as the
   * right-click open-with context menu.
   */
  private updateTasksDropdown_(fileTasks: FileTasks) {
    const combobutton = this.ui_.taskMenuButton;
    const tasks = fileTasks.getAnnotatedTasks();

    combobutton.hidden =
        tasks.length === 0 || fileTasks.entries.some(e => e.isDirectory);

    // Even if the task menu button is hidden, we still update the items if
    // tasks exist since they are used for the right-click context menu.
    if (tasks.length === 0) {
      return;
    }

    combobutton.clear();

    const defaultTask = fileTasks.defaultTask;
    // If there exist defaultTask show it on the combobutton.
    if (defaultTask) {
      combobutton.defaultItem =
          createDropdownItem(defaultTask, str('TASK_OPEN'));
    } else {
      combobutton.defaultItem = {
        type: TaskMenuItemType.SHOW_MENU,
        label: str('OPEN_WITH_BUTTON_LABEL'),
      };
    }

    // If there exist 2 or more available tasks, show them in context menu
    // (including defaultTask). If only one generic task is available, we
    // also show it in the context menu.
    const items = this.createItems(fileTasks);
    if (items.length > 1 || (items.length === 1 && !defaultTask)) {
      for (const item of items) {
        combobutton.addDropDownItem(item);
      }

      // If there exist non generic task (i.e. defaultTask is set) and this
      // default is not set by policy, we show an item to change default task.
      if (defaultTask && !fileTasks.getPolicyDefaultHandlerStatus()) {
        combobutton.addSeparator();
        // TODO(greengrape): Ensure that the passed object is a
        // `DropdownItem`.
        const changeDefaultMenuItem = combobutton.addDropDownItem({
          type: TaskMenuItemType.CHANGE_DEFAULT_TASK,
          label: str('CHANGE_DEFAULT_MENU_ITEM'),
          isDefault: false,
          isPolicyDefault: false,
        });
        changeDefaultMenuItem.classList.add('change-default');
      }
    }
  }

  /**
   * Creates sorted array of available task descriptions such as title and
   * icon.
   *
   * @param fileTasks File Tasks to create items.
   * @return Created array can be used to feed combobox, menus and so on.
   */
  createItems(fileTasks: FileTasks): DropdownItem[] {
    const tasks = fileTasks.getAnnotatedTasks();
    const items = [];

    // Create items.
    for (const task of tasks) {
      if (task === fileTasks.defaultTask) {
        const title = task.title + ' ' + str('DEFAULT_TASK_LABEL');
        items.push(createDropdownItem(
            task, title, /*isDefault=*/ true,
            /*isPolicyDefault=*/
            !!fileTasks.getPolicyDefaultHandlerStatus()));
      } else {
        items.push(createDropdownItem(task));
      }
    }

    // Sort items (Sort order: isDefault, lastExecutedTime, label).
    items.sort((a, b) => {
      // Sort by isDefaultTask.
      const isDefault = (b.isDefault ? 1 : 0) - (a.isDefault ? 1 : 0);
      if (isDefault !== 0) {
        return isDefault;
      }

      // Sort by last-executed time.
      const aTime = this.taskHistory_.getLastExecutedTime(a.task!.descriptor);
      const bTime = this.taskHistory_.getLastExecutedTime(b.task!.descriptor);
      if (aTime !== bTime) {
        return bTime - aTime;
      }

      // Sort by label.
      return a.label.localeCompare(b.label);
    });

    return items;
  }

  /** Executes default task from the dropdown menu. */
  async executeDefaultTask() {
    try {
      const tasks = await this.getFileTasks();
      const task: chrome.fileManagerPrivate.FileTask = {
        descriptor: this.ui_.defaultTaskMenuItem.descriptor!,
        title: this.ui_.defaultTaskMenuItem.label,
        get iconUrl() {
          console.assert(false);
          return '';
        },
        get isDefault() {
          console.assert(false);
          return false;
        },
        get isGenericFileHandler() {
          console.assert(false);
          return false;
        },
        isDlpBlocked: false,
      };

      tasks.execute(task);
    } catch (error: any) {
      if (error) {
        console.warn(error.stack || error);
      }
    }
  }

  /**
   * Get MIME type for an entry. This method first tries to obtain the MIME
   * type from metadata. If it fails, this falls back to obtain the MIME type
   * from its content or name.
   * @param entry An entry to obtain its mime type.
   */
  private async getMimeType_(entry: Entry|FilesAppEntry): Promise<string> {
    const properties =
        await this.metadataModel_.get([entry], ['contentMimeType']);
    if (properties && properties[0]!.contentMimeType) {
      return properties[0]!.contentMimeType;
    }
    const mimeType = await getMimeType(entry);
    return mimeType || '';
  }

  /**
   * Explicitly removes the cached tasks first and and re-calculates the
   * current tasks.
   */
  private clearCacheAndUpdateTasks_() {
    this.tasks_ = null;
    // Dispatch an empty fetch to invalidate any ongoing fetch.
    this.store_.dispatch(fetchFileTasks([]));
    this.updateTasks_();
  }

  private maybeHideButton(): boolean {
    // For the Store version the other conditions are checked in the store.
    const shouldDisableTasks = (this.selectionTasks_?.tasks ?? []).length === 0;

    if (shouldDisableTasks) {
      this.ui_.taskMenuButton.hidden = true;
      this.updateContextMenuTaskItems_([]);
      if (window.IN_TEST) {
        this.ui_.taskMenuButton.toggleAttribute('get-tasks-completed', true);
      }
      return true;
    }

    return false;
  }

  /** Updates available tasks opened from context menu or the open button.  */
  private async updateTasks_() {
    if (this.maybeHideButton()) {
      return;
    }

    try {
      const metricName = 'UpdateAvailableApps';
      startInterval(metricName);
      const tasks = await this.getFileTasks();
      // Update the DOM.
      this.display_(tasks);
      const openTaskItems = tasks.getAnnotatedTasks();
      this.updateContextMenuTaskItems_(
          openTaskItems, tasks.getPolicyDefaultHandlerStatus());
      if (window.IN_TEST) {
        this.ui_.taskMenuButton.toggleAttribute('get-tasks-completed', true);
      }
      recordDirectoryListLoadWithTolerance(
          metricName, openTaskItems.length, [10, 100], /*tolerance=*/ 0.8);
    } catch (error: any) {
      if (error) {
        console.warn(error.stack || error);
      }
    }
  }

  async getFileTasks(): Promise<FileTasks> {
    return this.getFileTasksStore_();
  }

  private async getFileTasksStore_(): Promise<FileTasks> {
    if (this.tasks_) {
      return this.tasks_!;
    }

    if (this.selectionKeys_ === undefined) {
      throw new Error('No selection to fulfill getFileTasks()');
    }
    // Request to fetch the tasks just to double check.
    this.store_.dispatch(fetchFileTasks(this.selectionFilesData_));
    await waitForState(
        this.store_,
        (st: State) => st.currentDirectory?.selection.fileTasks.status ===
            PropStatus.SUCCESS);
    const entries = this.selectionFilesData_.map(fd => fd.entry) as Entry[];
    this.tasks_ = Promise.resolve(FileTasks.fromStoreTasks(
        this.selectionTasks_!, this.volumeManager_, this.metadataModel_,
        this.directoryModel_, this.ui_, this.fileTransferController_!, entries,
        this.taskHistory_, this.progressCenter_, this));
    return this.tasks_;
  }

  /** Returns whether default task command can be executed or not. */
  canExecuteDefaultTask(): boolean {
    return this.canExecuteDefaultTask_;
  }

  /** Returns whether default task command should be hidden or not. */
  shouldHideDefaultTask(): boolean {
    return this.shouldHideDefaultTask_;
  }

  /** Returns whether open with command can be executed or not. */
  canExecuteOpenActions(): boolean {
    return this.canExecuteOpenActions_;
  }

  /**
   * Updates tasks menu item to match passed task items.
   * @param openTasks List of OPEN tasks.
   */
  private updateContextMenuTaskItems_(
      tasks: AnnotatedTask[],
      policyDefaultHandlerStatus?:
          chrome.fileManagerPrivate.PolicyDefaultHandlerStatus) {
    const taskCount = tasks.length;
    const defaultTask =
        getDefaultTask(tasks, policyDefaultHandlerStatus, this.taskHistory_);

    if (taskCount > 0) {
      if (defaultTask) {
        const menuItem = this.ui_.defaultTaskMenuItem;
        menuItem.setIsDefaultAttribute();
        /**
         * Menu icon can be controlled by either `iconEndImage` or
         * `iconEndFileType`, since the default task menu item DOM is shared,
         * before updating it, we should remove the previous one, e.g. reset
         * both `iconEndImage` and `iconEndFileType`.
         */
        menuItem.iconEndImage = '';
        menuItem.removeIconEndFileType();

        // If default is set by policy, we hide the original app icon and show
        // only the managed one.
        if (policyDefaultHandlerStatus) {
          menuItem.setIconEndHidden(true);
          menuItem.toggleManagedIcon(/*visible=*/ true);
        } else {
          menuItem.setIconEndHidden(false);
          menuItem.toggleManagedIcon(/*visible=*/ false);

          // iconType is defined for some tasks in FileTasks.annotate_().
          const iconType: string = (defaultTask as any).iconType;
          if (iconType) {
            menuItem.iconEndFileType = iconType;
          } else if (defaultTask.iconUrl) {
            menuItem.iconEndImage = 'url(' + defaultTask.iconUrl + ')';
          } else {
            menuItem.setIconEndHidden(true);
          }
        }

        menuItem.label = defaultTask.title;
        menuItem.descriptor = defaultTask.descriptor;
      }
    }

    this.canExecuteDefaultTask_ =
        defaultTask !== null && !defaultTask.isDlpBlocked;
    this.shouldHideDefaultTask_ = defaultTask === null;
    this.defaultTaskCommand_.canExecuteChange(this.ui_.listContainer.element);
    this.canExecuteOpenActions_ =
        taskCount > 1 || (taskCount === 1 && !defaultTask);
    this.openWithCommand_.canExecuteChange(this.ui_.listContainer.element);

    this.ui_.tasksSeparator.hidden = taskCount === 0;
  }

  /**
   * Return the tasks for the `entry`.
   * @param entry
   */
  async getEntryFileTasks(entry: Entry|FilesAppEntry): Promise<FileTasks> {
    return FileTasks.create(
        this.volumeManager_, this.metadataModel_, this.directoryModel_,
        this.ui_, this.fileTransferController_!, [entry], this.taskHistory_,
        this.crostini_, this.progressCenter_, this);
  }

  async executeEntryTask(entry: Entry) {
    const tasks = await this.getEntryFileTasks(entry);
    tasks.executeDefault();
  }

  /** Removes information about an extract archive task.  */
  private deleteExtractTaskDetails_(taskId: number) {
    this.extractTasks_.delete(taskId);
  }

  private onIoTaskProgressStatus_(
      event: chrome.fileManagerPrivate.ProgressStatus) {
    const taskId = event.taskId;
    if (!taskId) {
      console.warn('IOTask ProgressStatus without taskId');
      return;
    }

    // TaskController only manages IOTasks related to zip extract that were
    // started in this window.
    if (!(this.extractTasks_.has(taskId) &&
          event.type === chrome.fileManagerPrivate.IoTaskType.EXTRACT)) {
      return;
    }

    switch (event.state) {
      case chrome.fileManagerPrivate.IoTaskState.SUCCESS:
      case chrome.fileManagerPrivate.IoTaskState.CANCELLED:
      case chrome.fileManagerPrivate.IoTaskState.ERROR:
        this.deleteExtractTaskDetails_(taskId);
        break;
      case chrome.fileManagerPrivate.IoTaskState.NEED_PASSWORD:
        this.handleMissingPassword_(taskId);
        break;
    }
  }

  /**
   * Starts the Zip extract Here IO Task.
   * @param {!Array<!Entry|FilesAppEntry>} entries
   * @param {!DirectoryEntry|!FilesAppDirEntry} destination
   * @return {!Promise<void>} resolved with taskId.
   */
  async startExtractIoTask(
      entries: Array<Entry|FilesAppEntry>,
      destination: DirectoryEntry|FilesAppDirEntry): Promise<void> {
    const params = {
      destinationFolder: destination,
    } as chrome.fileManagerPrivate.IoTaskParams;
    return this.startExtractTask_(entries, params);
  }

  /**
   * Starts extraction for a single entry and stores the task details.
   * @return resolved with taskId.
   */
  private async startExtractTask_(
      entries: Array<Entry|FilesAppEntry>,
      params: chrome.fileManagerPrivate.IoTaskParams): Promise<void> {
    try {
      const taskId = await startIOTask(
          chrome.fileManagerPrivate.IoTaskType.EXTRACT, entries, params);
      this.extractTasks_.set(taskId, {entries, params});
    } catch (error: any) {
      console.warn('Error getting extract taskID', error);
    }
  }

  /**
   * Triggers a password dialog and starts an extract task with the
   * password (unless cancel is clicked on the dialog).
   */
  private async startGetPasswordThenExtractTask_(
      entry: Entry|FilesAppEntry,
      params: chrome.fileManagerPrivate.IoTaskParams) {
    let password: string|null = null;
    // Ask for password.
    try {
      const dialog = this.ui_.passwordDialog as XfPasswordDialog;
      password = await dialog.askForPassword(entry.fullPath, password);
    } catch (error) {
      console.warn('User cancelled password fetch ', error);
      return;
    }

    params['password'] = password;
    await this.startExtractTask_([entry], params);
  }

  /**
   * If an extract operation has finished due to missing password,
   * see if we have the operation stored and if so, pop up a password
   * dialog and try to restart another IO operation for it.
   */
  private handleMissingPassword_(taskId: number) {
    const existingOperation = this.extractTasks_.get(taskId);
    if (existingOperation) {
      // If we have multiple entries (from a multi-select extract) then
      // we need to start a new task for each of them individually so
      // that the password dialog is presented once for every file
      // that's encrypted.
      const selectionEntries = existingOperation['entries'];
      const params = existingOperation['params'];
      if (selectionEntries.length === 1) {
        this.startGetPasswordThenExtractTask_(selectionEntries[0]!, params);
      } else {
        for (const entry of selectionEntries) {
          this.startExtractTask_([entry], params);
        }
      }
    }
    // Remove the failed operation reference since it's finished.
    this.deleteExtractTaskDetails_(taskId);
  }
}

/** Type of the task in the dropdown menu. */
enum TaskMenuItemType {
  SHOW_MENU = 'ShowMenu',
  RUN_TASK = 'RunTask',
  CHANGE_DEFAULT_TASK = 'ChangeDefaultTask',
}

/** Item in the dropdown menu. */
export interface DropdownItem {
  type: TaskMenuItemType;
  label: string;
  iconUrl?: string;
  iconType?: string;
  task?: chrome.fileManagerPrivate.FileTask;
  isDefault?: boolean;
  isPolicyDefault?: boolean;
  isGenericFileHandler?: boolean;
  isDlpBlocked?: boolean;
  class
  ?: string;
}

/**
 * Creates dropdown item based on task.
 * @param isDefault Mark the item as default item.
 */
function createDropdownItem(
    task: chrome.fileManagerPrivate.FileTask, title?: string,
    isDefault?: boolean, isPolicyDefault?: boolean): DropdownItem {
  return {
    type: TaskMenuItemType.RUN_TASK,
    label: title || task.title,
    iconUrl: task.iconUrl || '',
    iconType: (task as AnnotatedTask).iconType || '',
    task: task,
    isDefault: isDefault || false,
    isPolicyDefault: isPolicyDefault || false,
    isGenericFileHandler: task.isGenericFileHandler,
    isDlpBlocked: task.isDlpBlocked,
  };
}
