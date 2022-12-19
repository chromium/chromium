// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/ash/common/assert.js';
import {$} from 'chrome://resources/ash/common/util.js';

import {DialogType, isFolderDialogType} from '../../common/js/dialog_type.js';
import {metrics} from '../../common/js/metrics.js';
import {str, UserCanceledError, util} from '../../common/js/util.js';
import {AllowedPaths, VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {FileSelectionHandler} from './file_selection.js';
import {LaunchParam} from './launch_param.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {NamingController} from './naming_controller.js';
import {Command} from './ui/command.js';
import {DialogFooter} from './ui/dialog_footer.js';

/**
 * Controler for handling behaviors of the Files app opened as a file/folder
 * selection dialog.
 */
export class DialogActionController {
  /**
   * @param {!DialogType} dialogType Dialog type.
   * @param {!DialogFooter} dialogFooter Dialog footer.
   * @param {!DirectoryModel} directoryModel Directory model.
   * @param {!MetadataModel} metadataModel Metadata cache.
   * @param {!VolumeManager} volumeManager Volume manager.
   * @param {!FileFilter} fileFilter File filter model.
   * @param {!NamingController} namingController Naming controller.
   * @param {!FileSelectionHandler} fileSelectionHandler Initial file selection.
   * @param {!LaunchParam} launchParam Whether the dialog should return local
   *     path or not.
   */
  constructor(
      dialogType, dialogFooter, directoryModel, metadataModel, volumeManager,
      fileFilter, namingController, fileSelectionHandler, launchParam) {
    /** @private @const {!DialogType} */
    this.dialogType_ = dialogType;

    /** @private @const {!DialogFooter} */
    this.dialogFooter_ = dialogFooter;

    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!MetadataModel} */
    this.metadataModel_ = metadataModel;

    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;

    /** @private @const {!FileFilter} */
    this.fileFilter_ = fileFilter;

    /** @private @const {!NamingController} */
    this.namingController_ = namingController;

    /** @private @const {!FileSelectionHandler} */
    this.fileSelectionHandler_ = fileSelectionHandler;

    /**
     * List of acceptable file types for open dialog.
     * @private @const {!Array<Object>}
     */
    this.fileTypes_ = launchParam.typeList || [];

    /** @private @const {!AllowedPaths} */
    this.allowedPaths_ = launchParam.allowedPaths;

    /**
     * Bound function for onCancel_.
     * @private @const {!function(this:DialogActionController, Event)}
     */
    this.onCancelBound_ = this.processCancelAction_.bind(this);

    dialogFooter.okButton.addEventListener(
        'click', this.processOKAction_.bind(this));
    dialogFooter.cancelButton.addEventListener('click', this.onCancelBound_);
    dialogFooter.newFolderButton.addEventListener(
        'click', this.processNewFolderAction_.bind(this));
    dialogFooter.fileTypeSelector.addEventListener(
        'change', this.onFileTypeFilterChanged_.bind(this));
    dialogFooter.filenameInput.addEventListener(
        'input', this.updateOkButton_.bind(this));
    fileSelectionHandler.addEventListener(
        FileSelectionHandler.EventType.CHANGE_THROTTLED,
        this.onFileSelectionChanged_.bind(this));
    volumeManager.addEventListener(
        'drive-connection-changed', this.updateOkButton_.bind(this));

    dialogFooter.initFileTypeFilter(
        this.fileTypes_, launchParam.includeAllFiles);
    this.onFileTypeFilterChanged_();

    this.newFolderCommand_ =
        /** @type {Command} */ ($('new-folder'));
    this.newFolderCommand_.addEventListener(
        'disabledChange', this.updateNewFolderButton_.bind(this));
  }

  /**
   * @private
   */
  async processOKActionForSaveDialog_() {
    const selection = this.fileSelectionHandler_.selection;

    // If OK action is clicked when a directory is selected, open the directory.
    if (selection.directoryCount === 1 && selection.fileCount === 0) {
      this.directoryModel_.changeDirectoryEntry(
          /** @type {!DirectoryEntry} */ (selection.entries[0]));
      return;
    }

    // Save-as doesn't require a valid selection from the list, since
    // we're going to take the filename from the text input.
    this.updateExtensionForSelectedFileType_(true);
    const filename = this.dialogFooter_.filenameInput.value;
    if (!filename) {
      throw new Error('Missing filename!');
    }

    try {
      const url =
          await this.namingController_.validateFileNameForSaving(filename);

      this.selectFilesAndClose_({
        urls: [url],
        multiple: false,
        filterIndex: this.dialogFooter_.selectedFilterIndex,
      });
    } catch (error) {
      if (!(error instanceof UserCanceledError)) {
        console.warn(error);
      }
    }
  }

  /**
   * Handle a click of the ok button.
   *
   * The ok button has different UI labels depending on the type of dialog, but
   * in code it's always referred to as 'ok'.
   *
   * @private
   */
  processOKAction_() {
    if (this.dialogFooter_.okButton.disabled) {
      throw new Error('Disabled!');
    }
    if (this.dialogType_ === DialogType.SELECT_SAVEAS_FILE) {
      this.processOKActionForSaveDialog_();
      return;
    }

    const files = [];
    const selectedIndexes =
        this.directoryModel_.getFileListSelection().selectedIndexes;

    if (isFolderDialogType(this.dialogType_) && selectedIndexes.length === 0) {
      const url = this.directoryModel_.getCurrentDirEntry().toURL();
      const singleSelection = {
        urls: [url],
        multiple: false,
        filterIndex: this.dialogFooter_.selectedFilterIndex,
      };
      this.selectFilesAndClose_(singleSelection);
      return;
    }

    // All other dialog types require at least one selected list item.
    // The logic to control whether or not the ok button is enabled should
    // prevent us from ever getting here, but we sanity check to be sure.
    if (!selectedIndexes.length) {
      throw new Error('Nothing selected!');
    }

    const dm = this.directoryModel_.getFileList();
    for (let i = 0; i < selectedIndexes.length; i++) {
      const entry = dm.item(selectedIndexes[i]);
      if (!entry) {
        console.warn('Error locating selected file at index: ' + i);
        continue;
      }

      files.push(entry.toURL());
    }

    // Multi-file selection has no other restrictions.
    if (this.dialogType_ === DialogType.SELECT_OPEN_MULTI_FILE) {
      const multipleSelection = {
        urls: files,
        multiple: true,
      };
      this.selectFilesAndClose_(multipleSelection);
      return;
    }

    // Everything else must have exactly one.
    if (files.length > 1) {
      throw new Error('Too many files selected!');
    }

    const selectedEntry = dm.item(selectedIndexes[0]);

    if (isFolderDialogType(this.dialogType_)) {
      if (!selectedEntry.isDirectory) {
        throw new Error('Selected entry is not a folder!');
      }
    } else if (this.dialogType_ === DialogType.SELECT_OPEN_FILE) {
      if (!selectedEntry.isFile) {
        throw new Error('Selected entry is not a file!');
      }
    }

    const singleSelection = {
      urls: [files[0]],
      multiple: false,
      filterIndex: this.dialogFooter_.selectedFilterIndex,
    };
    this.selectFilesAndClose_(singleSelection);
  }

  /**
   * Cancels file selection and closes the file selection dialog.
   * @private
   */
  processCancelAction_() {
    chrome.fileManagerPrivate.cancelDialog();
    window.close();
  }

  /**
   * Creates a new folder using new-folder command.
   * @private
   */
  processNewFolderAction_() {
    this.newFolderCommand_.canExecuteChange(this.dialogFooter_.newFolderButton);
    this.newFolderCommand_.execute(this.dialogFooter_.newFolderButton);
  }

  /**
   * Handles disabledChange event to update the new-folder button's
   * avaliability.
   * @private
   */
  updateNewFolderButton_() {
    this.dialogFooter_.newFolderButton.disabled =
        this.newFolderCommand_.disabled;
  }

  /**
   * Tries to close this modal dialog with some files selected.
   * Performs preprocessing if needed (e.g. for Drive).
   * @param {Object} selection Contains urls, filterIndex and multiple fields.
   * @private
   */
  selectFilesAndClose_(selection) {
    const currentRootType = this.directoryModel_.getCurrentRootType();
    const onFileSelected = () => {
      if (!chrome.runtime.lastError) {
        // Call next method on a timeout, as it's unsafe to
        // close a window from a callback.
        setTimeout(window.close.bind(window), 0);
      }
    };
    // Record the root types of chosen files in OPEN dialog.
    if (this.dialogType_ == DialogType.SELECT_OPEN_FILE ||
        this.dialogType_ == DialogType.SELECT_OPEN_MULTI_FILE) {
      metrics.recordEnum(
          'OpenFiles.RootType', currentRootType,
          VolumeManagerCommon.RootTypesForUMA);
    }
    if (selection.multiple) {
      chrome.fileManagerPrivate.selectFiles(
          selection.urls, this.allowedPaths_ === AllowedPaths.NATIVE_PATH,
          onFileSelected);
    } else {
      chrome.fileManagerPrivate.selectFile(
          selection.urls[0], selection.filterIndex,
          this.dialogType_ !== DialogType.SELECT_SAVEAS_FILE /* for opening */,
          this.allowedPaths_ === AllowedPaths.NATIVE_PATH, onFileSelected);
    }
  }

  /**
   * Returns the regex to match against files for the current filter.
   * @return {?RegExp}
   */
  regexpForCurrentFilter_() {
    // Note selectedFilterIndex indexing is 1-based. (0 is "all files").
    const selectedIndex = this.dialogFooter_.selectedFilterIndex;
    if (selectedIndex < 1) {
      return null;  // No specific filter selected.
    }
    return new RegExp(
        '\\.(' + this.fileTypes_[selectedIndex - 1].extensions.join('|') + ')$',
        'i');
  }

  /**
   * Updates the file input field to agree with the current filter.
   * @param {boolean} forConfirm The update is for the final confirm step.
   * @private
   */
  updateExtensionForSelectedFileType_(forConfirm) {
    const regexp = this.regexpForCurrentFilter_();
    if (!regexp) {
      return;  // No filter selected.
    }

    let filename = this.dialogFooter_.filenameInput.value;
    if (!filename || regexp.test(filename)) {
      return;  // Filename empty or already satisfies filter.
    }

    const selectedIndex = this.dialogFooter_.selectedFilterIndex;
    assert(selectedIndex > 0);  // Otherwise there would be no regex.
    const newExtension = this.fileTypes_[selectedIndex - 1].extensions[0];
    if (!newExtension) {
      return;  // No default extension.
    }

    const extensionIndex = filename.lastIndexOf('.');
    if (extensionIndex < 0) {
      // No current extension.
      if (!forConfirm) {
        return;  // Add one later.
      }
      filename = `${filename}.${newExtension}`;
    } else {
      if (forConfirm) {
        return;  // Keep the current user choice.
      }
      filename = `${filename.substr(0, extensionIndex)}.${newExtension}`;
    }

    this.dialogFooter_.filenameInput.value = filename;
    this.dialogFooter_.selectTargetNameInFilenameInput();
  }

  /**
   * Filters file according to the selected file type.
   * @private
   */
  onFileTypeFilterChanged_() {
    this.fileFilter_.removeFilter('fileType');
    const regexp = this.regexpForCurrentFilter_();
    if (!regexp) {
      return;
    }

    const filter = entry => entry.isDirectory || regexp.test(entry.name);
    this.fileFilter_.addFilter('fileType', filter);

    // In save dialog, update the destination name extension.
    if (this.dialogType_ === DialogType.SELECT_SAVEAS_FILE) {
      this.updateExtensionForSelectedFileType_(false);
    }
  }

  /**
   * Handles selection change.
   * @private
   */
  onFileSelectionChanged_() {
    // If this is a save-as dialog, copy the selected file into the filename
    // input text box.
    const selection = this.fileSelectionHandler_.selection;
    if (this.dialogType_ === DialogType.SELECT_SAVEAS_FILE &&
        selection.totalCount === 1 && selection.entries[0].isFile &&
        this.dialogFooter_.filenameInput.value !== selection.entries[0].name) {
      this.dialogFooter_.filenameInput.value = selection.entries[0].name;
    }

    this.updateOkButton_();
    if (!this.dialogFooter_.okButton.disabled) {
      util.testSendMessage('dialog-ready');
    }
  }

  /**
   * Updates the Ok button enabled state.
   * @private
   */
  updateOkButton_() {
    const selection = this.fileSelectionHandler_.selection;

    if (this.dialogType_ === DialogType.FULL_PAGE) {
      // No "select" buttons on the full page UI.
      this.dialogFooter_.okButton.disabled = false;
      return;
    }

    if (isFolderDialogType(this.dialogType_)) {
      // In SELECT_FOLDER mode, we allow to select current directory
      // when nothing is selected.
      this.dialogFooter_.okButton.disabled =
          selection.directoryCount > 1 || selection.fileCount !== 0;
      return;
    }

    if (this.dialogType_ === DialogType.SELECT_SAVEAS_FILE) {
      if (selection.directoryCount === 1 && selection.fileCount === 0) {
        this.dialogFooter_.okButtonLabel.textContent = str('OPEN_LABEL');
        this.dialogFooter_.okButton.disabled =
            this.fileSelectionHandler_.isDlpBlocked();
      } else {
        this.dialogFooter_.okButtonLabel.textContent = str('SAVE_LABEL');
        this.dialogFooter_.okButton.disabled =
            this.directoryModel_.isReadOnly() ||
            this.directoryModel_.isDlpBlocked() ||
            !this.dialogFooter_.filenameInput.value ||
            !this.fileSelectionHandler_.isAvailable();
      }
      return;
    }

    if (this.dialogType_ === DialogType.SELECT_OPEN_FILE) {
      this.dialogFooter_.okButton.disabled = selection.directoryCount !== 0 ||
          selection.fileCount !== 1 ||
          !this.fileSelectionHandler_.isAvailable() ||
          this.fileSelectionHandler_.isDlpBlocked();
      return;
    }

    if (this.dialogType_ === DialogType.SELECT_OPEN_MULTI_FILE) {
      this.dialogFooter_.okButton.disabled = selection.directoryCount !== 0 ||
          selection.fileCount === 0 ||
          !this.fileSelectionHandler_.isAvailable() ||
          this.fileSelectionHandler_.isDlpBlocked();
      return;
    }

    assertNotReached('Unknown dialog type.');
  }
}
