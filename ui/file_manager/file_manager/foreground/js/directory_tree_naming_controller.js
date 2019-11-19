// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Naming controller for directory tree.
 */
class DirectoryTreeNamingController {
  /**
   * @param {!DirectoryModel} directoryModel
   * @param {!DirectoryTree} directoryTree
   * @param {!cr.ui.dialogs.AlertDialog} alertDialog
   */
  constructor(directoryModel, directoryTree, alertDialog) {
    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!DirectoryTree} */
    this.directoryTree_ = directoryTree;

    /** @private @const {!cr.ui.dialogs.AlertDialog} */
    this.alertDialog_ = alertDialog;

    /** @private {?DirectoryItem} */
    this.currentDirectoryItem_ = null;

    /** @private {boolean} */
    this.editing_ = false;

    /** @private {boolean} */
    this.isRemovableRoot_ = false;

    /** @private {?VolumeInfo} */
    this.volumeInfo_ = null;

    /** @private @const {!HTMLInputElement} */
    this.inputElement_ = /** @type {!HTMLInputElement} */
        (document.createElement('input'));
    this.inputElement_.type = 'text';
    this.inputElement_.spellcheck = false;
    this.inputElement_.addEventListener('keydown', this.onKeyDown_.bind(this));
    this.inputElement_.addEventListener('blur', this.commitRename_.bind(this));
    this.inputElement_.addEventListener('click', event => {
      // Stop propagation of click event to prevent it being captured by
      // directory item and current directory is changed to editing item.
      event.stopPropagation();
    });
  }

  /**
   * Returns input element.
   * @return {!HTMLInputElement}
   */
  getInputElement() {
    return this.inputElement_;
  }

  /**
   * Returns the '.label' class element child of this.currentDirectoryItem_.
   * @private
   * @return {!HTMLElement}
   */
  getLabelElement_() {
    const element = this.currentDirectoryItem_.firstElementChild;
    const label = element.querySelector('.label');
    return /** @type {!HTMLElement} */ (assert(label));
  }

  /**
   * Attaches naming controller to specified directory item and start rename.
   * @param {!DirectoryItem} directoryItem An html element of a node of the
   *     target.
   * @param {boolean} isRemovableRoot Indicates whether the target is removable
   *     node or not.
   * @param {VolumeInfo} volumeInfo A volume information about the target node.
   *     |volumeInfo| can be null if method is invoked on a folder that is in
   *     the tree view and is not root of an external drive.
   */
  attachAndStart(directoryItem, isRemovableRoot, volumeInfo) {
    this.isRemovableRoot_ = isRemovableRoot;
    this.volumeInfo_ = this.isRemovableRoot_ ? assert(volumeInfo) : null;

    if (this.currentDirectoryItem_) {
      return;
    }

    this.currentDirectoryItem_ = directoryItem;
    this.currentDirectoryItem_.setAttribute('renaming', true);

    const renameInputElementPlaceholder =
        this.currentDirectoryItem_.firstElementChild.getElementsByClassName(
            'rename-placeholder');

    if (this.isRemovableRoot_ && renameInputElementPlaceholder.length === 1) {
      renameInputElementPlaceholder[0].appendChild(this.inputElement_);
    } else {
      const label = this.getLabelElement_();
      label.insertAdjacentElement('afterend', this.inputElement_);
    }

    this.inputElement_.value = this.currentDirectoryItem_.label;
    this.inputElement_.select();
    this.inputElement_.focus();

    this.editing_ = true;
  }

  /**
   * Commits rename.
   * @private
   */
  commitRename_() {
    if (!this.editing_) {
      return;
    }
    this.editing_ = false;

    const entry = this.currentDirectoryItem_.entry;
    const newName = this.inputElement_.value;

    // If new name is the same as current name or empty, do nothing.
    if (newName === this.currentDirectoryItem_.label || newName.length == 0) {
      this.detach_();
      return;
    }

    if (this.isRemovableRoot_) {
      // Validate new name.
      util.validateExternalDriveName(
              newName, assert(this.volumeInfo_).diskFileSystemType)
          .then(
              this.performExternalDriveRename_.bind(this, entry, newName),
              errorMessage => {
                this.alertDialog_.show(
                    /** @type {string} */ (errorMessage),
                    this.detach_.bind(this));
              });
    } else {
      // Validate new name.
      new Promise(entry.getParent.bind(entry))
          .then(parentEntry => {
            return util.validateFileName(
                parentEntry, newName,
                !this.directoryModel_.getFileFilter().isHiddenFilesVisible());
          })
          .then(
              this.performRename_.bind(this, entry, newName), errorMessage => {
                this.alertDialog_.show(
                    /** @type {string} */ (errorMessage),
                    this.detach_.bind(this));
              });
    }
  }

  /**
   * Performs rename operation.
   * @param {!DirectoryEntry} entry
   * @param {string} newName Validated name.
   * @private
   */
  performRename_(entry, newName) {
    const renamingCurrentDirectory =
        util.isSameEntry(entry, this.directoryModel_.getCurrentDirEntry());
    if (renamingCurrentDirectory) {
      this.directoryModel_.setIgnoringCurrentDirectoryDeletion(
          true /* ignore */);
    }

    // TODO(yawano): Rename might take time on some volumes. Optimistically show
    // new name in the UI before actual rename is completed.
    new Promise(util.rename.bind(null, entry, newName))
        .then(
            newEntry => {
              // Put the new name in the .label element before detaching the
              // <input> to prevent showing the old name.
              this.getLabelElement_().textContent = newName;

              this.currentDirectoryItem_.entry = newEntry;
              this.currentDirectoryItem_.updateSubDirectories(
                  true /* recursive */);

              this.detach_();

              // If renamed directory was current directory, change it to new
              // one.
              if (renamingCurrentDirectory) {
                this.directoryModel_.changeDirectoryEntry(
                    newEntry,
                    this.directoryModel_.setIgnoringCurrentDirectoryDeletion
                        .bind(this.directoryModel_, false /* not ignore */));
              }
            },
            error => {
              this.directoryModel_.setIgnoringCurrentDirectoryDeletion(
                  false /* not ignore */);
              this.detach_();

              this.alertDialog_.show(util.getRenameErrorMessage(
                  /** @type {DOMError} */ (error), entry, newName));
            });
  }

  /**
   * Performs external drive rename operation.
   * @param {!DirectoryEntry} entry
   * @param {string} newName Validated name.
   * @private
   */
  performExternalDriveRename_(entry, newName) {
    // Invoke external drive rename
    chrome.fileManagerPrivate.renameVolume(this.volumeInfo_.volumeId, newName);

    // Put the new name in the .label element before detaching the <input> to
    // prevent showing the old name.
    this.getLabelElement_().textContent = newName;
    this.detach_();
  }

  /**
   * Cancels rename.
   * @private
   */
  cancelRename_() {
    if (!this.editing_) {
      return;
    }

    this.editing_ = false;
    this.detach_();
  }

  /**
   * Detaches controller from current directory item.
   * @private
   */
  detach_() {
    assert(!!this.currentDirectoryItem_);

    this.inputElement_.remove();

    this.currentDirectoryItem_.removeAttribute('renaming');
    this.currentDirectoryItem_ = null;

    // Restore focus to directory tree.
    this.directoryTree_.focus();
  }

  /**
   * Handles keydown event.
   * @param {!Event} event
   * @private
   */
  onKeyDown_(event) {
    // Ignore key events if event.keyCode is VK_PROCESSKEY(229).
    // TODO(fukino): Remove this workaround once crbug.com/644140 is fixed.
    if (event.keyCode === 229) {
      return;
    }

    event.stopPropagation();

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
}
