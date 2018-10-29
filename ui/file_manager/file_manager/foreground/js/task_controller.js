// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {DialogType} dialogType
 * @param {!VolumeManager} volumeManager
 * @param {!FileManagerUI} ui
 * @param {!MetadataModel} metadataModel
 * @param {!DirectoryModel} directoryModel
 * @param {!FileSelectionHandler} selectionHandler
 * @param {!MetadataUpdateController} metadataUpdateController
 * @constructor
 * @struct
 */
function TaskController(
    dialogType, volumeManager, ui, metadataModel, directoryModel,
    selectionHandler, metadataUpdateController) {
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
   * @private {boolean}
   */
  this.canExecuteMoreActions_ = false;

  /**
   * @private {!cr.ui.Command}
   * @const
   */
  this.defaultTaskCommand_ = assertInstanceof(
      document.querySelector('#default-task'), cr.ui.Command);

  /**
   * More actions command that uses #open-with as selector due to the open-with
   * command used previously for the same task.
   * @private {!cr.ui.Command}
   * @const
   */
  this.openWithCommand_ =
      assertInstanceof(document.querySelector('#open-with'), cr.ui.Command);

  /**
   * More actions command that uses #open-with as selector due to the open-with
   * command used previously for the same task.
   * @private {!cr.ui.Command}
   * @const
   */
  this.moreActionsCommand_ =
      assertInstanceof(document.querySelector('#more-actions'), cr.ui.Command);

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
   * Selected entries from the last time onSelectionChanged_ was called.
   * @private {!Array<!Entry>}
   */
  this.lastSelectedEntries_ = [];

  ui.taskMenuButton.addEventListener(
      'select', this.onTaskItemClicked_.bind(this));
  ui.shareMenuButton.menu.addEventListener(
      'activate', this.onTaskItemClicked_.bind(this));
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
 * Cached the temporary disabled task item. Used inside
 * FileSelectionHandler.createTemporaryDisabledTaskItem_().
 * @type {Object}
 * @private
 */
TaskController.cachedDisabledTaskItem_ = null;

/**
 * Create the temporary disabled task item.
 * @return {Object} Created disabled item.
 * @private
 */
TaskController.createTemporaryDisabledTaskItem_ = function() {
  if (!TaskController.cachedDisabledTaskItem_) {
    TaskController.cachedDisabledTaskItem_ = {
      title: str('TASK_OPEN'),
      disabled: true,
      taskId: null
    };
  }

  return TaskController.cachedDisabledTaskItem_;
};

/**
 * Task combobox handler.
 *
 * @param {Object} event Event containing task which was clicked.
 * @private
 */
TaskController.prototype.onTaskItemClicked_ = function(event) {
  // If the clicked target has an associated command, the click event should not
  // be handled here since it is handled as a command.
  if (event.target && event.target.command)
    return;

  // 'select' event from ComboButton has the item as event.item.
  // 'activate' event from cr.ui.MenuButton has the item as event.target.data.
  var item = event.item || event.target.data;
  this.getFileTasks()
      .then(function(tasks) {
        switch (item.type) {
          case FileTasks.TaskMenuButtonItemType.ShowMenu:
            this.ui_.taskMenuButton.showMenu(false);
            break;
          case FileTasks.TaskMenuButtonItemType.RunTask:
            tasks.execute(item.task);
            break;
          case FileTasks.TaskMenuButtonItemType.ChangeDefaultTask:
            var selection = this.selectionHandler_.selection;
            var extensions = [];

            for (var i = 0; i < selection.entries.length; i++) {
              var match = /\.(\w+)$/g.exec(selection.entries[i].toURL());
              if (match) {
                var ext = match[1].toUpperCase();
                if (extensions.indexOf(ext) == -1) {
                  extensions.push(ext);
                }
              }
            }

            var format = '';

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
      }.bind(this))
      .catch(function(error) {
        if (error)
          console.error(error.stack || error);
      });
};

/**
 * Sets the given task as default, when this task is applicable.
 *
 * @param {!FileSelection} selection File selection.
 * @param {Object} task Task to set as default.
 * @private
 */
TaskController.prototype.changeDefaultTask_ = function(selection, task) {
  var entries = selection.entries;

  Promise.all(entries.map((entry) => this.getMimeType_(entry))).then(function(
      mimeTypes) {
    chrome.fileManagerPrivate.setDefaultTask(
        task.taskId,
        entries,
        mimeTypes,
        util.checkAPIError);
    this.metadataUpdateController_.refreshCurrentDirectoryMetadata();

    // Update task menu button unless the task button was updated other
    // selection.
    if (this.selectionHandler_.selection === selection) {
      this.tasks_ = null;
      this.getFileTasks()
          .then(function(tasks) {
            tasks.display(this.ui_.taskMenuButton, this.ui_.shareMenuButton);
          }.bind(this))
          .catch(function(error) {
            if (error)
              console.error(error.stack || error);
          });
    }
    this.selectionHandler_.onFileSelectionChanged();
  }.bind(this));
};

/**
 * Executes default task.
 */
TaskController.prototype.executeDefaultTask = function() {
  this.getFileTasks()
      .then(function(tasks) {
        var task = {
          taskId: this.ui_.fileContextMenu.defaultTaskMenuItem.taskId,
          title: this.ui_.fileContextMenu.defaultTaskMenuItem.label,
        };
        tasks.execute(task);
      }.bind(this))
      .catch(function(error) {
        if (error)
          console.error(error.stack || error);
      });
};

/**
 * Get MIME type for an entry. This method first tries to obtain the MIME type
 * from metadata. If it fails, this falls back to obtain the MIME type from its
 * content or name.
 *
 * @param {!Entry} entry An entry to obtain its mime type.
 * @return {!Promise}
 * @private
 */
TaskController.prototype.getMimeType_ = function(entry) {
  return this.metadataModel_.get([entry], ['contentMimeType']).then(
      function(properties) {
        if (properties[0].contentMimeType)
          return properties[0].contentMimeType;
        return new Promise(function(fulfill, reject) {
          chrome.fileManagerPrivate.getMimeType(
              entry, function(mimeType) {
                if (!chrome.runtime.lastError)
                  fulfill(mimeType);
                else
                  reject(chrome.runtime.lastError);
              });
        });
      });
};

/**
 * Handles change of selection and clears context menu.
 * @private
 */
TaskController.prototype.onSelectionChanged_ = function() {
  var selection = this.selectionHandler_.selection;
  // Caller of update context menu task items.
  // FileSelectionHandler.EventType.CHANGE
  if (this.dialogType_ === DialogType.FULL_PAGE &&
      (selection.directoryCount > 0 || selection.fileCount > 0)) {
    // Compare entries while ignoring changes inside directories.
    if (!util.isSameEntries(this.lastSelectedEntries_, selection.entries)) {
      // Show disabled items for position calculation of the menu. They will be
      // overridden in this.updateTasks_().
      this.updateContextMenuTaskItems_(
          [TaskController.createTemporaryDisabledTaskItem_()],
          [TaskController.createTemporaryDisabledTaskItem_()]);
    }
  } else {
    // Update context menu.
    this.updateContextMenuTaskItems_([], []);
  }
  this.lastSelectedEntries_ = selection.entries;
};

/**
 * Updates available tasks opened from context menu or the open button.
 * @private
 */
TaskController.prototype.updateTasks_ = function() {
  var selection = this.selectionHandler_.selection;
  if (this.dialogType_ === DialogType.FULL_PAGE &&
      (selection.directoryCount > 0 || selection.fileCount > 0)) {
    this.getFileTasks()
        .then(function(tasks) {
          tasks.display(this.ui_.taskMenuButton, this.ui_.shareMenuButton);
          this.updateContextMenuTaskItems_(
              tasks.getOpenTaskItems(), tasks.getNonOpenTaskItems());
        }.bind(this))
        .catch(function(error) {
          if (error)
            console.error(error.stack || error);
        });
  } else {
    this.ui_.taskMenuButton.hidden = true;
    this.ui_.shareMenuButton.hidden = true;
  }
};

/**
 * @return {!Promise<!FileTasks>}
 * @public
 */
TaskController.prototype.getFileTasks = function() {
  var selection = this.selectionHandler_.selection;
  if (this.tasks_ && util.isSameEntries(this.tasksEntries_, selection.entries))
    return this.tasks_;
  this.tasksEntries_ = selection.entries;
  this.tasks_ =
      selection.computeAdditional(this.metadataModel_).then(function() {
        if (this.selectionHandler_.selection !== selection) {
          if (util.isSameEntries(this.tasksEntries_, selection.entries))
            this.tasks_ = null;
          return Promise.reject();
        }
        return FileTasks
            .create(
                this.volumeManager_, this.metadataModel_, this.directoryModel_,
                this.ui_, selection.entries, assert(selection.mimeTypes),
                this.taskHistory_)
            .then(function(tasks) {
              if (this.selectionHandler_.selection !== selection) {
                if (util.isSameEntries(this.tasksEntries_, selection.entries))
                  this.tasks_ = null;
                return Promise.reject();
              }
              return tasks;
            }.bind(this));
      }.bind(this));
  return this.tasks_;
};

/**
 * Returns whether default task command can be executed or not.
 * @return {boolean} True if default task command is executable.
 */
TaskController.prototype.canExecuteDefaultTask = function() {
  return this.canExecuteDefaultTask_;
};

/**
 * Returns whether open with command can be executed or not.
 * @return {boolean} True if open with command is executable.
 */
TaskController.prototype.canExecuteOpenActions = function() {
  return this.canExecuteOpenActions_;
};

/**
 * Returns whether open with command can be executed or not.
 * @return {boolean} True if open with command is executable.
 */
TaskController.prototype.canExecuteMoreActions = function() {
  return this.canExecuteMoreActions_;
};

/**
 * Updates tasks menu item to match passed task items.
 *
 * @param {!Array<!Object>} openTasks List of OPEN tasks.
 * @param {!Array<!Object>} nonOpenTasks List of non-OPEN tasks.
 * @private
 */
TaskController.prototype.updateContextMenuTaskItems_ = function(
    openTasks, nonOpenTasks) {
  var defaultTask = FileTasks.getDefaultTask(openTasks, this.taskHistory_);
  if (defaultTask) {
    if (defaultTask.iconType) {
      this.ui_.fileContextMenu.defaultTaskMenuItem.style.backgroundImage = '';
      this.ui_.fileContextMenu.defaultTaskMenuItem.setAttribute(
          'file-type-icon', defaultTask.iconType);
    } else if (defaultTask.iconUrl) {
      this.ui_.fileContextMenu.defaultTaskMenuItem.style.backgroundImage =
          'url(' + defaultTask.iconUrl + ')';
    } else {
      this.ui_.fileContextMenu.defaultTaskMenuItem.style.backgroundImage = '';
    }

    if (defaultTask.taskId === FileTasks.ZIP_ARCHIVER_UNZIP_TASK_ID)
      this.ui_.fileContextMenu.defaultTaskMenuItem.label = str('TASK_OPEN');
    else
      this.ui_.fileContextMenu.defaultTaskMenuItem.label = defaultTask.title;

    this.ui_.fileContextMenu.defaultTaskMenuItem.disabled =
        !!defaultTask.disabled;
    this.ui_.fileContextMenu.defaultTaskMenuItem.taskId = defaultTask.taskId;
  }

  this.canExecuteDefaultTask_ = defaultTask != null;
  this.defaultTaskCommand_.canExecuteChange(this.ui_.listContainer.element);

  this.canExecuteOpenActions_ = openTasks.length > 1;
  this.openWithCommand_.canExecuteChange(this.ui_.listContainer.element);

  this.canExecuteMoreActions_ = nonOpenTasks.length >= 1;
  this.moreActionsCommand_.canExecuteChange(this.ui_.listContainer.element);

  this.ui_.fileContextMenu.tasksSeparator.hidden =
      openTasks.length === 0 && nonOpenTasks.length == 0;
};

/**
 * @param {FileEntry} entry
 */
TaskController.prototype.executeEntryTask = function(entry) {
  this.metadataModel_.get([entry], ['contentMimeType']).then(function(props) {
    FileTasks
        .create(
            this.volumeManager_, this.metadataModel_, this.directoryModel_,
            this.ui_, [entry], [props[0].contentMimeType || null],
            this.taskHistory_)
        .then(function(tasks) {
          tasks.executeDefault();
        });
  }.bind(this));
};
