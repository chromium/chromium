// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';

import {getKeyModifiers} from '../../common/js/dom_utils.js';
import {isSameEntry} from '../../common/js/entry_utils.js';
import {isNewDirectoryTreeEnabled} from '../../common/js/flags.js';
import {DirectoryTreeContainer} from '../../containers/directory_tree_container.js';
import {readSubDirectoriesForRenamedEntry} from '../../state/ducks/all_entries.js';
import {getStore} from '../../state/store.js';
import {XfTree} from '../../widgets/xf_tree.js';
import {XfTreeItem} from '../../widgets/xf_tree_item.js';
import {isTreeItem} from '../../widgets/xf_tree_util.js';

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
   * @param {!DirectoryTree|!XfTree} directoryTree
   * @param {DirectoryTreeContainer|null} directoryTreeContainer
   * @param {!FilesAlertDialog} alertDialog
   */
  constructor(
      directoryModel, directoryTree, directoryTreeContainer, alertDialog) {
    /** @private @const @type {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const @type {!DirectoryTree|!XfTree} */
    this.directoryTree_ = directoryTree;

    /** @private @const @type {DirectoryTreeContainer|null} */
    this.directoryTreeContainer_ = directoryTreeContainer;

    /** @private @const @type {!FilesAlertDialog} */
    this.alertDialog_ = alertDialog;

    /** @private @type {?(DirectoryItem|XfTreeItem)} */
    this.currentDirectoryItem_ = null;

    /** @private @type {boolean} */
    this.editing_ = false;

    /**
     * Whether the entry being renamed is a root of a removable
     * partition/volume.
     * @private @type {boolean}
     */
    this.isRemovableRoot_ = false;

    /** @private @type {?import("../../externs/volume_info.js").VolumeInfo} */
    this.volumeInfo_ = null;

    /** @private @const @type {!HTMLInputElement} */
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
    // These events propagation needs to be stopped otherwise ripple will show
    // on the tree item when the input is clicked.
    // Note: 'up/down' are events from <paper-ripple> component.
    const suppressedEvents = ['mouseup', 'mousedown', 'up', 'down'];
    suppressedEvents.forEach(event => {
      this.inputElement_.addEventListener(event, event => {
        event.stopPropagation();
      });
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
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const element = this.currentDirectoryItem_.firstElementChild;
    // @ts-ignore: error TS18047: 'element' is possibly 'null'.
    const label = element.querySelector('.label');
    return /** @type {!HTMLElement} */ (assert(label));
  }

  /**
   * Attaches naming controller to specified directory item and start rename.
   * @param {!(DirectoryItem|XfTreeItem)} directoryItem An html element of a
   *     node of the target.
   * @param {boolean} isRemovableRoot Indicates whether the target is a
   *     removable volume root or not.
   * @param {import("../../externs/volume_info.js").VolumeInfo} volumeInfo A
   *     volume information about the target entry. |volumeInfo| can be null if
   *     method is invoked on a folder that is in the tree view and is not root
   *     of an external drive.
   */
  attachAndStart(directoryItem, isRemovableRoot, volumeInfo) {
    this.isRemovableRoot_ = isRemovableRoot;
    this.volumeInfo_ = this.isRemovableRoot_ ? assert(volumeInfo) : null;

    if (this.currentDirectoryItem_) {
      return;
    }

    this.currentDirectoryItem_ = directoryItem;
    // @ts-ignore: error TS2345: Argument of type 'boolean' is not assignable to
    // parameter of type 'string'.
    this.currentDirectoryItem_.setAttribute('renaming', true);

    if (isTreeItem(directoryItem)) {  // XfTreeItem instance
      this.inputElement_.slot = 'rename';
      this.currentDirectoryItem_.appendChild(this.inputElement_);
    } else {  // DirectoryItem instance
      const renameInputElementPlaceholder =
          // @ts-ignore: error TS2531: Object is possibly 'null'.
          this.currentDirectoryItem_.firstElementChild.getElementsByClassName(
              'rename-placeholder');

      if (this.isRemovableRoot_ && renameInputElementPlaceholder.length === 1) {
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        renameInputElementPlaceholder[0].appendChild(this.inputElement_);
      } else {
        const label = this.getLabelElement_();
        label.insertAdjacentElement('afterend', this.inputElement_);
      }
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
    // @ts-ignore: error TS2551: Property 'contextMenu' does not exist on type
    // 'HTMLInputElement'. Did you mean 'oncontextmenu'?
    const contextMenu = this.inputElement_.contextMenu;
    if (!this.editing_ || (contextMenu && !contextMenu.hidden)) {
      return;
    }
    this.editing_ = false;

    // @ts-ignore: error TS2339: Property 'entry' does not exist on type
    // 'XfTreeItem | DirectoryItem'.
    const entry = this.currentDirectoryItem_.entry;
    const newName = this.inputElement_.value;

    // If new name is the same as current name or empty, do nothing.
    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
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
        isSameEntry(entry, this.directoryModel_.getCurrentDirEntry());
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
      if (isNewDirectoryTreeEnabled()) {
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.currentDirectoryItem_.label = newName;
      } else {
        this.getLabelElement_().textContent = newName;
        if (window.IN_TEST) {
          // @ts-ignore: error TS2531: Object is possibly 'null'.
          this.currentDirectoryItem_.setAttribute('entry-label', newName);
        }
      }

      // We currently don't have promises/callbacks for when removableRoots are
      // successfully renamed, so we can't update their subdirectories or
      // update the current directory to them at this point.
      if (this.isRemovableRoot_) {
        return;
      }

      if (isNewDirectoryTreeEnabled() && this.directoryTreeContainer_) {
        getStore().dispatch(readSubDirectoriesForRenamedEntry(newEntry));
        this.directoryTreeContainer_.focusItemWithKeyWhenRendered(
            newEntry.toURL());
      } else {
        // @ts-ignore: error TS2339: Property 'entry' does not exist on type
        // 'XfTreeItem | DirectoryItem'.
        this.currentDirectoryItem_.entry = newEntry;
        // @ts-ignore: error TS2339: Property 'updateSubDirectories' does not
        // exist on type 'XfTreeItem | DirectoryItem'.
        this.currentDirectoryItem_.updateSubDirectories(/* recursive= */ true);
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

      // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
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

    // @ts-ignore: error TS2531: Object is possibly 'null'.
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

    // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
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
