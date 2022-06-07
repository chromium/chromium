// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';

import {AsyncUtil} from '../../common/js/async_util.js';
import {FileType} from '../../common/js/file_type.js';
import {metrics} from '../../common/js/metrics.js';
import {ProgressCenterItem, ProgressItemState, ProgressItemType} from '../../common/js/progress_center_common.js';
import {LEGACY_FILES_EXTENSION_ID, SWA_APP_ID, SWA_FILES_APP_URL} from '../../common/js/url_constants.js';
import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {Crostini} from '../../externs/background/crostini.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {FilesPasswordDialog} from '../elements/files_password_dialog.js';

import {constants} from './constants.js';
import {DirectoryModel} from './directory_model.js';
import {FileTransferController} from './file_transfer_controller.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {NamingController} from './naming_controller.js';
import {TaskHistory} from './task_history.js';
import {ComboButton} from './ui/combobutton.js';
import {DefaultTaskDialog} from './ui/default_task_dialog.js';
import {FileManagerUI} from './ui/file_manager_ui.js';
import {FilesConfirmDialog} from './ui/files_confirm_dialog.js';

/**
 * Office file handlers UMA values (must be consistent with OfficeFileHandler in
 * tools/metrics/histograms/enums.xml).
 * @const @enum {number}
 */
const OfficeFileHandlersHistogramValues = {
  OTHER: 0,
  WEB_DRIVE_OFFICE: 1,
  QUICK_OFFICE: 2,
};

/**
 * Represents a collection of available tasks to execute for a specific list
 * of entries.
 */
export class FileTasks {
  /**
   * @param {!VolumeManager} volumeManager
   * @param {!MetadataModel} metadataModel
   * @param {!DirectoryModel} directoryModel
   * @param {!FileManagerUI} ui
   * @param {?FileTransferController} fileTransferController
   * @param {!Array<!Entry>} entries
   * @param {!Array<?string>} mimeTypes
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
   * @param {?chrome.fileManagerPrivate.FileTask} defaultTask
   * @param {!TaskHistory} taskHistory
   * @param {!NamingController} namingController
   * @param {!Crostini} crostini
   * @param {!ProgressCenter} progressCenter
   */
  constructor(
      volumeManager, metadataModel, directoryModel, ui, fileTransferController,
      entries, mimeTypes, tasks, defaultTask, taskHistory, namingController,
      crostini, progressCenter) {
    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;

    /** @private @const {!MetadataModel} */
    this.metadataModel_ = metadataModel;

    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!FileManagerUI} */
    this.ui_ = ui;

    /** @private @const {?FileTransferController} */
    this.fileTransferController_ = fileTransferController;

    /** @private @const {!Array<!Entry>} */
    this.entries_ = entries;

    /** @private @const {!Array<?string>} */
    this.mimeTypes_ = mimeTypes;

    /** @private @const {!Array<!chrome.fileManagerPrivate.FileTask>} */
    this.tasks_ = tasks;

    /** @private @const {?chrome.fileManagerPrivate.FileTask} */
    this.defaultTask_ = defaultTask;

    /** @private @const {!TaskHistory} */
    this.taskHistory_ = taskHistory;

    /** @private @const {!NamingController} */
    this.namingController_ = namingController;

    /** @private @const {!Crostini} */
    this.crostini_ = crostini;

    /** @private @const {!ProgressCenter} */
    this.progressCenter_ = progressCenter;

    /**
     * Mutex used to serialize password dialogs.
     * @private @const {!AsyncUtil.Queue}
     */
    this.mutex_ = new AsyncUtil.Queue();
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
   * @param {?FileTransferController} fileTransferController
   * @param {!Array<!Entry>} entries
   * @param {!Array<?string>} mimeTypes
   * @param {!TaskHistory} taskHistory
   * @param {!NamingController} namingController
   * @param {!Crostini} crostini
   * @param {!ProgressCenter} progressCenter
   * @return {!Promise<!FileTasks>}
   */
  static async create(
      volumeManager, metadataModel, directoryModel, ui, fileTransferController,
      entries, mimeTypes, taskHistory, namingController, crostini,
      progressCenter) {
    /** @type {!Array<!chrome.fileManagerPrivate.FileTask>} */
    let tasks = [];

    // Cannot use fake entries with getFileTasks.
    entries = entries.filter(e => !util.isFakeEntry(e));
    if (entries.length !== 0) {
      tasks = await new Promise(
          fulfill => chrome.fileManagerPrivate.getFileTasks(entries, fulfill));
      if (!tasks) {
        throw new Error(
            'Cannot get file tasks: ' + chrome.runtime.lastError.message);
      }
    }

    // Linux package installation is currently only supported for a single file
    // which is inside the Linux container, or in a shareable volume.
    // TODO(timloh): Instead of filtering these out, we probably should show a
    // dialog with an error message, similar to when attempting to run Crostini
    // tasks with non-Crostini entries.
    if (entries.length !== 1 ||
        !(FileTasks.isCrostiniEntry(entries[0], volumeManager) ||
          crostini.canSharePath(
              constants.DEFAULT_CROSTINI_VM, entries[0],
              false /* persist */))) {
      tasks = tasks.filter(
          task => !util.descriptorEqual(
              task.descriptor,
              FileTasks.INSTALL_LINUX_PACKAGE_TASK_DESCRIPTOR));
    }

    tasks = FileTasks.annotateTasks_(tasks, entries);

    const defaultTask = FileTasks.getDefaultTask(tasks, taskHistory);

    return new FileTasks(
        volumeManager, metadataModel, directoryModel, ui,
        fileTransferController, entries, mimeTypes, tasks, defaultTask,
        taskHistory, namingController, crostini, progressCenter);
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
   * Returns whether the system is currently offline.
   *
   * @param {!VolumeManager} volumeManager
   * @return {boolean} True if the network status is offline.
   * @private
   */
  static isOffline_(volumeManager) {
    const connection = volumeManager.getDriveConnectionState();
    return connection.type ==
        chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE &&
        connection.reason ==
        chrome.fileManagerPrivate.DriveOfflineReason.NO_NETWORK;
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
   * Returns ViewFileType enum or 'other' for the given entry.
   * @param {!Entry} entry The entry for which ViewFileType is computed.
   * @return {string} A ViewFileType enum or 'other'.
   */
  static getViewFileType(entry) {
    let extension = FileType.getExtension(entry).toLowerCase();
    if (FileTasks.UMA_INDEX_KNOWN_EXTENSIONS.indexOf(extension) < 0) {
      extension = 'other';
    }
    return extension;
  }

  /**
   * Records trial of opening file grouped by extensions.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Array<!Entry>} entries The entries to be opened.
   * @private
   */
  static recordViewingFileTypeUMA_(volumeManager, entries) {
    for (const entry of entries) {
      FileTasks.recordEnumWithOnlineAndOffline_(
          volumeManager, 'ViewingFileType', FileTasks.getViewFileType(entry),
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

  /**
   * Records the elapsed time for mounting a ZIP file as a ZipMountTime
   * histogram value.
   *
   * @param {?VolumeManagerCommon.RootType} rootType The type of the root where
   *     the ZIP file has been mounted from.
   * @param {number} time Time to be recorded in milliseconds.
   */
  static recordZipMountTimeUMA_(rootType, time) {
    let root;
    switch (rootType) {
      case VolumeManagerCommon.RootType.MY_FILES:
      case VolumeManagerCommon.RootType.DOWNLOADS:
        root = 'MyFiles';
        break;
      case VolumeManagerCommon.RootType.DRIVE:
        root = 'Drive';
        break;
      default:
        root = 'Other';
    }
    metrics.recordTime(`ZipMountTime.${root}`, time);
  }

  /**
   * Records trial of opening Office file grouped by file handlers.
   *
   * @param {!VolumeManager} volumeManager
   * @param {!Array<!Entry>} entries The entries to be opened.
   * @param {?VolumeManagerCommon.RootType} rootType The type of the root where
   *     entries are being opened.
   * @param {?chrome.fileManagerPrivate.FileTask} task FileTask.
   * @private
   */
  static recordOfficeFileHandlerUMA_(volumeManager, entries, rootType, task) {
    if (!task) {
      return;
    }

    // This UMA is only applicable to Office files.
    if (!entries.every(entry => hasOfficeExtension(entry))) {
      return;
    }

    let histogramName = 'OfficeFiles.FileHandler';
    switch (rootType) {
      case VolumeManagerCommon.RootType.DRIVE:
        histogramName += '.Drive';
        break;
      default:
        histogramName += '.NotDrive';
    }

    if (FileTasks.isOffline_(volumeManager)) {
      histogramName += '.Offline';
    } else {
      histogramName += '.Online';
    }

    let fileHandler = OfficeFileHandlersHistogramValues.OTHER;
    switch (parseActionId(task.descriptor.actionId)) {
      case 'open-web-drive-office-word':
      case 'open-web-drive-office-excel':
      case 'open-web-drive-office-powerpoint':
        fileHandler = OfficeFileHandlersHistogramValues.WEB_DRIVE_OFFICE;
        break;
      case 'qo_documents':
        fileHandler = OfficeFileHandlersHistogramValues.QUICK_OFFICE;
        break;
    }

    metrics.recordEnum(
        histogramName, fileHandler,
        Object.keys(OfficeFileHandlersHistogramValues).length);
  }

  /**
   * Returns true if the descriptor is for an internal task.
   *
   * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor
   * @return {boolean} True if the task descriptor is for an internal task.
   * @private
   */
  static isInternalTask_(descriptor) {
    const {appId, taskType, actionId} = descriptor;
    if (!isFilesAppId(appId)) {
      return false;
    }

    // Legacy Files app task type is 'app', Files SWA is 'web'.
    if (!(taskType === 'app' || taskType == 'web')) {
      return false;
    }
    const parsedActionId = parseActionId(actionId);

    switch (parsedActionId) {
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
    for (const task of tasks) {
      const {appId, taskType, actionId} = task.descriptor;
      const parsedActionId = parseActionId(actionId);

      // Skip internal Files app's handlers.
      if (isFilesAppId(appId) &&
          (parsedActionId === 'select' || parsedActionId === 'open')) {
        continue;
      }

      // Tweak images, titles of internal tasks.
      if (isFilesAppId(appId) && (taskType === 'app' || taskType === 'web')) {
        if (parsedActionId === 'mount-archive') {
          task.iconType = 'archive';
          task.title = loadTimeData.getString('MOUNT_ARCHIVE');
          task.verb = undefined;
        } else if (parsedActionId === 'open-hosted-generic') {
          if (entries.length > 1) {
            task.iconType = 'generic';
          } else {  // Use specific icon.
            task.iconType = FileType.getIcon(entries[0]);
          }
          task.title = loadTimeData.getString('TASK_OPEN');
          task.verb = undefined;
        } else if (parsedActionId === 'open-hosted-gdoc') {
          task.iconType = 'gdoc';
          task.title = loadTimeData.getString('TASK_OPEN_GDOC');
          task.verb = chrome.fileManagerPrivate.Verb.OPEN_WITH;
        } else if (parsedActionId === 'open-hosted-gsheet') {
          task.iconType = 'gsheet';
          task.title = loadTimeData.getString('TASK_OPEN_GSHEET');
          task.verb = chrome.fileManagerPrivate.Verb.OPEN_WITH;
        } else if (parsedActionId === 'open-hosted-gslides') {
          task.iconType = 'gslides';
          task.title = loadTimeData.getString('TASK_OPEN_GSLIDES');
          task.verb = chrome.fileManagerPrivate.Verb.OPEN_WITH;
        } else if (parsedActionId === 'open-web-drive-office-word') {
          task.iconType = 'gdoc';
          task.title = loadTimeData.getString('TASK_OPEN_GDOC');
          task.verb = chrome.fileManagerPrivate.Verb.OPEN_WITH;
        } else if (parsedActionId === 'open-web-drive-office-excel') {
          task.iconType = 'gsheet';
          task.title = loadTimeData.getString('TASK_OPEN_GSHEET');
          task.verb = chrome.fileManagerPrivate.Verb.OPEN_WITH;
        } else if (parsedActionId === 'open-web-drive-office-powerpoint') {
          task.iconType = 'gslides';
          task.title = loadTimeData.getString('TASK_OPEN_GSLIDES');
          task.verb = chrome.fileManagerPrivate.Verb.OPEN_WITH;
        } else if (parsedActionId === 'install-linux-package') {
          task.iconType = 'crostini';
          task.title = loadTimeData.getString('TASK_INSTALL_LINUX_PACKAGE');
          task.verb = undefined;
        } else if (parsedActionId === 'import-crostini-image') {
          task.iconType = 'tini';
          task.title = loadTimeData.getString('TASK_IMPORT_CROSTINI_IMAGE');
          task.verb = undefined;
        } else if (parsedActionId === 'view-pdf') {
          task.iconType = 'pdf';
          task.title = loadTimeData.getString('TASK_VIEW');
          task.verb = undefined;
        } else if (parsedActionId === 'view-in-browser') {
          task.iconType = 'generic';
          task.title = loadTimeData.getString('TASK_VIEW');
          task.verb = undefined;
        }
      }
      if (!task.iconType && taskType === 'web-intent') {
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
            if (!(taskType == 'arc' &&
                  (parsedActionId == 'send' ||
                   parsedActionId == 'send_multiple'))) {
              verbButtonLabel = 'SHARE_WITH_VERB_BUTTON_LABEL';
            }
            break;
          case chrome.fileManagerPrivate.Verb.OPEN_WITH:
            verbButtonLabel = 'OPEN_WITH_VERB_BUTTON_LABEL';
            break;
          default:
            console.error('Invalid task verb: ' + task.verb);
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
    const location = volumeManager.getLocationInfo(entry);
    return !!location &&
        location.rootType === VolumeManagerCommon.RootType.CROSTINI;
  }

  /**
   * @param {!Entry} entry
   * @param {!VolumeManager} volumeManager
   * @return {boolean} True if the entry is from MyFiles.
   */
  static isMyFilesEntry(entry, volumeManager) {
    const location = volumeManager.getLocationInfo(entry);
    return !!location &&
        location.rootType === VolumeManagerCommon.RootType.DOWNLOADS;
  }

  /**
   * Show dialog when user opens or drags a file with PluginVM and the file
   * is not in PvmSharedDir or shared with PluginVM. The dialog tells the
   * user to move or copy the file to PvmSharedDir and offers an action to do
   * that.
   *
   * @param {!Array<!Entry>} entries Selected entries to be moved or copied.
   * @param {!VolumeManager} volumeManager
   * @param {!MetadataModel} metadataModel
   * @param {!FileManagerUI} ui FileManager UI to show dialog.
   * @param {string} moveMessage Message if files are local and can be moved.
   * @param {string} copyMessage Message if files should be copied.
   * @param {?FileTransferController} fileTransferController
   * @param {!DirectoryModel} directoryModel
   */
  static showPluginVmNotSharedDialog(
      entries, volumeManager, metadataModel, ui, moveMessage, copyMessage,
      fileTransferController, directoryModel) {
    assert(entries.length > 0);
    const isMyFiles = FileTasks.isMyFilesEntry(entries[0], volumeManager);
    const dialog = new FilesConfirmDialog(ui.element);
    dialog.setOkLabel(strf(
        isMyFiles ? 'CONFIRM_MOVE_BUTTON_LABEL' : 'CONFIRM_COPY_BUTTON_LABEL'));
    dialog.show(isMyFiles ? moveMessage : copyMessage, async () => {
      if (!fileTransferController) {
        console.warn('FileTransferController not set');
        return;
      }

      const pvmDir = await FileTasks.getPvmSharedDir_(volumeManager);

      assert(volumeManager.getLocationInfo(pvmDir));

      fileTransferController.executePaste(new FileTransferController.PastePlan(
          entries.map(e => e.toURL()), [], pvmDir, metadataModel,
          /*isMove=*/ isMyFiles));
      directoryModel.changeDirectoryEntry(pvmDir);
    });
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
    FileTasks.recordOfficeFileHandlerUMA_(
        this.volumeManager_, this.entries_,
        this.directoryModel_.getCurrentRootType(), this.defaultTask_);
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

      const text = strf(textMessageId, str('NO_TASK_FOR_FILE_URL'));
      const title = titleMessageId ? str(titleMessageId) : filename;
      this.ui_.alertDialog.showHtml(title, text, null, null, null);
      callback(false, this.entries_);
    };

    const onViewFilesFailure = () => {
      showAlert();
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
      const descriptor = {
        appId: LEGACY_FILES_EXTENSION_ID,
        taskType: 'file',
        actionId: 'view-in-browser'
      };
      chrome.fileManagerPrivate.executeTask(
          descriptor, this.entries_, onViewFiles);
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
    FileTasks.recordOfficeFileHandlerUMA_(
        this.volumeManager_, this.entries_,
        this.directoryModel_.getCurrentRootType(), task);
    this.executeInternal_(task);
  }

  /**
   * The core implementation to execute a single task.
   *
   * @param {chrome.fileManagerPrivate.FileTask} task FileTask.
   * @private
   */
  executeInternal_(task) {
    const onFileManagerPrivateExecuteTask = result => {
      if (chrome.runtime.lastError) {
        console.warn(
            'Unable to execute task: ' + chrome.runtime.lastError.message);
        return;
      }
      const taskResult = chrome.fileManagerPrivate.TaskResult;
      switch (result) {
        case taskResult.MESSAGE_SENT:
          util.isTeleported(window).then((teleported) => {
            if (teleported) {
              this.ui_.showOpenInOtherDesktopAlert(this.entries_);
            }
          });
          break;
        case taskResult.FAILED_PLUGIN_VM_DIRECTORY_NOT_SHARED:
          const moveMessage = strf(
              'UNABLE_TO_OPEN_WITH_PLUGIN_VM_DIRECTORY_NOT_SHARED_MESSAGE',
              task.title);
          const copyMessage = strf(
              'UNABLE_TO_OPEN_WITH_PLUGIN_VM_EXTERNAL_DRIVE_MESSAGE',
              task.title);
          FileTasks.showPluginVmNotSharedDialog(
              this.entries_, this.volumeManager_, this.metadataModel_, this.ui_,
              moveMessage, copyMessage, this.fileTransferController_,
              this.directoryModel_);
          break;
      }
    };

    this.checkAvailability_(() => {
      this.taskHistory_.recordTaskExecuted(task.descriptor);
      let msg;
      if (this.entries_.length === 1) {
        msg = strf('OPEN_A11Y', this.entries_[0].name);
      } else {
        msg = strf('OPEN_A11Y_PLURAL', this.entries_.length);
      }
      this.ui_.speakA11yMessage(msg);
      if (FileTasks.isInternalTask_(task.descriptor)) {
        this.executeInternalTask_(task.descriptor);
      } else {
        chrome.fileManagerPrivate.executeTask(
            task.descriptor, this.entries_, onFileManagerPrivateExecuteTask);
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
        chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE;

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
        chrome.fileManagerPrivate.DriveConnectionStateType.METERED;

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
   * Executes an internal task, which is a task Files app handles internally
   * without calling into fileManagerPrivate to execute it.
   *
   * @param {!chrome.fileManagerPrivate.FileTaskDescriptor} descriptor
   * @private
   */
  executeInternalTask_(descriptor) {
    const parsedActionId = parseActionId(descriptor.actionId);
    if (parsedActionId === 'mount-archive') {
      this.mountArchives_();
      return;
    }
    if (parsedActionId === 'install-linux-package') {
      this.installLinuxPackageInternal_();
      return;
    }
    if (parsedActionId === 'import-crostini-image') {
      this.importCrostiniImageInternal_();
      return;
    }

    console.error(
        'The specified task is not a valid internal task: ' +
        util.makeTaskID(descriptor));
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
   * Mounts an archive file. Asks for password and retries if necessary.
   * @param {string} url URL of the archive file to moumt.
   * @return {!Promise<!VolumeInfo>}
   * @private
   */
  async mountArchive_(url) {
    const filename = util.extractFilePath(url).split('/').pop();

    const item = new ProgressCenterItem();
    item.id = 'Mounting: ' + url;
    item.type = ProgressItemType.MOUNT_ARCHIVE;
    item.message = strf('ARCHIVE_MOUNT_MESSAGE', filename);

    item.cancelCallback = async () => {
      // Remove progress panel.
      item.state = ProgressItemState.CANCELED;
      this.progressCenter_.updateItem(item);

      // Cancel archive mounting.
      try {
        await this.volumeManager_.cancelMounting(url);
      } catch (error) {
        console.warn('Cannot cancel archive (redacted):', error);
        console.log(`Cannot cancel archive '${url}':`, error);
      }
    };

    // Display progress panel.
    item.state = ProgressItemState.PROGRESSING;
    this.progressCenter_.updateItem(item);

    // First time, try without providing a password.
    try {
      return await this.volumeManager_.mountArchive(url);
    } catch (error) {
      // If error is not about needing a password, propagate it.
      if (error !== VolumeManagerCommon.VolumeError.NEED_PASSWORD) {
        throw error;
      }
    } finally {
      // Remove progress panel.
      item.state = ProgressItemState.COMPLETED;
      this.progressCenter_.updateItem(item);
    }

    // We need a password.
    const unlock = await this.mutex_.lock();
    try {
      /** @type {?string} */ let password = null;
      while (true) {
        // Ask for password.
        do {
          password =
              await this.ui_.passwordDialog.askForPassword(filename, password);
        } while (!password);

        // Display progress panel.
        item.state = ProgressItemState.PROGRESSING;
        this.progressCenter_.updateItem(item);

        // Mount archive with password.
        try {
          return await this.volumeManager_.mountArchive(url, password);
        } catch (error) {
          // If error is not about needing a password, propagate it.
          if (error !== VolumeManagerCommon.VolumeError.NEED_PASSWORD) {
            throw error;
          }
        } finally {
          // Remove progress panel.
          item.state = ProgressItemState.COMPLETED;
          this.progressCenter_.updateItem(item);
        }
      }
    } finally {
      unlock();
    }
  }

  /**
   * Mounts an archive file and changes directory. Asks for password if
   * necessary. Displays error message if necessary.
   * @param {Object} tracker
   * @param {string} url URL of the archive file to moumt.
   * @return {!Promise<void>} a promise that is never rejected.
   * @private
   */
  async mountArchiveAndChangeDirectory_(tracker, url) {
    try {
      const startTime = Date.now();
      const volumeInfo = await this.mountArchive_(url);
      // On mountArchive_ success, record mount time UMA.
      FileTasks.recordZipMountTimeUMA_(
          this.directoryModel_.getCurrentRootType(), Date.now() - startTime);

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
      // No need to display an error message if user canceled mounting or
      // canceled the password prompt.
      if (error === FilesPasswordDialog.USER_CANCELLED ||
          error === VolumeManagerCommon.VolumeError.CANCELLED) {
        return;
      }

      const filename = util.extractFilePath(url).split('/').pop();
      const item = new ProgressCenterItem();
      item.id = 'Cannot mount: ' + url;
      item.type = ProgressItemType.MOUNT_ARCHIVE;
      const msgId = error === VolumeManagerCommon.VolumeError.INVALID_PATH ?
          'ARCHIVE_MOUNT_INVALID_PATH' :
          'ARCHIVE_MOUNT_FAILED';
      item.message = strf(msgId, filename);
      item.state = ProgressItemState.ERROR;
      this.progressCenter_.updateItem(item);

      console.warn('Cannot mount (redacted):', error);
      console.debug(`Cannot mount '${url}':`, error);
    }
  }

  /**
   * Mounts the selected archive(s). Asks for password if necessary.
   * @private
   */
  async mountArchives_() {
    const tracker = this.directoryModel_.createDirectoryChangeTracker();
    tracker.start();
    try {
      this.entries_.forEach(entry => entry.getMetadata(metadata => {
        const extension = entry.name.split('.').pop().toLowerCase();
        metrics.recordSmallCount(
            `ArchiveSize.${extension}`,
            Math.ceil(metadata.size / 104857600));  // Each unit = 100MiB
      }));
      // TODO(mtomasz): Move conversion from entry to url to custom bindings.
      // crbug.com/345527.
      const urls = util.entriesToURLs(this.entries_);
      const promises =
          urls.map(url => this.mountArchiveAndChangeDirectory_(tracker, url));
      await Promise.all(promises);
    } finally {
      tracker.stop();
    }
  }

  /**
   * Displays the list of tasks in a open task picker combobutton..
   *
   * @param {!ComboButton} openCombobutton The open task picker
   *     combobutton.
   * @public
   */
  display(openCombobutton) {
    const openTasks = [];
    for (const task of this.tasks_) {
      if (FileTasks.isOpenTask(task)) {
        openTasks.push(task);
      }
    }
    this.updateOpenComboButton_(openCombobutton, openTasks);
  }

  /**
   * Setup a task picker combobutton based on the given tasks. The combobutton
   * is not shown if there are no tasks, or if any entry is a directory.
   *
   * @param {!ComboButton} combobutton
   * @param {!Array<!chrome.fileManagerPrivate.FileTask>} tasks
   */
  updateOpenComboButton_(combobutton, tasks) {
    combobutton.hidden =
        tasks.length == 0 || this.entries_.some(e => e.isDirectory);
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
      for (const item of items) {
        combobutton.addDropDownItem(item);
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
    for (const task of tasks) {
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
      const aTime = this.taskHistory_.getLastExecutedTime(a.task.descriptor);
      const bTime = this.taskHistory_.getLastExecutedTime(b.task.descriptor);
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
   * @param {DefaultTaskDialog} taskDialog Task dialog to show
   *     and update.
   * @param {string} title Title to use.
   * @param {string} message Message to use.
   * @param {function(!chrome.fileManagerPrivate.FileTask)} onSuccess Callback
   *     to pass selected task.
   * @param {FileTasks.TaskPickerType} pickerType Task picker type.
   */
  showTaskPicker(taskDialog, title, message, onSuccess, pickerType) {
    let items = this.createItems_(this.getOpenTaskItems());
    if (pickerType == FileTasks.TaskPickerType.ChangeDefault) {
      items = items.filter(item => !item.isGenericFileHandler);
    }

    let defaultIdx = 0;
    if (this.defaultTask_) {
      for (let j = 0; j < items.length; j++) {
        if (util.descriptorEqual(
                items[j].task.descriptor, this.defaultTask_.descriptor)) {
          defaultIdx = j;
        }
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
    for (const task of tasks) {
      if (task.isDefault) {
        return task;
      }
    }

    const nonGenericTasks = tasks.filter(t => !t.isGenericFileHandler);
    if (nonGenericTasks.length === 0) {
      return null;
    }

    // 2. Most recently executed or sole non-generic task.
    const latest = nonGenericTasks[0];
    if (nonGenericTasks.length == 1 ||
        taskHistory.getLastExecutedTime(latest.descriptor)) {
      return latest;
    }

    return null;
  }

  /**
   * @param {!VolumeManager} volumeManager
   */
  static async getPvmSharedDir_(volumeManager) {
    return new Promise((resolve, reject) => {
      volumeManager
          .getCurrentProfileVolumeInfo(VolumeManagerCommon.VolumeType.DOWNLOADS)
          .fileSystem.root.getDirectory(
              'PvmDefault', {create: false},
              (dir) => {
                resolve(dir);
              },
              (...args) => {
                reject(new Error(`Error getting PvmDefault dir: ${args}`));
              });
    });
  }
}

/**
 * The task descriptor of 'Install Linux package'.
 * @const {!chrome.fileManagerPrivate.FileTaskDescriptor}
 */
FileTasks.INSTALL_LINUX_PACKAGE_TASK_DESCRIPTOR = {
  appId: LEGACY_FILES_EXTENSION_ID,
  taskType: 'app',
  actionId: 'install-linux-package'
};

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
};

/**
 * List of file extensions to record in UMA.
 *
 * Note: since the data is recorded by list index, new items should be added
 * to the end of this list.
 *
 * The list must also match the FileBrowser ViewFileType entry in enums.xml.
 *
 * @const {!Array<string>}
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
 * List of Office file extensions
 * @const {Set<string>}
 */
const OFFICE_EXTENSIONS =
    new Set(['.doc', '.docx', '.xls', '.xlsx', '.ppt', '.pptx']);

/**
 * The number of menu-item entries in the top level menu.
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

/**
 * @param {string} appId
 * @return {boolean} Whether the appId belongs to Files app (legacy or SWA).
 */
function isFilesAppId(appId) {
  return appId === LEGACY_FILES_EXTENSION_ID || appId === SWA_APP_ID;
}

/**
 * @param {!Entry} entry
 * @return {boolean}
 */
function hasOfficeExtension(entry) {
  return OFFICE_EXTENSIONS.has(FileType.getExtension(entry));
}

/**
 * The SWA actionId is prefixed with chrome://file-manager/?ACTION_ID, just the
 * sub-string compatible with the extension/legacy e.g.: "view-pdf".
 *
 * @param {string} actionId
 * @return {string}
 */
export function parseActionId(actionId) {
  if (window.isSWA) {
    const swaUrl = SWA_FILES_APP_URL.toString() + '?';
    return actionId.replace(swaUrl, '');
  }

  return actionId;
}
