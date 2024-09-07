// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';
import {getKeyModifiers} from '../../common/js/dom_utils.js';
import {getTreeItemEntry, isSameEntry} from '../../common/js/entry_utils.js';
import type {DirectoryTreeContainer} from '../../containers/directory_tree_container.js';
import {readSubDirectoriesForRenamedEntry} from '../../state/ducks/all_entries.js';
import {getStore} from '../../state/store.js';
import type {XfTree} from '../../widgets/xf_tree.js';
import type {XfTreeItem} from '../../widgets/xf_tree_item.js';

import type {DirectoryModel} from './directory_model.js';
import {renameEntry, validateEntryName} from './file_rename.js';
import type {WithContextMenu} from './ui/context_menu_handler.js';
import type {FilesAlertDialog} from './ui/files_alert_dialog.js';

/**
 * Naming controller for directory tree.
 */
export class DirectoryTreeNamingController {
  private currentDirectoryItem_: XfTreeItem|null = null;
  private editing_ = false;
  /**
   * Whether the entry being renamed is a root of a removable partition/volume.
   */
  private isRemovableRoot_: boolean = false;
  private volumeInfo_: VolumeInfo|null = null;
  private readonly inputElement_: HTMLInputElement&WithContextMenu;

  constructor(
      private readonly directoryModel_: DirectoryModel,
      private readonly directoryTree_: XfTree|null,
      private readonly directoryTreeContainer_: DirectoryTreeContainer|null,
      private readonly alertDialog_: FilesAlertDialog) {
    this.inputElement_ =
        document.createElement('input') as HTMLInputElement & WithContextMenu;
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
   */
  getInputElement(): HTMLInputElement&WithContextMenu {
    return this.inputElement_;
  }

  /**
   * Attaches naming controller to specified directory item and start rename.
   * @param directoryItem An html element of a node of the target.
   * @param isRemovableRoot Indicates whether the target is a removable volume
   *     root or not.
   * @param volumeInfo A volume information about the target entry. |volumeInfo|
   *     can be null if method is invoked on a folder that is in the tree view
   *     and is not root of an external drive.
   */
  attachAndStart(
      directoryItem: XfTreeItem, isRemovableRoot: boolean,
      volumeInfo: VolumeInfo|null) {
    this.isRemovableRoot_ = isRemovableRoot;
    if (this.isRemovableRoot_) {
      assert(volumeInfo);
      this.volumeInfo_ = volumeInfo;
    } else {
      this.volumeInfo_ = null;
    }

    if (this.currentDirectoryItem_) {
      return;
    }

    this.currentDirectoryItem_ = directoryItem;
    this.currentDirectoryItem_.setAttribute('renaming', 'true');

    this.inputElement_.slot = 'rename';
    this.currentDirectoryItem_.appendChild(this.inputElement_);

    this.inputElement_.value = this.currentDirectoryItem_.label;
    this.inputElement_.select();
    this.inputElement_.focus();

    this.editing_ = true;
  }

  /**
   * Commits rename.
   */
  private async commitRename_() {
    const contextMenu = this.inputElement_.contextMenu;
    if (!this.editing_ || (contextMenu && !contextMenu.hidden)) {
      return;
    }
    this.editing_ = false;

    const entry =
        getTreeItemEntry(this.currentDirectoryItem_) as DirectoryEntry;
    assert(entry);
    const newName = this.inputElement_.value;

    // If new name is the same as current name or empty, do nothing.
    if (newName === this.currentDirectoryItem_!.label || newName.length === 0) {
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
      await this.alertDialog_.showAsync((error as Error).message);
      this.editing_ = true;
    }
  }

  /**
   * Performs rename operation.
   * @param newName Validated name.
   */
  private async performRename_(entry: DirectoryEntry, newName: string) {
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
                           entry, newName, this.volumeInfo_,
                           this.isRemovableRoot_) as DirectoryEntry;

      // Put the new name in the .label element before detaching the <input> to
      // prevent showing the old name.
      this.currentDirectoryItem_!.label = newName;

      // We currently don't have promises/callbacks for when removableRoots are
      // successfully renamed, so we can't update their subdirectories or update
      // the current directory to them at this point.
      if (this.isRemovableRoot_) {
        return;
      }

      getStore().dispatch(readSubDirectoriesForRenamedEntry(newEntry));
      this.directoryTreeContainer_?.focusItemWithKeyWhenRendered(
          newEntry.toURL());

      // If renamed directory was current directory, change it to new one.
      if (renamingCurrentDirectory) {
        this.directoryModel_.changeDirectoryEntry(
            newEntry,
            this.directoryModel_.setIgnoringCurrentDirectoryDeletion.bind(
                this.directoryModel_, /* ignore= */ false));
      }
    } catch (error) {
      this.directoryModel_.setIgnoringCurrentDirectoryDeletion(
          /* ignore= */ false);

      this.alertDialog_.show((error as Error).message);
    } finally {
      this.detach_();
    }
  }

  private cancelRename_() {
    if (!this.editing_) {
      return;
    }

    this.editing_ = false;
    this.detach_();
  }

  /**
   * Detaches controller from current directory item.
   */
  private detach_() {
    assert(!!this.currentDirectoryItem_);

    this.inputElement_.remove();

    this.currentDirectoryItem_.removeAttribute('renaming');
    this.currentDirectoryItem_ = null;

    // Restore focus to directory tree.
    this.directoryTree_?.focus();
  }

  /**
   * Handles keydown event.
   */
  private onKeyDown_(event: KeyboardEvent) {
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
