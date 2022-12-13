// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {getKeyModifiers} from '../../common/js/dom_utils.js';
import {util} from '../../common/js/util.js';
import {VolumeInfo} from '../../externs/volume_info.js';

import {DirectoryModel} from './directory_model.js';
import {renameEntry, validateEntryName} from './file_rename.js';
import {DirectoryItem, DirectoryTree} from './ui/directory_tree.js';
import {FilesAlertDialog} from './ui/files_alert_dialog.js';

/**
 * Naming controller for directory tree.
 */
export class DirectoryTreeNamingController {
  /**
   * @param {!DirectoryModel} directoryModel
   * @param {!DirectoryTree} directoryTree
   * @param {!FilesAlertDialog} alertDialog
   */
  constructor(directoryModel, directoryTree, alertDialog) {
    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!DirectoryTree} */
    this.directoryTree_ = directoryTree;

    /** @private @const {!FilesAlertDialog} */
    this.alertDialog_ = alertDialog;

    /** @private {?DirectoryItem} */
    this.currentDirectoryItem_ = null;

    /** @private {boolean} */
    this.editing_ = false;

    /**
     * Whether the entry being renamed is a root of a removable
     * partition/volume.
     * @private {boolean}
     */
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
   * @param {boolean} isRemovableRoot Indicates whether the target is a
   *     removable volume root or not.
   * @param {VolumeInfo} volumeInfo A volume information about the target entry.
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
  async commitRename_() {
    const contextMenu = this.inputElement_.contextMenu;
    if (!this.editing_ || (contextMenu && !contextMenu.hidden)) {
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

    try {
      await validateEntryName(
          entry, newName,
          this.directoryModel_.getFileFilter().isHiddenFilesVisible(),
          this.volumeInfo_, this.isRemovableRoot_);
      await this.performRename_(entry, newName);
    } catch (error) {
      await this.alertDialog_.showAsync(/** @type {string} */ (error.message));
      this.editing_ = true;
    }
  }

  /**
   * Performs rename operation.
   * @param {!DirectoryEntry} entry
   * @param {string} newName Validated name.
   * @private
   */
  async performRename_(entry, newName) {
    const renamingCurrentDirectory =
        util.isSameEntry(entry, this.directoryModel_.getCurrentDirEntry());
    if (renamingCurrentDirectory) {
      this.directoryModel_.setIgnoringCurrentDirectoryDeletion(
          true /* ignore */);
    }

    // TODO(yawano): Rename might take time on some volumes. Optimistically show
    // new name in the UI before actual rename is completed.
    try {
      const newEntry = await renameEntry(
          entry, newName, this.volumeInfo_, this.isRemovableRoot_);

      // Put the new name in the .label element before detaching the
      // <input> to prevent showing the old name.
      this.getLabelElement_().textContent = newName;

      // We currently don't have promises/callbacks for when removableRoots are
      // successfully renamed, so we can't update their subdirectories or
      // update the current directory to them at this point.
      if (this.isRemovableRoot_) {
        return;
      }

      this.currentDirectoryItem_.entry = newEntry;
      this.currentDirectoryItem_.updateSubDirectories(/* recursive= */ true);
      if (window.IN_TEST) {
        this.currentDirectoryItem_.setAttribute('entry-label', newName);
      }

      // If renamed directory was current directory, change it to new one.
      if (renamingCurrentDirectory) {
        this.directoryModel_.changeDirectoryEntry(
            /** @type {!DirectoryEntry} */ (newEntry),
            this.directoryModel_.setIgnoringCurrentDirectoryDeletion.bind(
                this.directoryModel_, /* ignore= */ false));
      }
    } catch (error) {
      this.directoryModel_.setIgnoringCurrentDirectoryDeletion(
          /* ignore= */ false);

      this.alertDialog_.show(/** @type {string} */ (error.message));
    } finally {
      this.detach_();
    }
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
    event.stopPropagation();

    switch (getKeyModifiers(event) + event.key) {
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
