// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents a collection of available tasks to execute for a specific list
 * of entries.
 *
 * @param {!VolumeManager} volumeManager
 * @param {!MetadataModel} metadataModel
 * @param {!DirectoryModel} directoryModel
 * @param {!FileManagerUI} ui
 * @param {!Array<!Entry>} entries
 * @param {!Array<?string>} mimeTypes
 * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
 * @param {chrome.fileManagerPrivate.FileTask} defaultTask
 * @param {!TaskHistory} taskHistory
 * @constructor
 * @struct
 */
function FileTasks(
    volumeManager, metadataModel, directoryModel, ui, entries, mimeTypes, tasks,
    defaultTask, taskHistory) {
  /**
   * @private {!VolumeManager}
   * @const
   */
  this.volumeManager_ = volumeManager;

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
   * @private {!FileManagerUI}
   * @const
   */
  this.ui_ = ui;

  /**
   * @private {!Array<!Entry>}
   * @const
   */
  this.entries_ = entries;

  /**
   * @private {!Array<?string>}
   * @const
   */
  this.mimeTypes_ = mimeTypes;

  /**
   * @private {!Array<!chrome.fileManagerPrivate.FileTask>}
   * @const
   */
  this.tasks_ = tasks;

  /**
   * @private {chrome.fileManagerPrivate.FileTask}
   * @const
   */
  this.defaultTask_ = defaultTask;

  /**
   * @private {!TaskHistory}
   * @const
   */
  this.taskHistory_ = taskHistory;
}

FileTasks.prototype = {
  /**
   * @return {!Array<!Entry>}
   */
  get entries() {
    return this.entries_;
  }
};

/**
 * The app ID of the video player app.
 * @const
 * @type {string}
 */
FileTasks.VIDEO_PLAYER_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

/**
 * The task id of the zip unpacker app.
 * @const
 * @type {string}
 */
FileTasks.ZIP_UNPACKER_TASK_ID = 'oedeeodfidgoollimchfdnbmhcpnklnd|app|zip';

/**
 * The task id of unzip action of Zip Archiver app.
 * @const
 * @type {string}
 */
FileTasks.ZIP_ARCHIVER_UNZIP_TASK_ID =
    'dmboannefpncccogfdikhmhpmdnddgoe|app|open';

/**
 * The task id of zip action of Zip Archiver app.
 * @const
 * @type {string}
 */
FileTasks.ZIP_ARCHIVER_ZIP_TASK_ID =
    'dmboannefpncccogfdikhmhpmdnddgoe|app|pack';

/**
 * The task id of zip action of Zip Archiver app, using temporary dir as workdir
 * @const
 * @type {string}
 */
FileTasks.ZIP_ARCHIVER_ZIP_USING_TMP_TASK_ID =
    'dmboannefpncccogfdikhmhpmdnddgoe|app|pack_using_tmp';


/**
 * Available tasks in task menu button.
 * @enum {string}
 */
FileTasks.TaskMenuButtonItemType = {
  ShowMenu: 'ShowMenu',
  RunTask: 'RunTask',
  ChangeDefaultTask: 'ChangeDefaultTask'
};

/**
 * Dialog types to show a task picker.
 * @enum {string}
 */
FileTasks.TaskPickerType = {
  ChangeDefault: 'ChangeDefault',
  OpenWith: 'OpenWith',
  MoreActions: 'MoreActions'
};

/**
 * Creates an instance of FileTasks for the specified list of entries with mime
 * types.
 *
 * @param {!VolumeManager} volumeManager
 * @param {!MetadataModel} metadataModel
 * @param {!DirectoryModel} directoryModel
 * @param {!FileManagerUI} ui
 * @param {!Array<!Entry>} entries
 * @param {!Array<?string>} mimeTypes
 * @param {!TaskHistory} taskHistory
 * @return {!Promise<!FileTasks>}
 */
FileTasks.create = function(
    volumeManager, metadataModel, directoryModel, ui, entries, mimeTypes,
    taskHistory) {
  var tasksPromise = new Promise(function(fulfill) {
    // getFileTasks supports only native entries.
    entries = entries.filter(util.isNativeEntry);
    if (entries.length === 0) {
      fulfill([]);
      return;
    }
    chrome.fileManagerPrivate.getFileTasks(entries, function(taskItems) {
      if (chrome.runtime.lastError) {
        console.error('Failed to fetch file tasks due to: ' +
            chrome.runtime.lastError.message);
        Promise.reject();
        return;
      }

      // Linux package installation is currently only supported for a single
      // file which is inside the Linux container, or in a sharable volume.
      // TODO(timloh): Instead of filtering these out, we probably should show
      // a dialog with an error message, similar to when attempting to run
      // Crostini tasks with non-Crostini entries.
      if (entries.length !== 1 ||
          !(Crostini.isCrostiniEntry(entries[0], volumeManager) ||
            Crostini.canSharePath(
                entries[0], false /* persist */, volumeManager))) {
        taskItems = taskItems.filter(function(item) {
          var taskParts = item.taskId.split('|');
          var appId = taskParts[0];
          var taskType = taskParts[1];
          var actionId = taskParts[2];
          return !(
              appId === chrome.runtime.id && taskType === 'file' &&
              actionId === 'install-linux-package');
        });
      }

      // Filters out Pack with Zip Archiver task because it will be accessible
      // via 'Zip selection' context menu button
      taskItems = taskItems.filter(function(item) {
        return item.taskId !== FileTasks.ZIP_ARCHIVER_ZIP_TASK_ID &&
            item.taskId !== FileTasks.ZIP_ARCHIVER_ZIP_USING_TMP_TASK_ID;
      });

      fulfill(FileTasks.annotateTasks_(assert(taskItems), entries));
    });
  });

  var defaultTaskPromise = tasksPromise.then(function(tasks) {
    return FileTasks.getDefaultTask(tasks, taskHistory);
  });

  return Promise.all([tasksPromise, defaultTaskPromise]).then(function(args) {
    return new FileTasks(
        volumeManager, metadataModel, directoryModel, ui, entries, mimeTypes,
        args[0], args[1], taskHistory);
  });
};

/**
 * Obtains the task items.
 * @return {!Array<!chrome.fileManagerPrivate.FileTask>}
 */
FileTasks.prototype.getTaskItems = function() {
  return this.tasks_;
};

/**
 * Obtain tasks which are categorized as OPEN tasks.
 * @return {!Array<!chrome.fileManagerPrivate.FileTask>}
 */
FileTasks.prototype.getOpenTaskItems = function() {
  return this.tasks_.filter(FileTasks.isOpenTask);
};

/**
 * Obtain tasks which are not categorized as OPEN tasks.
 * @return {!Array<!chrome.fileManagerPrivate.FileTask>}
 */
FileTasks.prototype.getNonOpenTaskItems = function() {
  return this.tasks_.filter(task => !FileTasks.isOpenTask(task));
};

/**
 * Opens the suggest file dialog.
 *
 * @param {function()} onSuccess Success callback.
 * @param {function()} onCancelled User-cancelled callback.
 * @param {function()} onFailure Failure callback.
 */
FileTasks.prototype.openSuggestAppsDialog = function(
    onSuccess, onCancelled, onFailure) {
  if (this.entries_.length !== 1) {
    onFailure();
    return;
  }

  var entry = this.entries_[0];
  var mimeType = this.mimeTypes_[0];
  var basename = entry.name;
  var splitted = util.splitExtension(basename);
  var extension = splitted[1];

  // Returns with failure if the file has neither extension nor MIME type.
  if (!extension && !mimeType) {
    onFailure();
    return;
  }

  var onDialogClosed = function(result, itemId) {
    switch (result) {
      case SuggestAppsDialog.Result.SUCCESS:
        onSuccess();
        break;
      case SuggestAppsDialog.Result.FAILED:
        onFailure();
        break;
      default:
        onCancelled();
    }
  };

  this.ui_.suggestAppsDialog.showByExtensionAndMime(
      extension, mimeType, onDialogClosed);
};

/**
 * The list of known extensions to record UMA.
 * Note: Because the data is recorded by the index, so new item shouldn't be
 * inserted.
 * Must match the ViewFileType entry in enums.xml.
 *
 * @const
 * @type {Array<string>}
 */
FileTasks.UMA_INDEX_KNOWN_EXTENSIONS = Object.freeze([
  'other',     '.3ga',         '.3gp',
  '.aac',      '.alac',        '.asf',
  '.avi',      '.bmp',         '.csv',
  '.doc',      '.docx',        '.flac',
  '.gif',      '.jpeg',        '.jpg',
  '.log',      '.m3u',         '.m3u8',
  '.m4a',      '.m4v',         '.mid',
  '.mkv',      '.mov',         '.mp3',
  '.mp4',      '.mpg',         '.odf',
  '.odp',      '.ods',         '.odt',
  '.oga',      '.ogg',         '.ogv',
  '.pdf',      '.png',         '.ppt',
  '.pptx',     '.ra',          '.ram',
  '.rar',      '.rm',          '.rtf',
  '.wav',      '.webm',        '.webp',
  '.wma',      '.wmv',         '.xls',
  '.xlsx',     '.crdownload',  '.crx',
  '.dmg',      '.exe',         '.html',
  '.htm',      '.jar',         '.ps',
  '.torrent',  '.txt',         '.zip',
  'directory', 'no extension', 'unknown extension',
  '.mhtml',    '.gdoc',        '.gsheet',
  '.gslides'
]);

/**
 * The list of extensions to skip the suggest app dialog.
 * @const
 * @type {Array<string>}
 * @private
 */
FileTasks.EXTENSIONS_TO_SKIP_SUGGEST_APPS_ = Object.freeze([
  '.crdownload', '.dsc', '.inf', '.crx',
]);

/**
 * Task IDs of the zip file handlers to be recorded.
 * The indexes of the IDs must match with the values of
 * FileManagerZipHandlerType in enums.xml, and should not change.
 */
FileTasks.UMA_ZIP_HANDLER_TASK_IDS_ = Object.freeze([
  FileTasks.ZIP_UNPACKER_TASK_ID, FileTasks.ZIP_ARCHIVER_UNZIP_TASK_ID,
  FileTasks.ZIP_ARCHIVER_ZIP_TASK_ID
]);

/**
 * Records trial of opening file grouped by extensions.
 *
 * @param {Array<!Entry>} entries The entries to be opened.
 * @private
 */
FileTasks.recordViewingFileTypeUMA_ = function(entries) {
  for (var i = 0; i < entries.length; i++) {
    var entry = entries[i];
    var extension = FileType.getExtension(entry).toLowerCase();
    if (FileTasks.UMA_INDEX_KNOWN_EXTENSIONS.indexOf(extension) < 0) {
      extension = 'other';
    }
    metrics.recordEnum(
        'ViewingFileType', extension, FileTasks.UMA_INDEX_KNOWN_EXTENSIONS);
  }
};

/**
 * Records trial of opening file grouped by root types.
 *
 * @param {?VolumeManagerCommon.RootType} rootType The type of the root where
 *     entries are being opened.
 * @private
 */
FileTasks.recordViewingRootTypeUMA_ = function(rootType) {
  if (rootType !== null) {
    metrics.recordEnum(
        'ViewingRootType', rootType, VolumeManagerCommon.RootTypesForUMA);
  }
};

FileTasks.recordZipHandlerUMA_ = function(taskId) {
  if (FileTasks.UMA_ZIP_HANDLER_TASK_IDS_.indexOf(taskId) != -1) {
    metrics.recordEnum(
        'ZipFileTask', taskId, FileTasks.UMA_ZIP_HANDLER_TASK_IDS_);
  }
};

/**
 * Crostini Share Dialog types.
 * Keep in sync with enums.xml FileManagerCrostiniShareDialogType.
 * @enum {string}
 */
FileTasks.CrostiniShareDialogType = {
  None: 'None',
  ShareBeforeOpen: 'ShareBeforeOpen',
  UnableToOpen: 'UnableToOpen',
};

/**
 * The indexes of these types must match with the values of
 * FileManagerCrostiniShareDialogType in enums.xml, and should not change.
 */
FileTasks.UMA_CROSTINI_SHARE_DIALOG_TYPES_ = Object.freeze([
  FileTasks.CrostiniShareDialogType.None,
  FileTasks.CrostiniShareDialogType.ShareBeforeOpen,
  FileTasks.CrostiniShareDialogType.UnableToOpen,
]);


/**
 * Records the type of dialog shown when using a crostini app to open a file.
 * @param {!FileTasks.CrostiniShareDialogType} dialogType
 * @private
 */
FileTasks.recordCrostiniShareDialogTypeUMA_ = function(dialogType) {
  metrics.recordEnum(
      'CrostiniShareDialog', dialogType,
      FileTasks.UMA_CROSTINI_SHARE_DIALOG_TYPES_);
};

/**
 * Returns true if the taskId is for an internal task.
 *
 * @param {string} taskId Task identifier.
 * @return {boolean} True if the task ID is for an internal task.
 * @private
 */
FileTasks.isInternalTask_ = function(taskId) {
  var taskParts = taskId.split('|');
  var appId = taskParts[0];
  var taskType = taskParts[1];
  var actionId = taskParts[2];
  return (
      appId === chrome.runtime.id && taskType === 'file' &&
      (actionId === 'mount-archive' || actionId === 'install-linux-package'));
};

/**
 * Returns true if the given task is categorized as an OPEN task.
 *
 * @param {!chrome.fileManagerPrivate.FileTask} task
 * @return {boolean} True if the given task is an OPEN task.
 */
FileTasks.isOpenTask = function(task) {
  // We consider following types of tasks as OPEN tasks.
  // - Files app's internal tasks
  // - file_handler tasks with OPEN_WITH verb
  return !task.verb || task.verb == chrome.fileManagerPrivate.Verb.OPEN_WITH;
};

/**
 * Annotates tasks returned from the API.
 *
 * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks Input tasks from
 *     the API.
 * @param {!Array<!Entry>} entries List of entries for the tasks.
 * @return {!Array<!chrome.fileManagerPrivate.FileTask>} Annotated tasks.
 * @private
 */
FileTasks.annotateTasks_ = function(tasks, entries) {
  var result = [];
  var id = chrome.runtime.id;
  for (var i = 0; i < tasks.length; i++) {
    var task = tasks[i];
    var taskParts = task.taskId.split('|');

    // Skip internal Files app's handlers.
    if (taskParts[0] === id &&
        (taskParts[2] === 'select' || taskParts[2] === 'open')) {
      continue;
    }

    // Tweak images, titles of internal tasks.
    if (taskParts[0] === id && taskParts[1] === 'file') {
      if (taskParts[2] === 'play') {
        // TODO(serya): This hack needed until task.iconUrl is working
        //             (see GetFileTasksFileBrowserFunction::RunImpl).
        task.iconType = 'audio';
        task.title = loadTimeData.getString('TASK_LISTEN');
      } else if (taskParts[2] === 'mount-archive') {
        task.iconType = 'archive';
        task.title = loadTimeData.getString('MOUNT_ARCHIVE');
      } else if (taskParts[2] === 'open-hosted-generic') {
        if (entries.length > 1)
          task.iconType = 'generic';
        else // Use specific icon.
          task.iconType = FileType.getIcon(entries[0]);
        task.title = loadTimeData.getString('TASK_OPEN');
      } else if (taskParts[2] === 'open-hosted-gdoc') {
        task.iconType = 'gdoc';
        task.title = loadTimeData.getString('TASK_OPEN_GDOC');
      } else if (taskParts[2] === 'open-hosted-gsheet') {
        task.iconType = 'gsheet';
        task.title = loadTimeData.getString('TASK_OPEN_GSHEET');
      } else if (taskParts[2] === 'open-hosted-gslides') {
        task.iconType = 'gslides';
        task.title = loadTimeData.getString('TASK_OPEN_GSLIDES');
      } else if (taskParts[2] === 'install-linux-package') {
        task.iconType = 'crostini';
        task.title = loadTimeData.getString('TASK_INSTALL_LINUX_PACKAGE');
      } else if (taskParts[2] === 'view-swf') {
        // Do not render this task if disabled.
        if (!loadTimeData.getBoolean('SWF_VIEW_ENABLED'))
          continue;
        task.iconType = 'generic';
        task.title = loadTimeData.getString('TASK_VIEW');
      } else if (taskParts[2] === 'view-pdf') {
        // Do not render this task if disabled.
        if (!loadTimeData.getBoolean('PDF_VIEW_ENABLED'))
          continue;
        task.iconType = 'pdf';
        task.title = loadTimeData.getString('TASK_VIEW');
      } else if (taskParts[2] === 'view-in-browser') {
        task.iconType = 'generic';
        task.title = loadTimeData.getString('TASK_VIEW');
      }
    }
    if (!task.iconType && taskParts[1] === 'web-intent') {
      task.iconType = 'generic';
    }

    // Add verb to title.
    if (task.verb) {
      var verbButtonLabel = '';
      switch (task.verb) {
        case chrome.fileManagerPrivate.Verb.ADD_TO:
          verbButtonLabel = 'ADD_TO_VERB_BUTTON_LABEL';
          break;
        case chrome.fileManagerPrivate.Verb.PACK_WITH:
          verbButtonLabel = 'PACK_WITH_VERB_BUTTON_LABEL';
          break;
        case chrome.fileManagerPrivate.Verb.SHARE_WITH:
          // Even when the task has SHARE_WITH verb, we don't prefix the title
          // with "Share with" when the task is from SEND/SEND_MULTIPLE intent
          // handlers from Android apps, since the title can already have an
          // appropriate verb.
          if (!(taskParts[1] == 'arc' &&
                (taskParts[2] == 'send' || taskParts[2] == 'send_multiple'))) {
            verbButtonLabel = 'SHARE_WITH_VERB_BUTTON_LABEL';
          }
          break;
        case chrome.fileManagerPrivate.Verb.OPEN_WITH:
          verbButtonLabel = 'OPEN_WITH_VERB_BUTTON_LABEL';
          break;
        default:
          console.error('Invalid task verb: ' + task.verb + '.');
      }
      if (verbButtonLabel)
        task.label = loadTimeData.getStringF(verbButtonLabel, task.title);
    }

    result.push(task);
  }

  return result;
};

/**
 * Checks if task is a crostini task and all entries are accessible to, or can
 * be shared with crostini.  Shares files as required if possible and invokes
 * callback, or shows Unable to Open error dialog and does not invoke callback.
 * @param {!chrome.fileManagerPrivate.FileTask} task Task to run.
 * @param {function()} callback Callback is called when all files (if any) are
 *   accessible to crostini, else error dialog is shown.
 * @private
 */
FileTasks.prototype.maybeShareWithCrostiniOrShowDialog_ = function(
    task, callback) {
  // Check if this is a crostini task.
  if (!Crostini.taskRequiresSharing(task))
    return callback();

  let showUnableToOpen = false;
  const entriesToShare = [];

  for (let i = 0; i < this.entries_.length; i++) {
    const entry = this.entries_[i];
    if (Crostini.isCrostiniEntry(entry, this.volumeManager_) ||
        Crostini.isPathShared(entry, this.volumeManager_)) {
      continue;
    }
    if (!Crostini.canSharePath(
            entry, false /* persist */, this.volumeManager_)) {
      showUnableToOpen = true;
      break;
    }
    entriesToShare.push(entry);
  }

  // Show unable to open alert dialog.
  if (showUnableToOpen) {
    this.ui_.alertDialog.showHtml(
        strf('UNABLE_TO_OPEN_CROSTINI_TITLE', task.title),
        strf('UNABLE_TO_OPEN_CROSTINI', task.title));
    FileTasks.recordCrostiniShareDialogTypeUMA_(
        FileTasks.CrostiniShareDialogType.UnableToOpen);
    return;
  }

  // No sharing required.
  if (entriesToShare.length === 0) {
    FileTasks.recordCrostiniShareDialogTypeUMA_(
        FileTasks.CrostiniShareDialogType.None);
    return callback();
  }

  // Share then invoke callback.
  FileTasks.recordCrostiniShareDialogTypeUMA_(
      FileTasks.CrostiniShareDialogType.ShareBeforeOpen);
  // Set persist to false when sharing paths to open with a crostini app.
  chrome.fileManagerPrivate.sharePathsWithCrostini(
      entriesToShare, false /* persist */, () => {
        // It is unexpected to get an error sharing any files since we have
        // already validated that all selected files can be shared.
        // But if it happens, log error, and do not execute callback.
        if (chrome.runtime.lastError) {
          return console.error(
              'Error sharing with linux to execute: ' +
              chrome.runtime.lastError.message);
        }
        // Register paths as shared, and now we are ready to execute.
        entriesToShare.forEach((entry) => {
          Crostini.registerSharedPath(entry, this.volumeManager_);
        });
        callback();
      });
};

/**
 * Executes default task.
 *
 * @param {function(boolean, Array<!Entry>)=} opt_callback Called when the
 *     default task is executed, or the error is occurred.
 */
FileTasks.prototype.executeDefault = function(opt_callback) {
  FileTasks.recordViewingFileTypeUMA_(this.entries_);
  FileTasks.recordViewingRootTypeUMA_(
      this.directoryModel_.getCurrentRootType());
  this.executeDefaultInternal_(opt_callback);
};

/**
 * Executes default task.
 *
 * @param {function(boolean, Array<!Entry>)=} opt_callback Called when the
 *     default task is executed, or the error is occurred.
 * @private
 */
FileTasks.prototype.executeDefaultInternal_ = function(opt_callback) {
  var callback = opt_callback || function(arg1, arg2) {};

  if (this.defaultTask_ !== null) {
    this.executeInternal_(this.defaultTask_);
    callback(true, this.entries_);
    return;
  }

  var nonGenericTasks = this.tasks_.filter(t => !t.isGenericFileHandler);
  // If there is only one task that is not a generic file handler, it should be
  // executed as a default task. If there are multiple tasks that are not
  // generic file handlers, and none of them are considered as default, we show
  // a task picker to ask the user to choose one.
  if (nonGenericTasks.length >= 2) {
    this.showTaskPicker(
        this.ui_.defaultTaskPicker, str('OPEN_WITH_BUTTON_LABEL'),
        '', function(task) {
          this.execute(task);
        }.bind(this), FileTasks.TaskPickerType.OpenWith);
    return;
  }

  // We don't have tasks, so try to show a file in a browser tab.
  // We only do that for single selection to avoid confusion.
  if (this.entries_.length !== 1)
    return;

  var filename = this.entries_[0].name;
  var extension = util.splitExtension(filename)[1] || null;
  var mimeType = this.mimeTypes_[0] || null;

  var showAlert = function() {
    var textMessageId;
    var titleMessageId;
    switch (extension) {
      case '.exe':
      case '.msi':
        textMessageId = 'NO_TASK_FOR_EXECUTABLE';
        break;
      case '.dmg':
        textMessageId = 'NO_TASK_FOR_DMG';
        break;
      case '.crx':
        textMessageId = 'NO_TASK_FOR_CRX';
        titleMessageId = 'NO_TASK_FOR_CRX_TITLE';
        break;
      default:
        textMessageId = 'NO_TASK_FOR_FILE';
    }

    var webStoreUrl = webStoreUtils.createWebStoreLink(extension, mimeType);
    var text = strf(textMessageId, webStoreUrl, str('NO_TASK_FOR_FILE_URL'));
    var title = titleMessageId ? str(titleMessageId) : filename;
    this.ui_.alertDialog.showHtml(title, text, null, null, null);
    callback(false, this.entries_);
  }.bind(this);

  var onViewFilesFailure = function() {
    if (extension &&
        (FileTasks.EXTENSIONS_TO_SKIP_SUGGEST_APPS_.indexOf(extension) !== -1 ||
         constants.EXECUTABLE_EXTENSIONS.indexOf(assert(extension)) !== -1)) {
      showAlert();
      return;
    }

    this.openSuggestAppsDialog(
        function() {
          FileTasks
              .create(
                  this.volumeManager_, this.metadataModel_,
                  this.directoryModel_, this.ui_, this.entries_,
                  this.mimeTypes_, this.taskHistory_)
              .then(
                  function(tasks) {
                    tasks.executeDefault();
                    callback(true, this.entries_);
                  }.bind(this),
                  function() {
                    callback(false, this.entries_);
                  }.bind(this));
        }.bind(this),
        // Cancelled callback.
        function() {
          callback(false, this.entries_);
        }.bind(this),
        showAlert);
  }.bind(this);

  var onViewFiles = function(result) {
    switch (result) {
      case 'opened':
        callback(true, this.entries_);
        break;
      case 'message_sent':
        util.isTeleported(window).then(function(teleported) {
          if (teleported)
            this.ui_.showOpenInOtherDesktopAlert(this.entries_);
        }.bind(this));
        callback(true, this.entries_);
        break;
      case 'empty':
        callback(true, this.entries_);
        break;
      case 'failed':
        onViewFilesFailure();
        break;
    }
  }.bind(this);

  this.checkAvailability_(function() {
    var taskId = chrome.runtime.id + '|file|view-in-browser';
    chrome.fileManagerPrivate.executeTask(taskId, this.entries_, onViewFiles);
  }.bind(this));
};

/**
 * Executes a single task.
 *
 * @param {chrome.fileManagerPrivate.FileTask} task FileTask.
 */
FileTasks.prototype.execute = function(task) {
  FileTasks.recordViewingFileTypeUMA_(this.entries_);
  FileTasks.recordViewingRootTypeUMA_(
      this.directoryModel_.getCurrentRootType());
  this.executeInternal_(task);
};

/**
 * The core implementation to execute a single task.
 *
 * @param {chrome.fileManagerPrivate.FileTask} task FileTask.
 * @private
 */
FileTasks.prototype.executeInternal_ = function(task) {
  this.checkAvailability_(() => {
    this.maybeShareWithCrostiniOrShowDialog_(task, () => {
      this.taskHistory_.recordTaskExecuted(task.taskId);
      if (FileTasks.isInternalTask_(task.taskId)) {
        this.executeInternalTask_(task.taskId);
      } else {
        FileTasks.recordZipHandlerUMA_(task.taskId);
        chrome.fileManagerPrivate.executeTask(
            task.taskId, this.entries_, (result) => {
              if (result !== 'message_sent')
                return;
              util.isTeleported(window).then((teleported) => {
                if (teleported)
                  this.ui_.showOpenInOtherDesktopAlert(this.entries_);
              });
            });
      }
    });
  });
};

/**
 * Ensures that the all files are available right now.
 *
 * Must not call before initialization.
 * @param {function()} callback Called when checking is completed and all files
 *     are available. Otherwise not called.
 * @private
 */
FileTasks.prototype.checkAvailability_ = function(callback) {
  var areAll = function(entries, props, name) {
    // TODO(cmihail): Make files in directories available offline.
    // See http://crbug.com/569767.
    var okEntriesNum = 0;
    for (var i = 0; i < entries.length; i++) {
      // If got no properties, we safely assume that item is available.
      if (props[i] && (props[i][name] || entries[i].isDirectory))
        okEntriesNum++;
    }
    return okEntriesNum === props.length;
  };

  var containsDriveEntries =
      this.entries_.some(function(entry) {
        var volumeInfo = this.volumeManager_.getVolumeInfo(entry);
        return volumeInfo && volumeInfo.volumeType ===
            VolumeManagerCommon.VolumeType.DRIVE;
      }.bind(this));

  // Availability is not checked for non-Drive files, as availableOffline, nor
  // availableWhenMetered are not exposed for other types of volumes at this
  // moment.
  if (!containsDriveEntries) {
    callback();
    return;
  }

  var isDriveOffline = this.volumeManager_.getDriveConnectionState().type ===
      VolumeManagerCommon.DriveConnectionType.OFFLINE;

  if (isDriveOffline) {
    this.metadataModel_.get(this.entries_, ['availableOffline', 'hosted']).then(
        function(props) {
          if (areAll(this.entries_, props, 'availableOffline')) {
            callback();
            return;
          }

          this.ui_.alertDialog.showHtml(
              loadTimeData.getString('OFFLINE_HEADER'),
              props[0].hosted ?
                  loadTimeData.getStringF(
                      this.entries_.length === 1 ?
                          'HOSTED_OFFLINE_MESSAGE' :
                          'HOSTED_OFFLINE_MESSAGE_PLURAL') :
                  loadTimeData.getStringF(
                      this.entries_.length === 1 ?
                          'OFFLINE_MESSAGE' :
                          'OFFLINE_MESSAGE_PLURAL',
                      loadTimeData.getString('OFFLINE_COLUMN_LABEL')),
              null, null, null);
    }.bind(this));
    return;
  }

  var isOnMetered = this.volumeManager_.getDriveConnectionState().type ===
      VolumeManagerCommon.DriveConnectionType.METERED;

  if (isOnMetered) {
    this.metadataModel_.get(this.entries_, ['availableWhenMetered', 'size'])
        .then(function(props) {
          if (areAll(this.entries_, props, 'availableWhenMetered')) {
            callback();
            return;
          }

          var sizeToDownload = 0;
          for (var i = 0; i !== this.entries_.length; i++) {
            if (!props[i].availableWhenMetered)
              sizeToDownload += props[i].size;
          }
          this.ui_.confirmDialog.show(
              loadTimeData.getStringF(
                  this.entries_.length === 1 ?
                      'CONFIRM_MOBILE_DATA_USE' :
                      'CONFIRM_MOBILE_DATA_USE_PLURAL',
                  util.bytesToString(sizeToDownload)),
              callback, null, null);
        }.bind(this));
    return;
  }

  callback();
};

/**
 * Executes an internal task.
 *
 * @param {string} taskId The task id.
 * @private
 */
FileTasks.prototype.executeInternalTask_ = function(taskId) {
  var taskParts = taskId.split('|');
  if (taskParts[2] === 'mount-archive') {
    this.mountArchivesInternal_();
    return;
  }
  if (taskParts[2] === 'install-linux-package') {
    this.installLinuxPackageInternal_();
    return;
  }

  console.error('The specified task is not a valid internal task: ' + taskId);
};

/**
 * Install a Linux Package in the Linux container.
 * @private
 */
FileTasks.prototype.installLinuxPackageInternal_ = function() {
  assert(this.entries_.length === 1);
  this.ui_.installLinuxPackageDialog.showInstallLinuxPackageDialog(
      this.entries_[0]);
};

/**
 * The core implementation of mounts archives.
 * @private
 */
FileTasks.prototype.mountArchivesInternal_ = function() {
  var tracker = this.directoryModel_.createDirectoryChangeTracker();
  tracker.start();

  // TODO(mtomasz): Move conversion from entry to url to custom bindings.
  // crbug.com/345527.
  var urls = util.entriesToURLs(this.entries_);
  for (var index = 0; index < urls.length; ++index) {
    // TODO(mtomasz): Pass Entry instead of URL.
    this.volumeManager_.mountArchive(
        urls[index],
        function(volumeInfo) {
          if (tracker.hasChanged) {
            tracker.stop();
            return;
          }
          volumeInfo.resolveDisplayRoot(
              function(displayRoot) {
                if (tracker.hasChanged) {
                  tracker.stop();
                  return;
                }
                this.directoryModel_.changeDirectoryEntry(displayRoot);
              }.bind(this),
              function() {
                console.warn(
                    'Failed to resolve the display root after mounting.');
                tracker.stop();
              });
        }.bind(this),
        function(url, error) {
          tracker.stop();
          var path = util.extractFilePath(url);
          var namePos = path.lastIndexOf('/');
          this.ui_.alertDialog.show(
              strf('ARCHIVE_MOUNT_FAILED', path.substr(namePos + 1), error),
              null,
              null);
        }.bind(this, urls[index]));
  }
};

/**
 * Displays the list of tasks in a open task picker combobutton and a share
 * options menu.
 *
 * @param {!cr.ui.ComboButton} openCombobutton The open task picker combobutton.
 * @param {!cr.ui.MenuButton} shareMenuButton The menu button for share options.
 * @public
 */
FileTasks.prototype.display = function(openCombobutton, shareMenuButton) {
  var openTasks = [];
  var otherTasks = [];
  for (var i = 0; i < this.tasks_.length; i++) {
    var task = this.tasks_[i];
    if (FileTasks.isOpenTask(task))
      openTasks.push(task);
    else
      otherTasks.push(task);
  }
  this.updateOpenComboButton_(openCombobutton, openTasks);
  this.updateShareMenuButton_(shareMenuButton, otherTasks);
};

/**
 * Setup a task picker combobutton based on the given tasks.
 * @param {!cr.ui.ComboButton} combobutton
 * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
 */
FileTasks.prototype.updateOpenComboButton_ = function(combobutton, tasks) {
  combobutton.hidden = tasks.length == 0;
  if (tasks.length == 0)
    return;

  combobutton.clear();

  // If there exist defaultTask show it on the combobutton.
  if (this.defaultTask_) {
    combobutton.defaultItem =
        this.createCombobuttonItem_(this.defaultTask_, str('TASK_OPEN'));
  } else {
    combobutton.defaultItem = {
      type: FileTasks.TaskMenuButtonItemType.ShowMenu,
      label: str('OPEN_WITH_BUTTON_LABEL')
    };
  }

  // If there exist 2 or more available tasks, show them in context menu
  // (including defaultTask). If only one generic task is available, we
  // also show it in the context menu.
  var items = this.createItems_(tasks);
  if (items.length > 1 || (items.length === 1 && this.defaultTask_ === null)) {
    for (var j = 0; j < items.length; j++) {
      combobutton.addDropDownItem(items[j]);
    }

    // If there exist non generic task (i.e. defaultTask is set), we show
    // an item to change default task.
    if (this.defaultTask_) {
      combobutton.addSeparator();
      var changeDefaultMenuItem = combobutton.addDropDownItem({
        type: FileTasks.TaskMenuButtonItemType.ChangeDefaultTask,
        label: loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM')
      });
      changeDefaultMenuItem.classList.add('change-default');
    }
  }
};

/**
 * Setup a menu button for sharing options based on the given tasks.
 * @param {!cr.ui.MenuButton} shareMenuButton
 * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
 */
FileTasks.prototype.updateShareMenuButton_ = function(shareMenuButton, tasks) {
  var driveShareCommand =
      shareMenuButton.menu.querySelector('cr-menu-item[command="#share"]');
  var driveShareCommandSeparator =
      shareMenuButton.menu.querySelector('#drive-share-separator');

  shareMenuButton.hidden = driveShareCommand.disabled && tasks.length == 0;

  // Show the separator if Drive share command is enabled and there is at least
  // one other share actions.
  driveShareCommandSeparator.hidden =
      driveShareCommand.disabled || tasks.length == 0;

  // Clear menu items except for drive share menu and a separator for it.
  // As querySelectorAll() returns live NodeList, we need to copy elements to
  // Array object to modify DOM in the for loop.
  var itemsToRemove = [].slice.call(shareMenuButton.menu.querySelectorAll(
      'cr-menu-item:not([command="#share"])'));
  for (var i = 0; i < itemsToRemove.length; i++) {
    var item = itemsToRemove[i];
    item.parentNode.removeChild(item);
  }

  // Add menu items for the new tasks.
  var items = this.createItems_(tasks);
  for (var i = 0; i < items.length; i++) {
    var menuitem = shareMenuButton.menu.addMenuItem(items[i]);
    cr.ui.decorate(menuitem, cr.ui.FilesMenuItem);
    menuitem.data = items[i];
    if (items[i].iconType) {
      menuitem.style.backgroundImage = '';
      menuitem.setAttribute('file-type-icon', items[i].iconType);
    }
  }
};

/**
 * Creates sorted array of available task descriptions such as title and icon.
 *
 * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks Tasks to create
 *     items.
 * @return {!Array<!FileTasks.ComboButtonItem>} Created array can be used to
 *     feed combobox, menus and so on.
 * @private
 */
FileTasks.prototype.createItems_ = function(tasks) {
  var items = [];

  // Create items.
  for (var index = 0; index < tasks.length; index++) {
    var task = tasks[index];
    if (task === this.defaultTask_) {
      var title = task.title + ' ' +
                  loadTimeData.getString('DEFAULT_TASK_LABEL');
      items.push(this.createCombobuttonItem_(task, title, true, true));
    } else {
      items.push(this.createCombobuttonItem_(task));
    }
  }

  // Sort items (Sort order: isDefault, lastExecutedTime, label).
  items.sort(function(a, b) {
    // Sort by isDefaultTask.
    var isDefault = (b.isDefault ? 1 : 0) - (a.isDefault ? 1 : 0);
    if (isDefault !== 0)
      return isDefault;

    // Sort by last-executed time.
    var aTime = this.taskHistory_.getLastExecutedTime(a.task.taskId);
    var bTime = this.taskHistory_.getLastExecutedTime(b.task.taskId);
    if (aTime != bTime)
      return bTime - aTime;

    // Sort by label.
    return a.label.localeCompare(b.label);
  }.bind(this));

  return items;
};

/**
 * @typedef {{
 *   type: !FileTasks.TaskMenuButtonItemType,
 *   label: string,
 *   iconUrl: string,
 *   iconType: string,
 *   task: !chrome.fileManagerPrivate.FileTask,
 *   bold: boolean,
 *   isDefault: boolean,
 *   isGenericFileHandler: boolean,
 * }}
 */
FileTasks.ComboButtonItem;

/**
 * Creates combobutton item based on task.
 *
 * @param {!chrome.fileManagerPrivate.FileTask} task Task to convert.
 * @param {string=} opt_title Title.
 * @param {boolean=} opt_bold Make a menu item bold.
 * @param {boolean=} opt_isDefault Mark the item as default item.
 * @return {!FileTasks.ComboButtonItem} Item appendable to combobutton drop-down
 *     list.
 * @private
 */
FileTasks.prototype.createCombobuttonItem_ = function(
    task, opt_title, opt_bold, opt_isDefault) {
  return {
    type: FileTasks.TaskMenuButtonItemType.RunTask,
    label: opt_title || task.label || task.title,
    iconUrl: task.iconUrl,
    iconType: task.iconType || '',
    task: task,
    bold: opt_bold || false,
    isDefault: opt_isDefault || false,
    isGenericFileHandler: task.isGenericFileHandler
  };
};

/**
 * Shows modal task picker dialog with currently available list of tasks.
 *
 * @param {cr.filebrowser.DefaultTaskDialog} taskDialog Task dialog to show and
 *     update.
 * @param {string} title Title to use.
 * @param {string} message Message to use.
 * @param {function(!chrome.fileManagerPrivate.FileTask)} onSuccess Callback to
 *     pass selected task.
 * @param {FileTasks.TaskPickerType} pickerType Task picker type.
 */
FileTasks.prototype.showTaskPicker = function(
    taskDialog, title, message, onSuccess, pickerType) {
  var tasks = pickerType == FileTasks.TaskPickerType.MoreActions ?
      this.getNonOpenTaskItems() :
      this.getOpenTaskItems();
  var items = this.createItems_(tasks);
  if (pickerType == FileTasks.TaskPickerType.ChangeDefault)
    items = items.filter(item => !item.isGenericFileHandler);

  var defaultIdx = 0;
  for (var j = 0; j < items.length; j++) {
    if (this.defaultTask_ && items[j].task.taskId === this.defaultTask_.taskId)
      defaultIdx = j;
  }

  taskDialog.showDefaultTaskDialog(
      title,
      message,
      items, defaultIdx,
      function(item) {
        onSuccess(item.task);
      });
};

/**
 * Gets the default task from tasks. In case there is no such task (i.e. all
 * tasks are generic file handlers), then return null.
 *
 * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks The list of tasks
 *     from where to choose the default task.
 * @param {!TaskHistory} taskHistory
 * @return {?chrome.fileManagerPrivate.FileTask} the default task, or null if
 *     no default task found.
 */
FileTasks.getDefaultTask = function(tasks, taskHistory) {
  // 1. Default app set for MIME or file extension by user, or built-in app.
  for (var i = 0; i < tasks.length; i++) {
    if (tasks[i].isDefault) {
      return tasks[i];
    }
  }
  var nonGenericTasks = tasks.filter(t => !t.isGenericFileHandler);
  // 2. Most recently executed non-generic task.
  var latest = nonGenericTasks[0];
  if (latest && taskHistory.getLastExecutedTime(latest.taskId)) {
    return latest;
  }
  // 3. Sole non-generic handler.
  if (nonGenericTasks.length == 1) {
    return nonGenericTasks[0];
  }
  return null;
};
