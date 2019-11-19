// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Represents a collection of available tasks to execute for a specific list
 * of entries.
 */
class FileTasks {
  /**
   * @param {!VolumeManager} volumeManager
   * @param {!MetadataModel} metadataModel
   * @param {!DirectoryModel} directoryModel
   * @param {!FileManagerUI} ui
   * @param {!Array<!Entry>} entries
   * @param {!Array<?string>} mimeTypes
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
   * @param {chrome.fileManagerPrivate.FileTask} defaultTask
   * @param {!TaskHistory} taskHistory
   * @param {!NamingController} namingController
   * @param {!Crostini} crostini
   */
  constructor(
      volumeManager, metadataModel, directoryModel, ui, entries, mimeTypes,
      tasks, defaultTask, taskHistory, namingController, crostini) {
    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;

    /** @private @const {!MetadataModel} */
    this.metadataModel_ = metadataModel;

    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!FileManagerUI} */
    this.ui_ = ui;

    /** @private @const {!Array<!Entry>} */
    this.entries_ = entries;

    /** @private @const {!Array<?string>} */
    this.mimeTypes_ = mimeTypes;

    /** @private @const {!Array<!chrome.fileManagerPrivate.FileTask>} */
    this.tasks_ = tasks;

    /** @private @const {chrome.fileManagerPrivate.FileTask} */
    this.defaultTask_ = defaultTask;

    /** @private @const {!TaskHistory} */
    this.taskHistory_ = taskHistory;

    /** @private @const {!NamingController} */
    this.namingController_ = namingController;

    /** @private @const {!Crostini} */
    this.crostini_ = crostini;
  }

  /**
   * @return {!Array<!Entry>}
   */
  get entries() {
    return this.entries_;
  }

  /**
   * Creates an instance of FileTasks for the specified list of entries with
   * mime types.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!MetadataModel} metadataModel
   * @param {!DirectoryModel} directoryModel
   * @param {!FileManagerUI} ui
   * @param {!Array<!Entry>} entries
   * @param {!Array<?string>} mimeTypes
   * @param {!TaskHistory} taskHistory
   * @param {!NamingController} namingController
   * @param {!Crostini} crostini
   * @return {!Promise<!FileTasks>}
   */
  static create(
      volumeManager, metadataModel, directoryModel, ui, entries, mimeTypes,
      taskHistory, namingController, crostini) {
    const tasksPromise = new Promise(fulfill => {
      // getFileTasks supports only native entries.
      entries = entries.filter(util.isNativeEntry);
      if (entries.length === 0) {
        fulfill([]);
        return;
      }
      chrome.fileManagerPrivate.getFileTasks(entries, taskItems => {
        if (chrome.runtime.lastError) {
          console.error(
              'Failed to fetch file tasks due to: ' +
              chrome.runtime.lastError.message);
          Promise.reject();
          return;
        }

        // Linux package installation is currently only supported for a single
        // file which is inside the Linux container, or in a shareable volume.
        // TODO(timloh): Instead of filtering these out, we probably should
        // show a dialog with an error message, similar to when attempting to
        // run Crostini tasks with non-Crostini entries.
        if (entries.length !== 1 ||
            !(FileTasks.isCrostiniEntry(entries[0], volumeManager) ||
              crostini.canSharePath(
                  constants.DEFAULT_CROSTINI_VM, entries[0],
                  false /* persist */))) {
          taskItems = taskItems.filter(item => {
            const taskParts = item.taskId.split('|');
            const appId = taskParts[0];
            const taskType = taskParts[1];
            const actionId = taskParts[2];
            return !(
                appId === chrome.runtime.id && taskType === 'app' &&
                actionId === 'install-linux-package');
          });
        }

        // Filters out Pack with Zip Archiver task because it will be
        // accessible via 'Zip selection' context menu button
        taskItems = taskItems.filter(item => {
          return item.taskId !== FileTasks.ZIP_ARCHIVER_ZIP_TASK_ID &&
              item.taskId !== FileTasks.ZIP_ARCHIVER_ZIP_USING_TMP_TASK_ID;
        });

        fulfill(FileTasks.annotateTasks_(assert(taskItems), entries));
      });
    });

    const defaultTaskPromise = tasksPromise.then(tasks => {
      return FileTasks.getDefaultTask(tasks, taskHistory);
    });

    return Promise.all([tasksPromise, defaultTaskPromise]).then(args => {
      return new FileTasks(
          volumeManager, metadataModel, directoryModel, ui, entries, mimeTypes,
          args[0], args[1], taskHistory, namingController, crostini);
    });
  }

  /**
   * Gets task items.
   * @return {!Array<!chrome.fileManagerPrivate.FileTask>}
   */
  getTaskItems() {
    return this.tasks_;
  }

  /**
   * Gets tasks which are categorized as OPEN tasks.
   * @return {!Array<!chrome.fileManagerPrivate.FileTask>}
   */
  getOpenTaskItems() {
    return this.tasks_.filter(FileTasks.isOpenTask);
  }

  /**
   * Gets tasks which are not categorized as OPEN tasks.
   * @return {!Array<!chrome.fileManagerPrivate.FileTask>}
   */
  getNonOpenTaskItems() {
    return this.tasks_.filter(task => !FileTasks.isOpenTask(task));
  }

  /**
   * Opens the suggest file dialog.
   *
   * @param {function()} onSuccess Success callback.
   * @param {function()} onCancelled User-cancelled callback.
   * @param {function()} onFailure Failure callback.
   */
  openSuggestAppsDialog(onSuccess, onCancelled, onFailure) {
    if (this.entries_.length !== 1) {
      onFailure();
      return;
    }

    const entry = this.entries_[0];
    const mimeType = this.mimeTypes_[0];
    const basename = entry.name;
    const splitted = util.splitExtension(basename);
    const extension = splitted[1];

    // Returns with failure if the file has neither extension nor MIME type.
    if (!extension && !mimeType) {
      onFailure();
      return;
    }

    const onDialogClosed = (result, itemId) => {
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
  }

  /**
   * Returns whether the system is currently offline.
   *
   * @param {!VolumeManager} volumeManager
   * @return {boolean} True if the network status is offline.
   * @private
   */
  static isOffline_(volumeManager) {
    const connection = volumeManager.getDriveConnectionState();
    return connection.type == VolumeManagerCommon.DriveConnectionType.OFFLINE &&
        connection.reason ==
        VolumeManagerCommon.DriveConnectionReason.NO_NETWORK;
  }

  /**
   * Records a metric, as well as recording online and offline versions of it.
   *
   * @param {!VolumeManager} volumeManager
   * @param {string} name Metric name.
   * @param {!*} value Enum value.
   * @param {!Array<*>} values Array of valid values.
   */
  static recordEnumWithOnlineAndOffline_(volumeManager, name, value, values) {
    metrics.recordEnum(name, value, values);
    if (FileTasks.isOffline_(volumeManager)) {
      metrics.recordEnum(name + '.Offline', value, values);
    } else {
      metrics.recordEnum(name + '.Online', value, values);
    }
  }

  /**
   * Records trial of opening file grouped by extensions.
   *
   * @param {!VolumeManager} volumeManager
   * @param {Array<!Entry>} entries The entries to be opened.
   * @private
   */
  static recordViewingFileTypeUMA_(volumeManager, entries) {
    for (let i = 0; i < entries.length; i++) {
      const entry = entries[i];
      let extension = FileType.getExtension(entry).toLowerCase();
      if (FileTasks.UMA_INDEX_KNOWN_EXTENSIONS.indexOf(extension) < 0) {
        extension = 'other';
      }
      FileTasks.recordEnumWithOnlineAndOffline_(
          volumeManager, 'ViewingFileType', extension,
          FileTasks.UMA_INDEX_KNOWN_EXTENSIONS);
    }
  }

  /**
   * Records trial of opening file grouped by root types.
   *
   * @param {!VolumeManager} volumeManager
   * @param {?VolumeManagerCommon.RootType} rootType The type of the root where
   *     entries are being opened.
   * @private
   */
  static recordViewingRootTypeUMA_(volumeManager, rootType) {
    if (rootType !== null) {
      FileTasks.recordEnumWithOnlineAndOffline_(
          volumeManager, 'ViewingRootType', rootType,
          VolumeManagerCommon.RootTypesForUMA);
    }
  }

  static recordZipHandlerUMA_(taskId) {
    if (FileTasks.UMA_ZIP_HANDLER_TASK_IDS_.indexOf(taskId) != -1) {
      metrics.recordEnum(
          'ZipFileTask', taskId, FileTasks.UMA_ZIP_HANDLER_TASK_IDS_);
    }
  }

  /**
   * Records the type of dialog shown when using a crostini app to open a file.
   * @param {!FileTasks.CrostiniShareDialogType} dialogType
   * @private
   */
  static recordCrostiniShareDialogTypeUMA_(dialogType) {
    metrics.recordEnum(
        'CrostiniShareDialog', dialogType,
        FileTasks.UMA_CROSTINI_SHARE_DIALOG_TYPES_);
  }

  /**
   * Returns true if the taskId is for an internal task.
   *
   * @param {string} taskId Task identifier.
   * @return {boolean} True if the task ID is for an internal task.
   * @private
   */
  static isInternalTask_(taskId) {
    const taskParts = taskId.split('|');
    const appId = taskParts[0];
    const taskType = taskParts[1];
    const actionId = taskParts[2];
    if (appId !== chrome.runtime.id || taskType !== 'app') {
      return false;
    }
    switch (actionId) {
      case 'mount-archive':
      case 'install-linux-package':
      case 'import-crostini-image':
        return true;
      default:
        return false;
    }
  }

  /**
   * Returns true if the given task is categorized as an OPEN task.
   *
   * @param {!chrome.fileManagerPrivate.FileTask} task
   * @return {boolean} True if the given task is an OPEN task.
   */
  static isOpenTask(task) {
    // We consider following types of tasks as OPEN tasks.
    // - Files app's internal tasks
    // - file_handler tasks with OPEN_WITH verb
    return !task.verb || task.verb == chrome.fileManagerPrivate.Verb.OPEN_WITH;
  }

  /**
   * Annotates tasks returned from the API.
   *
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks Input tasks from
   *     the API.
   * @param {!Array<!Entry>} entries List of entries for the tasks.
   * @return {!Array<!chrome.fileManagerPrivate.FileTask>} Annotated tasks.
   * @private
   */
  static annotateTasks_(tasks, entries) {
    const result = [];
    const id = chrome.runtime.id;
    for (let i = 0; i < tasks.length; i++) {
      const task = tasks[i];
      const taskParts = task.taskId.split('|');

      // Skip internal Files app's handlers.
      if (taskParts[0] === id &&
          (taskParts[2] === 'select' || taskParts[2] === 'open')) {
        continue;
      }

      // Tweak images, titles of internal tasks.
      if (taskParts[0] === id && taskParts[1] === 'app') {
        if (taskParts[2] === 'mount-archive') {
          task.iconType = 'archive';
          task.title = loadTimeData.getString('MOUNT_ARCHIVE');
          task.verb = undefined;
        } else if (taskParts[2] === 'open-hosted-generic') {
          if (entries.length > 1) {
            task.iconType = 'generic';
          } else {  // Use specific icon.
            task.iconType = FileType.getIcon(entries[0]);
          }
          task.title = loadTimeData.getString('TASK_OPEN');
          task.verb = undefined;
        } else if (taskParts[2] === 'open-hosted-gdoc') {
          task.iconType = 'gdoc';
          task.title = loadTimeData.getString('TASK_OPEN_GDOC');
          task.verb = undefined;
        } else if (taskParts[2] === 'open-hosted-gsheet') {
          task.iconType = 'gsheet';
          task.title = loadTimeData.getString('TASK_OPEN_GSHEET');
          task.verb = undefined;
        } else if (taskParts[2] === 'open-hosted-gslides') {
          task.iconType = 'gslides';
          task.title = loadTimeData.getString('TASK_OPEN_GSLIDES');
          task.verb = undefined;
        } else if (taskParts[2] === 'install-linux-package') {
          task.iconType = 'crostini';
          task.title = loadTimeData.getString('TASK_INSTALL_LINUX_PACKAGE');
          task.verb = undefined;
        } else if (taskParts[2] === 'import-crostini-image') {
          task.iconType = 'tini';
          task.title = loadTimeData.getString('TASK_IMPORT_CROSTINI_IMAGE');
          task.verb = undefined;
        } else if (taskParts[2] === 'view-swf') {
          task.iconType = 'generic';
          task.title = loadTimeData.getString('TASK_VIEW');
          task.verb = undefined;
        } else if (taskParts[2] === 'view-pdf') {
          task.iconType = 'pdf';
          task.title = loadTimeData.getString('TASK_VIEW');
          task.verb = undefined;
        } else if (taskParts[2] === 'view-in-browser') {
          task.iconType = 'generic';
          task.title = loadTimeData.getString('TASK_VIEW');
          task.verb = undefined;
        }
      }
      if (!task.iconType && taskParts[1] === 'web-intent') {
        task.iconType = 'generic';
      }

      // Add verb to title.
      if (task.verb) {
        let verbButtonLabel = '';
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
                  (taskParts[2] == 'send' ||
                   taskParts[2] == 'send_multiple'))) {
              verbButtonLabel = 'SHARE_WITH_VERB_BUTTON_LABEL';
            }
            break;
          case chrome.fileManagerPrivate.Verb.OPEN_WITH:
            verbButtonLabel = 'OPEN_WITH_VERB_BUTTON_LABEL';
            break;
          default:
            console.error('Invalid task verb: ' + task.verb + '.');
        }
        if (verbButtonLabel) {
          task.label = loadTimeData.getStringF(verbButtonLabel, task.title);
        }
      }

      result.push(task);
    }

    return result;
  }

  /**
   * @param {!Entry} entry
   * @param {!VolumeManager} volumeManager
   * @return {boolean} True if the entry is from crostini.
   */
  static isCrostiniEntry(entry, volumeManager) {
    return volumeManager.getLocationInfo(entry).rootType ===
        VolumeManagerCommon.RootType.CROSTINI;
  }

  /**
   * Executes default task.
   *
   * @param {function(boolean, Array<!Entry>)=} opt_callback Called when the
   *     default task is executed, or the error is occurred.
   */
  executeDefault(opt_callback) {
    FileTasks.recordViewingFileTypeUMA_(this.volumeManager_, this.entries_);
    FileTasks.recordViewingRootTypeUMA_(
        this.volumeManager_, this.directoryModel_.getCurrentRootType());
    this.executeDefaultInternal_(opt_callback);
  }

  /**
   * Executes default task.
   *
   * @param {function(boolean, Array<!Entry>)=} opt_callback Called when the
   *     default task is executed, or the error is occurred.
   * @private
   */
  executeDefaultInternal_(opt_callback) {
    const callback = opt_callback || ((arg1, arg2) => {});

    if (this.defaultTask_ !== null) {
      this.executeInternal_(this.defaultTask_);
      callback(true, this.entries_);
      return;
    }

    const nonGenericTasks = this.tasks_.filter(t => !t.isGenericFileHandler);
    // If there is only one task that is not a generic file handler, it should
    // be executed as a default task. If there are multiple tasks that are not
    // generic file handlers, and none of them are considered as default, we
    // show a task picker to ask the user to choose one.
    if (nonGenericTasks.length >= 2) {
      this.showTaskPicker(
          this.ui_.defaultTaskPicker, str('OPEN_WITH_BUTTON_LABEL'),
          '', task => {
            this.execute(task);
          }, FileTasks.TaskPickerType.OpenWith);
      return;
    }

    // We don't have tasks, so try to show a file in a browser tab.
    // We only do that for single selection to avoid confusion.
    if (this.entries_.length !== 1) {
      return;
    }

    const filename = this.entries_[0].name;
    const extension = util.splitExtension(filename)[1] || null;
    const mimeType = this.mimeTypes_[0] || null;

    const showAlert = () => {
      let textMessageId;
      let titleMessageId;
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

      const webStoreUrl = webStoreUtils.createWebStoreLink(extension, mimeType);
      const text =
          strf(textMessageId, webStoreUrl, str('NO_TASK_FOR_FILE_URL'));
      const title = titleMessageId ? str(titleMessageId) : filename;
      this.ui_.alertDialog.showHtml(title, text, null, null, null);
      callback(false, this.entries_);
    };

    const onViewFilesFailure = () => {
      if (extension &&
          (FileTasks.EXTENSIONS_TO_SKIP_SUGGEST_APPS_.indexOf(extension) !==
               -1 ||
           constants.EXECUTABLE_EXTENSIONS.indexOf(assert(extension)) !== -1)) {
        showAlert();
        return;
      }

      this.openSuggestAppsDialog(
          () => {
            FileTasks
                .create(
                    this.volumeManager_, this.metadataModel_,
                    this.directoryModel_, this.ui_, this.entries_,
                    this.mimeTypes_, this.taskHistory_, this.namingController_,
                    this.crostini_)
                .then(
                    tasks => {
                      tasks.executeDefault();
                      callback(true, this.entries_);
                    },
                    () => {
                      callback(false, this.entries_);
                    });
          },
          () => {
            callback(false, this.entries_);
          },
          showAlert);
    };

    const onViewFiles = result => {
      if (chrome.runtime.lastError) {
        // Suppress the Unchecked runtime.lastError console message
        console.debug(chrome.runtime.lastError.message);
        onViewFilesFailure();
        return;
      }
      switch (result) {
        case 'opened':
          callback(true, this.entries_);
          break;
        case 'message_sent':
          util.isTeleported(window).then(teleported => {
            if (teleported) {
              this.ui_.showOpenInOtherDesktopAlert(this.entries_);
            }
          });
          callback(true, this.entries_);
          break;
        case 'empty':
          callback(true, this.entries_);
          break;
        case 'failed':
          onViewFilesFailure();
          break;
      }
    };

    this.checkAvailability_(() => {
      const taskId = chrome.runtime.id + '|file|view-in-browser';
      chrome.fileManagerPrivate.executeTask(taskId, this.entries_, onViewFiles);
    });
  }

  /**
   * Executes a single task.
   *
   * @param {chrome.fileManagerPrivate.FileTask} task FileTask.
   */
  execute(task) {
    FileTasks.recordViewingFileTypeUMA_(this.volumeManager_, this.entries_);
    FileTasks.recordViewingRootTypeUMA_(
        this.volumeManager_, this.directoryModel_.getCurrentRootType());
    this.executeInternal_(task);
  }

  /**
   * The core implementation to execute a single task.
   *
   * @param {chrome.fileManagerPrivate.FileTask} task FileTask.
   * @private
   */
  executeInternal_(task) {
    this.checkAvailability_(() => {
      this.taskHistory_.recordTaskExecuted(task.taskId);
      let msg;
      if (this.entries.length === 1) {
        msg = strf('OPEN_A11Y', this.entries_[0].name);
      } else {
        msg = strf('OPEN_A11Y_PLURAL', this.entries_.length);
      }
      this.ui_.speakA11yMessage(msg);
      if (FileTasks.isInternalTask_(task.taskId)) {
        this.executeInternalTask_(task.taskId);
      } else {
        FileTasks.recordZipHandlerUMA_(task.taskId);
        chrome.fileManagerPrivate.executeTask(
            task.taskId, this.entries_, (result) => {
              if (chrome.runtime.lastError) {
                console.warn(
                    'Unable to execute task: ' +
                    chrome.runtime.lastError.message);
                return;
              }
              if (result !== 'message_sent') {
                return;
              }
              util.isTeleported(window).then((teleported) => {
                if (teleported) {
                  this.ui_.showOpenInOtherDesktopAlert(this.entries_);
                }
              });
            });
      }
    });
  }

  /**
   * Ensures that the all files are available right now.
   *
   * Must not call before initialization.
   * @param {function()} callback Called when checking is completed and all
   *     files are available. Otherwise not called.
   * @private
   */
  checkAvailability_(callback) {
    const areAll = (entries, props, name) => {
      // TODO(cmihail): Make files in directories available offline.
      // See http://crbug.com/569767.
      let okEntriesNum = 0;
      for (let i = 0; i < entries.length; i++) {
        // If got no properties, we safely assume that item is available.
        if (props[i] && (props[i][name] || entries[i].isDirectory)) {
          okEntriesNum++;
        }
      }
      return okEntriesNum === props.length;
    };

    const containsDriveEntries = this.entries_.some(entry => {
      const volumeInfo = this.volumeManager_.getVolumeInfo(entry);
      return volumeInfo &&
          volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE;
    });

    // Availability is not checked for non-Drive files, as availableOffline, nor
    // availableWhenMetered are not exposed for other types of volumes at this
    // moment.
    if (!containsDriveEntries) {
      callback();
      return;
    }

    const isDriveOffline =
        this.volumeManager_.getDriveConnectionState().type ===
        VolumeManagerCommon.DriveConnectionType.OFFLINE;

    if (isDriveOffline) {
      this.metadataModel_.get(this.entries_, ['availableOffline', 'hosted'])
          .then(props => {
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
                        this.entries_.length === 1 ? 'OFFLINE_MESSAGE' :
                                                     'OFFLINE_MESSAGE_PLURAL',
                        loadTimeData.getString('OFFLINE_COLUMN_LABEL')),
                null, null, null);
          });
      return;
    }

    const isOnMetered = this.volumeManager_.getDriveConnectionState().type ===
        VolumeManagerCommon.DriveConnectionType.METERED;

    if (isOnMetered) {
      this.metadataModel_.get(this.entries_, ['availableWhenMetered', 'size'])
          .then(props => {
            if (areAll(this.entries_, props, 'availableWhenMetered')) {
              callback();
              return;
            }

            let sizeToDownload = 0;
            for (let i = 0; i !== this.entries_.length; i++) {
              if (!props[i].availableWhenMetered) {
                sizeToDownload += props[i].size;
              }
            }
            this.ui_.confirmDialog.show(
                loadTimeData.getStringF(
                    this.entries_.length === 1 ?
                        'CONFIRM_MOBILE_DATA_USE' :
                        'CONFIRM_MOBILE_DATA_USE_PLURAL',
                    util.bytesToString(sizeToDownload)),
                callback, null, null);
          });
      return;
    }

    callback();
  }

  /**
   * Executes an internal task.
   *
   * @param {string} taskId The task id.
   * @private
   */
  executeInternalTask_(taskId) {
    const taskParts = taskId.split('|');
    if (taskParts[2] === 'mount-archive') {
      this.mountArchivesInternal_();
      return;
    }
    if (taskParts[2] === 'install-linux-package') {
      this.installLinuxPackageInternal_();
      return;
    }
    if (taskParts[2] === 'import-crostini-image') {
      this.importCrostiniImageInternal_();
      return;
    }

    console.error('The specified task is not a valid internal task: ' + taskId);
  }

  /**
   * Install a Linux Package in the Linux container.
   * @private
   */
  installLinuxPackageInternal_() {
    assert(this.entries_.length === 1);
    this.ui_.installLinuxPackageDialog.showInstallLinuxPackageDialog(
        this.entries_[0]);
  }

  /**
   * Imports a Crostini Image File (.tini). This overrides the existing Linux
   * apps and files.
   * @private
   */
  importCrostiniImageInternal_() {
    assert(this.entries_.length === 1);
    this.ui_.importCrostiniImageDialog.showImportCrostiniImageDialog(
        this.entries_[0]);
  }

  /**
   * The core implementation of mount archives.
   * @private
   */
  async mountArchivesInternal_() {
    const tracker = this.directoryModel_.createDirectoryChangeTracker();
    tracker.start();
    try {
      // TODO(mtomasz): Move conversion from entry to url to custom bindings.
      // crbug.com/345527.
      const urls = util.entriesToURLs(this.entries_);
      const promises = urls.map(async (url) => {
        try {
          const volumeInfo = await this.volumeManager_.mountArchive(url);
          if (tracker.hasChanged) {
            return;
          }

          try {
            const displayRoot = await volumeInfo.resolveDisplayRoot();
            if (tracker.hasChanged) {
              return;
            }

            this.directoryModel_.changeDirectoryEntry(displayRoot);
          } catch (error) {
            console.error('Cannot resolve display root after mounting:', error);
          }
        } catch (error) {
          const path = util.extractFilePath(url);
          const namePos = path.lastIndexOf('/');
          this.ui_.alertDialog.show(
              strf('ARCHIVE_MOUNT_FAILED', path.substr(namePos + 1), error),
              null, null);
          console.error(`Cannot mount '${path}': ${error.stack || error}`);
        }
      });

      await Promise.all(promises);
    } finally {
      tracker.stop();
    }
  }

  /**
   * Displays the list of tasks in a open task picker combobutton and a share
   * options menu.
   *
   * @param {!cr.ui.ComboButton} openCombobutton The open task picker
   *     combobutton.
   * @param {!cr.ui.MultiMenuButton} shareMenuButton Button for share options.
   * @public
   */
  display(openCombobutton, shareMenuButton) {
    const openTasks = [];
    const otherTasks = [];
    for (let i = 0; i < this.tasks_.length; i++) {
      const task = this.tasks_[i];
      if (FileTasks.isOpenTask(task)) {
        openTasks.push(task);
      } else {
        otherTasks.push(task);
      }
    }
    this.updateOpenComboButton_(openCombobutton, openTasks);
    this.updateShareMenuButton_(shareMenuButton, otherTasks);
  }

  /**
   * Setup a task picker combobutton based on the given tasks.
   * @param {!cr.ui.ComboButton} combobutton
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
   */
  updateOpenComboButton_(combobutton, tasks) {
    combobutton.hidden = tasks.length == 0;
    if (tasks.length == 0) {
      return;
    }

    combobutton.clear();

    // If there exist defaultTask show it on the combobutton.
    if (this.defaultTask_) {
      combobutton.defaultItem =
          FileTasks.createComboButtonItem_(this.defaultTask_, str('TASK_OPEN'));
    } else {
      combobutton.defaultItem = {
        type: FileTasks.TaskMenuButtonItemType.ShowMenu,
        label: str('OPEN_WITH_BUTTON_LABEL')
      };
    }

    // If there exist 2 or more available tasks, show them in context menu
    // (including defaultTask). If only one generic task is available, we
    // also show it in the context menu.
    const items = this.createItems_(tasks);
    if (items.length > 1 ||
        (items.length === 1 && this.defaultTask_ === null)) {
      for (let j = 0; j < items.length; j++) {
        combobutton.addDropDownItem(items[j]);
      }

      // If there exist non generic task (i.e. defaultTask is set), we show
      // an item to change default task.
      if (this.defaultTask_) {
        combobutton.addSeparator();
        const changeDefaultMenuItem = combobutton.addDropDownItem({
          type: FileTasks.TaskMenuButtonItemType.ChangeDefaultTask,
          label: loadTimeData.getString('CHANGE_DEFAULT_MENU_ITEM')
        });
        changeDefaultMenuItem.classList.add('change-default');
      }
    }
  }

  /**
   * Setup a menu button for sharing options based on the given tasks.
   * @param {!cr.ui.MultiMenuButton} shareMenuButton
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
   */
  updateShareMenuButton_(shareMenuButton, tasks) {
    let driveShareCommand =
        shareMenuButton.menu.querySelector('cr-menu-item[command="#share"]');
    let driveShareCommandSeparator =
        shareMenuButton.menu.querySelector('#drive-share-separator');
    let moreActionsSeparator =
        shareMenuButton.menu.querySelector('#more-actions-separator');

    // Update share command.
    driveShareCommand.command.canExecuteChange(
        this.ui_.listContainer.currentList);

    // Hide share icon for New Folder creation.  See https://crbug.com/571355.
    shareMenuButton.hidden =
        (driveShareCommand.disabled && tasks.length == 0) ||
        this.namingController_.isRenamingInProgress();
    moreActionsSeparator.hidden = true;

    // Show the separator if Drive share command is enabled and there is at
    // least one other share actions.
    driveShareCommandSeparator.hidden =
        driveShareCommand.disabled || tasks.length == 0;

    // Temporarily remove the more actions item while the rest of the menu
    // items are being cleared out so we don't lose it and make it hidden for
    // now
    let moreActions = shareMenuButton.menu.querySelector(
        'cr-menu-item[command="#show-submenu"]');
    moreActions.remove();
    moreActions.setAttribute('hidden', '');
    // Remove the separator as well
    moreActionsSeparator.remove();

    // Clear menu items except for drive share menu and a separator for it.
    // As querySelectorAll() returns live NodeList, we need to copy elements to
    // Array object to modify DOM in the for loop.
    const itemsToRemove = [].slice.call(shareMenuButton.menu.querySelectorAll(
        'cr-menu-item:not([command="#share"])'));
    for (let i = 0; i < itemsToRemove.length; i++) {
      const item = itemsToRemove[i];
      item.parentNode.removeChild(item);
    }
    // Clear menu items in the overflow sub-menu since we'll repopulate it
    // with any relevant items below.
    if (shareMenuButton.overflow !== null) {
      while (shareMenuButton.overflow.firstChild !== null) {
        shareMenuButton.overflow.removeChild(
            shareMenuButton.overflow.firstChild);
      }
    }

    // Add menu items for the new tasks.
    const items = this.createItems_(tasks);
    let menu = /** @type {!cr.ui.Menu} */ (shareMenuButton.menu);
    for (let i = 0; i < items.length; i++) {
      // If we have at least 10 entries, split off into a sub-menu
      if (i == NUM_TOP_LEVEL_ENTRIES && MAX_NON_SPLIT_ENTRIES <= items.length) {
        moreActions.removeAttribute('hidden');
        moreActionsSeparator.hidden = false;
        menu = shareMenuButton.overflow;
      }
      const menuitem = menu.addMenuItem(items[i]);
      cr.ui.decorate(menuitem, cr.ui.FilesMenuItem);
      menuitem.data = items[i];
      if (items[i].iconType) {
        menuitem.style.backgroundImage = '';
        menuitem.setAttribute('file-type-icon', items[i].iconType);
      }
    }
    // Replace the more actions menu item and separator
    shareMenuButton.menu.appendChild(moreActionsSeparator);
    shareMenuButton.menu.appendChild(moreActions);
  }

  /**
   * Creates sorted array of available task descriptions such as title and icon.
   *
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks Tasks to create
   *     items.
   * @return {!Array<!FileTasks.ComboButtonItem>} Created array can be used to
   *     feed combobox, menus and so on.
   * @private
   */
  createItems_(tasks) {
    const items = [];

    // Create items.
    for (let index = 0; index < tasks.length; index++) {
      const task = tasks[index];
      if (task === this.defaultTask_) {
        const title =
            task.title + ' ' + loadTimeData.getString('DEFAULT_TASK_LABEL');
        items.push(FileTasks.createComboButtonItem_(task, title, true, true));
      } else {
        items.push(FileTasks.createComboButtonItem_(task));
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
      const aTime = this.taskHistory_.getLastExecutedTime(a.task.taskId);
      const bTime = this.taskHistory_.getLastExecutedTime(b.task.taskId);
      if (aTime != bTime) {
        return bTime - aTime;
      }

      // Sort by label.
      return a.label.localeCompare(b.label);
    });

    return items;
  }

  /**
   * Creates combobutton item based on task.
   *
   * @param {!chrome.fileManagerPrivate.FileTask} task Task to convert.
   * @param {string=} opt_title Title.
   * @param {boolean=} opt_bold Make a menu item bold.
   * @param {boolean=} opt_isDefault Mark the item as default item.
   * @return {!FileTasks.ComboButtonItem} Item appendable to combobutton
   *     drop-down list.
   * @private
   */
  static createComboButtonItem_(task, opt_title, opt_bold, opt_isDefault) {
    return {
      type: FileTasks.TaskMenuButtonItemType.RunTask,
      label: opt_title || task.label || task.title,
      iconUrl: task.iconUrl || '',
      iconType: task.iconType || '',
      task: task,
      bold: opt_bold || false,
      isDefault: opt_isDefault || false,
      isGenericFileHandler: /** @type {boolean} */ (task.isGenericFileHandler)
    };
  }

  /**
   * Shows modal task picker dialog with currently available list of tasks.
   *
   * @param {cr.filebrowser.DefaultTaskDialog} taskDialog Task dialog to show
   *     and update.
   * @param {string} title Title to use.
   * @param {string} message Message to use.
   * @param {function(!chrome.fileManagerPrivate.FileTask)} onSuccess Callback
   *     to pass selected task.
   * @param {FileTasks.TaskPickerType} pickerType Task picker type.
   */
  showTaskPicker(taskDialog, title, message, onSuccess, pickerType) {
    const tasks = pickerType == FileTasks.TaskPickerType.MoreActions ?
        this.getNonOpenTaskItems() :
        this.getOpenTaskItems();
    let items = this.createItems_(tasks);
    if (pickerType == FileTasks.TaskPickerType.ChangeDefault) {
      items = items.filter(item => !item.isGenericFileHandler);
    }

    let defaultIdx = 0;
    for (let j = 0; j < items.length; j++) {
      if (this.defaultTask_ &&
          items[j].task.taskId === this.defaultTask_.taskId) {
        defaultIdx = j;
      }
    }

    taskDialog.showDefaultTaskDialog(
        title, message, items, defaultIdx, item => {
          onSuccess(item.task);
        });
  }

  /**
   * Gets the default task from tasks. In case there is no such task (i.e. all
   * tasks are generic file handlers), then return null.
   *
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks The list of
   *     tasks from where to choose the default task.
   * @param {!TaskHistory} taskHistory
   * @return {?chrome.fileManagerPrivate.FileTask} the default task, or null if
   *     no default task found.
   */
  static getDefaultTask(tasks, taskHistory) {
    // 1. Default app set for MIME or file extension by user, or built-in app.
    for (let i = 0; i < tasks.length; i++) {
      if (tasks[i].isDefault) {
        return tasks[i];
      }
    }
    const nonGenericTasks = tasks.filter(t => !t.isGenericFileHandler);
    // 2. Most recently executed non-generic task.
    const latest = nonGenericTasks[0];
    if (latest && taskHistory.getLastExecutedTime(latest.taskId)) {
      return latest;
    }
    // 3. Sole non-generic handler.
    if (nonGenericTasks.length == 1) {
      return nonGenericTasks[0];
    }
    return null;
  }
}

/**
 * The app ID of the video player app.
 * @const {string}
 */
FileTasks.VIDEO_PLAYER_ID = 'jcgeabjmjgoblfofpppfkcoakmfobdko';

/**
 * The task id of the zip unpacker app.
 * @const {string}
 */
FileTasks.ZIP_UNPACKER_TASK_ID = 'oedeeodfidgoollimchfdnbmhcpnklnd|app|zip';

/**
 * The task id of unzip action of Zip Archiver app.
 * @const {string}
 */
FileTasks.ZIP_ARCHIVER_UNZIP_TASK_ID =
    'dmboannefpncccogfdikhmhpmdnddgoe|app|open';

/**
 * The task id of zip action of Zip Archiver app.
 * @const {string}
 */
FileTasks.ZIP_ARCHIVER_ZIP_TASK_ID =
    'dmboannefpncccogfdikhmhpmdnddgoe|app|pack';

/**
 * The task id of zip action of Zip Archiver app, using temporary dir as workdir
 * @const {string}
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
 * List of file extensions to record in UMA.
 *
 * Note: since the data is recorded by list index, new items should be added
 * to the end of this list.
 *
 * The list must also match the FileBrowser ViewFileType entry in enums.xml.
 *
 * @const {Array<string>}
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
  '.gslides',  '.arw',         '.cr2',
  '.dng',      '.nef',         '.nrw',
  '.orf',      '.raf',         '.rw2',
  '.tini'
]);

/**
 * The list of extensions to skip the suggest app dialog.
 * @private @const {Array<string>}
 */
FileTasks.EXTENSIONS_TO_SKIP_SUGGEST_APPS_ = Object.freeze([
  '.crdownload',
  '.dsc',
  '.inf',
  '.crx',
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
 * The number of menu-item entries in the top level menu
 * before we split and show the 'More actions' option
 * @const {number}
 */
const NUM_TOP_LEVEL_ENTRIES = 6;

/**
 * Don't split the menu if the number of entries is smaller
 * than this. e.g. with 7 entries it'd be poor to show a
 * sub-menu with a single entry.
 * @const {number}
 */
const MAX_NON_SPLIT_ENTRIES = 10;

/**
 * @typedef {{
 *   type: !FileTasks.TaskMenuButtonItemType,
 *   label: string,
 *   iconUrl: (string|undefined),
 *   iconType: string,
 *   task: !chrome.fileManagerPrivate.FileTask,
 *   bold: boolean,
 *   isDefault: boolean,
 *   isGenericFileHandler: (boolean|undefined),
 * }}
 */
FileTasks.ComboButtonItem;
