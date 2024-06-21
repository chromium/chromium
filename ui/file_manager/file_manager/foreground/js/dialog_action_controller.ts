// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertNotReached} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {isFolderDialogType} from '../../common/js/dialog_type.js';
import type {FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {recordEnum} from '../../common/js/metrics.js';
import {str} from '../../common/js/translations.js';
import {recordViewingNavigationSurfaceUma, recordViewingVolumeTypeUma} from '../../common/js/uma.js';
import {testSendMessage, UserCanceledError} from '../../common/js/util.js';
import {AllowedPaths, RootTypesForUMA} from '../../common/js/volume_manager_types.js';
import {DialogType} from '../../state/state.js';
import {getStore} from '../../state/store.js';

import type {FileFilter} from './directory_contents.js';
import type {DirectoryModel} from './directory_model.js';
import {EventType, type FileSelectionHandler} from './file_selection.js';
import type {LaunchParam} from './launch_param.js';
import type {NamingController} from './naming_controller.js';
import type {Command} from './ui/command.js';
import type {DialogFooter} from './ui/dialog_footer.js';

interface SelectFilesAndCloseParams {
  urls: string[];
  multiple: boolean;
  filterIndex?: number;
}

/**
 * Controller for handling behaviors of the Files app opened as a file/folder
 * selection dialog.
 */
export class DialogActionController {
  private fileTypes_: LaunchParam['typeList'];
  private allowedPaths_: AllowedPaths;
  /**
   * Bound function for onCancel_.
   */
  private onCancelBound_ = this.processCancelAction_.bind(this);
  private newFolderCommand_: Command =
      document.querySelector<Command>('#new-folder')!;

  /**
   * @param dialogType Dialog type.
   * @param dialogFooter Dialog footer.
   * @param directoryModel Directory model.
   * @param volumeManager Volume manager.
   * @param fileFilter File filter model.
   * @param namingController Naming controller.
   * @param fileSelectionHandler Initial file selection.
   * @param launchParam Whether the dialog should return local path or not.
   */
  constructor(
      private dialogType_: DialogType, private dialogFooter_: DialogFooter,
      private directoryModel_: DirectoryModel,
      private volumeManager_: VolumeManager, private fileFilter_: FileFilter,
      private namingController_: NamingController,
      private fileSelectionHandler_: FileSelectionHandler,
      launchParam: LaunchParam) {
    /**
     * List of acceptable file types for open dialog.
     */
    this.fileTypes_ = launchParam.typeList || [];
    this.allowedPaths_ = launchParam.allowedPaths;

    this.dialogFooter_.okButton.addEventListener(
        'click', this.processOkAction_.bind(this));
    this.dialogFooter_.cancelButton.addEventListener(
        'click', this.onCancelBound_);
    this.dialogFooter_.newFolderButton.addEventListener(
        'click', this.processNewFolderAction_.bind(this));
    this.dialogFooter_.fileTypeSelector?.addEventListener(
        'change', this.onFileTypeFilterChanged_.bind(this));
    this.dialogFooter_.filenameInput.addEventListener(
        'input', this.updateOkButton_.bind(this));
    this.fileSelectionHandler_.addEventListener(
        EventType.CHANGE_THROTTLED, this.onFileSelectionChanged_.bind(this));
    this.volumeManager_.addEventListener(
        'drive-connection-changed', this.updateOkButton_.bind(this));

    this.dialogFooter_.initFileTypeFilter(
        this.fileTypes_, launchParam.includeAllFiles);
    this.onFileTypeFilterChanged_();

    this.newFolderCommand_.addEventListener(
        'disabledChange', this.updateNewFolderButton_.bind(this));
  }

  /**
   */
  private async processOkActionForSaveDialog_() {
    const selection = this.fileSelectionHandler_.selection;

    // If OK action is clicked when a directory is selected, open the directory.
    if (selection.directoryCount === 1 && selection.fileCount === 0) {
      this.directoryModel_.changeDirectoryEntry(
          selection.entries[0] as DirectoryEntry | FilesAppDirEntry);
      return;
    }

    // Save-as doesn't require a valid selection from the list, since
    // we're going to take the filename from the text input.
    this.updateExtensionForSelectedFileType_(true);
    const filename = this.dialogFooter_.filenameInput.value;
    if (!filename) {
      console.warn('Missing filename');
      return;
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
   */
  private processOkAction_() {
    if (this.dialogFooter_.okButton.disabled) {
      console.warn('okButton Disabled');
      return;
    }
    if (this.dialogType_ === DialogType.SELECT_SAVEAS_FILE) {
      this.processOkActionForSaveDialog_();
      return;
    }

    const files = [];
    const selectedIndexes =
        this.directoryModel_.getFileListSelection().selectedIndexes;

    if (isFolderDialogType(this.dialogType_) && selectedIndexes.length === 0) {
      const url = this.directoryModel_.getCurrentDirEntry()!.toURL();
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
      console.warn('Nothing selected in the file list');
      return;
    }

    const dm = this.directoryModel_.getFileList();
    for (let i = 0; i < selectedIndexes.length; i++) {
      const index = selectedIndexes[i];
      const entry = index === undefined ? null : dm.item(index);
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
      console.warn('Too many files selected');
      return;
    }

    const selectedEntry = dm.item(selectedIndexes[0] ?? -1)!;

    if (isFolderDialogType(this.dialogType_)) {
      if (!selectedEntry.isDirectory) {
        console.warn('Selected entry is not a folder');
        return;
      }
    } else if (this.dialogType_ === DialogType.SELECT_OPEN_FILE) {
      if (!selectedEntry.isFile) {
        console.warn('Selected entry is not a file');
        return;
      }
    }

    const singleSelection: SelectFilesAndCloseParams = {
      urls: [files[0]!],
      multiple: false,
      filterIndex: this.dialogFooter_.selectedFilterIndex,
    };
    this.selectFilesAndClose_(singleSelection);
  }

  /**
   * Cancels file selection and closes the file selection dialog.
   */
  private processCancelAction_() {
    chrome.fileManagerPrivate.cancelDialog();
    window.close();
  }

  /**
   * Creates a new folder using new-folder command.
   */
  private processNewFolderAction_() {
    this.newFolderCommand_.canExecuteChange(this.dialogFooter_.newFolderButton);
    this.newFolderCommand_.execute(this.dialogFooter_.newFolderButton);
  }

  /**
   * Handles disabledChange event to update the new-folder button's
   * avaliability.
   */
  private updateNewFolderButton_() {
    this.dialogFooter_.newFolderButton.disabled =
        this.newFolderCommand_.disabled;
  }

  /**
   * Tries to close this modal dialog with some files selected.
   * Performs preprocessing if needed (e.g. for Drive).
   * @param selection Contains urls, filterIndex and multiple fields.
   */
  private selectFilesAndClose_(selection: SelectFilesAndCloseParams) {
    const currentRootType = this.directoryModel_.getCurrentRootType();
    const onFileSelected = () => {
      if (!chrome.runtime.lastError) {
        // Call next method on a timeout, as it's unsafe to
        // close a window from a callback.
        setTimeout(window.close.bind(window), 0);
      }
    };
    // Record the root types of chosen files in OPEN dialog.
    if (this.dialogType_ === DialogType.SELECT_OPEN_FILE ||
        this.dialogType_ === DialogType.SELECT_OPEN_MULTI_FILE) {
      recordEnum('OpenFiles.RootType', currentRootType, RootTypesForUMA);
    }

    const state = getStore().getState();
    for (const url of selection.urls) {
      recordViewingVolumeTypeUma(state, url);
      // Recorded per file.
      recordViewingNavigationSurfaceUma(state);
    }

    if (selection.multiple) {
      chrome.fileManagerPrivate.selectFiles(
          selection.urls, this.allowedPaths_ === AllowedPaths.NATIVE_PATH,
          onFileSelected);
    } else {
      chrome.fileManagerPrivate.selectFile(
          selection.urls[0]!, selection.filterIndex!,
          this.dialogType_ !== DialogType.SELECT_SAVEAS_FILE /* for opening */,
          this.allowedPaths_ === AllowedPaths.NATIVE_PATH, onFileSelected);
    }
  }

  /**
   * Returns the regex to match against files for the current filter.
   */
  private regexpForCurrentFilter_(): RegExp|null {
    // Note selectedFilterIndex indexing is 1-based. (0 is "all files").
    const selectedIndex = this.dialogFooter_.selectedFilterIndex;
    if (selectedIndex < 1) {
      return null;  // No specific filter selected.
    }
    return new RegExp(
        '\\.(' + this.fileTypes_[selectedIndex - 1]!.extensions.join('|') +
            ')$',
        'i');
  }

  /**
   * Updates the file input field to agree with the current filter.
   * @param forConfirm The update is for the final confirm step.
   */
  private updateExtensionForSelectedFileType_(forConfirm: boolean) {
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
    const newExtension = this.fileTypes_[selectedIndex - 1]!.extensions[0];
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
   */
  private onFileTypeFilterChanged_() {
    this.fileFilter_.removeFilter('fileType');
    const regexp = this.regexpForCurrentFilter_();
    if (!regexp) {
      return;
    }

    const filter = (entry: Entry|FilesAppEntry) =>
        entry.isDirectory || regexp.test(entry.name);
    this.fileFilter_.addFilter('fileType', filter);

    // In save dialog, update the destination name extension.
    if (this.dialogType_ === DialogType.SELECT_SAVEAS_FILE) {
      this.updateExtensionForSelectedFileType_(false);
    }
  }

  /**
   * Handles selection change.
   */
  private onFileSelectionChanged_() {
    // If this is a save-as dialog, copy the selected file into the filename
    // input text box.
    const selection = this.fileSelectionHandler_.selection;
    if (this.dialogType_ === DialogType.SELECT_SAVEAS_FILE &&
        selection.totalCount === 1 && selection.entries[0]!.isFile &&
        this.dialogFooter_.filenameInput.value !== selection.entries[0]!.name) {
      this.dialogFooter_.filenameInput.value = selection.entries[0]!.name;
    }

    this.updateOkButton_();
    if (!this.dialogFooter_.okButton.disabled) {
      testSendMessage('dialog-ready');
    }
  }

  /**
   * Updates the Ok button enabled state.
   */
  private updateOkButton_() {
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
