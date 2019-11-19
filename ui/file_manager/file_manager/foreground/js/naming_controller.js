// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Controller to handle naming.
 */
class NamingController {
  /**
   * @param {!ListContainer} listContainer
   * @param {!cr.ui.dialogs.AlertDialog} alertDialog
   * @param {!cr.ui.dialogs.ConfirmDialog} confirmDialog
   * @param {!DirectoryModel} directoryModel
   * @param {!FileFilter} fileFilter
   * @param {!FileSelectionHandler} selectionHandler
   */
  constructor(
      listContainer, alertDialog, confirmDialog, directoryModel, fileFilter,
      selectionHandler) {
    /** @private @const {!ListContainer} */
    this.listContainer_ = listContainer;

    /** @private @const {!cr.ui.dialogs.AlertDialog} */
    this.alertDialog_ = alertDialog;

    /** @private @const {!cr.ui.dialogs.ConfirmDialog} */
    this.confirmDialog_ = confirmDialog;

    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!FileFilter} */
    this.fileFilter_ = fileFilter;

    /** @private @const {!FileSelectionHandler} */
    this.selectionHandler_ = selectionHandler;

    // Register events.
    this.listContainer_.renameInput.addEventListener(
        'keydown', this.onRenameInputKeyDown_.bind(this));
    this.listContainer_.renameInput.addEventListener(
        'blur', this.onRenameInputBlur_.bind(this));
  }

  /**
   * Verifies the user entered name for file or folder to be created or
   * renamed to. See also util.validateFileName.
   *
   * @param {!DirectoryEntry} parentEntry The URL of the parent directory entry.
   * @param {string} name New file or folder name.
   * @param {function(boolean)} onDone Function to invoke when user closes the
   *    warning box or immediately if file name is correct. If the name was
   *    valid it is passed true, and false otherwise.
   */
  validateFileName(parentEntry, name, onDone) {
    const fileNameErrorPromise = util.validateFileName(
        parentEntry, name, !this.fileFilter_.isHiddenFilesVisible());
    fileNameErrorPromise
        .then(
            onDone.bind(null, true),
            message => {
              this.alertDialog_.show(
                  /** @type {string} */ (message), onDone.bind(null, false));
            })
        .catch(error => {
          console.error(error.stack || error);
        });
  }

  /**
   * @param {string} filename
   * @return {Promise<string>}
   */
  validateFileNameForSaving(filename) {
    const directory =
        /** @type {DirectoryEntry} */ (
            this.directoryModel_.getCurrentDirEntry());
    const currentDirUrl = directory.toURL().replace(/\/?$/, '/');
    const fileUrl = currentDirUrl + encodeURIComponent(filename);

    return new Promise(this.validateFileName.bind(this, directory, filename))
        .then(isValid => {
          if (!isValid) {
            return Promise.reject('Invalid filename.');
          }

          if (directory && util.isFakeEntry(directory)) {
            // Can't save a file into a fake directory.
            return Promise.reject('Cannot save into fake entry.');
          }

          return new Promise(
              directory.getFile.bind(directory, filename, {create: false}));
        })
        .then(
            () => {
              // An existing file is found. Show confirmation dialog to
              // overwrite it. If the user select "OK" on the dialog, save it.
              return new Promise((fulfill, reject) => {
                this.confirmDialog_.show(
                    strf('CONFIRM_OVERWRITE_FILE', filename),
                    fulfill.bind(null, fileUrl), reject.bind(null, 'Cancelled'),
                    () => {});
              });
            },
            error => {
              if (error.name == util.FileError.NOT_FOUND_ERR) {
                // The file does not exist, so it should be ok to create a
                // new file.
                return fileUrl;
              }

              if (error.name == util.FileError.TYPE_MISMATCH_ERR) {
                // An directory is found.
                // Do not allow to overwrite directory.
                this.alertDialog_.show(
                    strf('DIRECTORY_ALREADY_EXISTS', filename));
                return Promise.reject(error);
              }

              // Unexpected error.
              console.error('File save failed: ' + error.code);
              return Promise.reject(error);
            });
  }

  /**
   * @return {boolean}
   */
  isRenamingInProgress() {
    return !!this.listContainer_.renameInput.currentEntry;
  }

  initiateRename() {
    const item = this.listContainer_.currentList.ensureLeadItemExists();
    if (!item) {
      return;
    }
    const label = item.querySelector('.filename-label');
    const input = this.listContainer_.renameInput;
    const currentEntry =
        this.listContainer_.currentList.dataModel.item(item.listIndex);

    input.value = label.textContent;
    item.setAttribute('renaming', '');
    label.parentNode.appendChild(input);
    input.focus();

    const selectionEnd = input.value.lastIndexOf('.');
    if (currentEntry.isFile && selectionEnd !== -1) {
      input.selectionStart = 0;
      input.selectionEnd = selectionEnd;
    } else {
      input.select();
    }

    // This has to be set late in the process so we don't handle spurious
    // blur events.
    input.currentEntry = currentEntry;
    this.listContainer_.startBatchUpdates();
  }

  /**
   * Restores the item which is being renamed while refreshing the file list. Do
   * nothing if no item is being renamed or such an item disappeared.
   *
   * While refreshing file list it gets repopulated with new file entries.
   * There is not a big difference whether DOM items stay the same or not.
   * Except for the item that the user is renaming.
   */
  restoreItemBeingRenamed() {
    if (!this.isRenamingInProgress()) {
      return;
    }

    const dm = this.directoryModel_;
    const leadIndex = dm.getFileListSelection().leadIndex;
    if (leadIndex < 0) {
      return;
    }

    const leadEntry = /** @type {Entry} */ (dm.getFileList().item(leadIndex));
    if (!util.isSameEntry(
            this.listContainer_.renameInput.currentEntry, leadEntry)) {
      return;
    }

    const leadListItem = this.listContainer_.findListItemForNode(
        this.listContainer_.renameInput);
    if (this.listContainer_.currentListType == ListContainer.ListType.DETAIL) {
      this.listContainer_.table.updateFileMetadata(leadListItem, leadEntry);
    }
    this.listContainer_.currentList.restoreLeadItem(leadListItem);
  }

  /**
   * @param {Event} event Key event.
   * @private
   */
  onRenameInputKeyDown_(event) {
    // Ignore key events if event.keyCode is VK_PROCESSKEY(229).
    // TODO(fukino): Remove this workaround once crbug.com/644140 is fixed.
    if (event.keyCode === 229) {
      return;
    }

    if (!this.isRenamingInProgress()) {
      return;
    }

    // Do not move selection or lead item in list during rename.
    if (event.key === 'ArrowUp' || event.key === 'ArrowDown') {
      event.stopPropagation();
    }

    switch (util.getKeyModifiers(event) + event.key) {
      case 'Escape':
        this.cancelRename_();
        event.preventDefault();
        break;

      case 'Enter':
        this.commitRename_();
        event.preventDefault();
        break;
    }
  }

  /**
   * @param {Event} event Blur event.
   * @private
   */
  onRenameInputBlur_(event) {
    if (this.isRenamingInProgress() &&
        !this.listContainer_.renameInput.validation_) {
      this.commitRename_();
    }
  }

  /**
   * @private
   */
  commitRename_() {
    const input = this.listContainer_.renameInput;
    const entry = input.currentEntry;
    const newName = input.value;

    if (!newName || newName == entry.name) {
      this.cancelRename_();
      return;
    }

    const renamedItemElement = this.listContainer_.findListItemForNode(
        this.listContainer_.renameInput);
    const nameNode = renamedItemElement.querySelector('.filename-label');

    input.validation_ = true;
    const validationDone = valid => {
      input.validation_ = false;

      if (!valid) {
        // Cancel rename if it fails to restore focus from alert dialog.
        // Otherwise, just cancel the commitment and continue to rename.
        if (document.activeElement != input) {
          this.cancelRename_();
        }
        return;
      }

      // Validation succeeded. Do renaming.
      this.listContainer_.renameInput.currentEntry = null;
      if (this.listContainer_.renameInput.parentNode) {
        this.listContainer_.renameInput.parentNode.removeChild(
            this.listContainer_.renameInput);
      }
      renamedItemElement.setAttribute('renaming', 'provisional');

      // Optimistically apply new name immediately to avoid flickering in
      // case of success.
      nameNode.textContent = newName;

      util.rename(
          entry, newName,
          newEntry => {
            this.directoryModel_.onRenameEntry(entry, assert(newEntry), () => {
              // Select new entry.
              this.listContainer_.currentList.selectionModel.selectedIndex =
                  this.directoryModel_.getFileList().indexOf(newEntry);
              // Force to update selection immediately.
              this.selectionHandler_.onFileSelectionChanged();

              renamedItemElement.removeAttribute('renaming');
              this.listContainer_.endBatchUpdates();

              // Focus may go out of the list. Back it to the list.
              this.listContainer_.currentList.focus();
            });
          },
          error => {
            // Write back to the old name.
            nameNode.textContent = entry.name;
            renamedItemElement.removeAttribute('renaming');
            this.listContainer_.endBatchUpdates();

            // Show error dialog.
            const message = util.getRenameErrorMessage(error, entry, newName);
            this.alertDialog_.show(message);
          });
    };

    // TODO(mtomasz): this.getCurrentDirectoryEntry() might not return the
    // actual parent if the directory content is a search result. Fix it to do
    // proper validation.
    this.validateFileName(
        /** @type {!DirectoryEntry} */ (
            this.directoryModel_.getCurrentDirEntry()),
        newName, validationDone.bind(this));
  }

  /**
   * @private
   */
  cancelRename_() {
    this.listContainer_.renameInput.currentEntry = null;

    const item = this.listContainer_.findListItemForNode(
        this.listContainer_.renameInput);
    if (item) {
      item.removeAttribute('renaming');
    }

    const parent = this.listContainer_.renameInput.parentNode;
    if (parent) {
      parent.removeChild(this.listContainer_.renameInput);
    }

    this.listContainer_.endBatchUpdates();

    // Focus may go out of the list. Back it to the list.
    this.listContainer_.currentList.focus();
  }
}
