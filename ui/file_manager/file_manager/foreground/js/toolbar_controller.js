// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';

import {queryRequiredElement} from '../../common/js/dom_utils.js';
import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {DirectoryModel} from './directory_model.js';
import {FileSelectionHandler} from './file_selection.js';
import {A11yAnnounce} from './ui/a11y_announce.js';
import {Command} from './ui/command.js';
import {FileListSelectionModel} from './ui/file_list_selection_model.js';
import {ListContainer} from './ui/list_container.js';

/**
 * This class controls wires toolbar UI and selection model. When selection
 * status is changed, this class changes the view of toolbar. If cancel
 * selection button is pressed, this class clears the selection.
 */
export class ToolbarController {
  /**
   * @param {!HTMLElement} toolbar Toolbar element which contains controls.
   * @param {!HTMLElement} navigationList Navigation list on the left pane. The
   *     position of silesSelectedLabel depends on the navitaion list's width.
   * @param {!ListContainer} listContainer List container.
   * @param {!FileSelectionHandler} selectionHandler
   * @param {!DirectoryModel} directoryModel
   * @param {!VolumeManager} volumeManager
   * @param {!FileOperationManager} fileOperationManager
   * @param {!A11yAnnounce} a11y
   */
  constructor(
      toolbar, navigationList, listContainer, selectionHandler, directoryModel,
      volumeManager, fileOperationManager, a11y) {
    /**
     * @private {!HTMLElement}
     * @const
     */
    this.toolbar_ = toolbar;

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.cancelSelectionButton_ =
        queryRequiredElement('#cancel-selection-button', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.cancelSelectionButtonWrapper_ =
        queryRequiredElement('#cancel-selection-button-wrapper', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.filesSelectedLabel_ =
        queryRequiredElement('#files-selected-label', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.deleteButton_ = queryRequiredElement('#delete-button', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.moveToTrashButton_ =
        queryRequiredElement('#move-to-trash-button', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.restoreFromTrashButton_ =
        queryRequiredElement('#restore-from-trash-button', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.sharesheetButton_ =
        queryRequiredElement('#sharesheet-button', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.readOnlyIndicator_ =
        queryRequiredElement('#read-only-indicator', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.pinnedToggleWrapper_ =
        queryRequiredElement('#pinned-toggle-wrapper', this.toolbar_);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.pinnedToggle_ = queryRequiredElement('#pinned-toggle', this.toolbar_);

    /**
     * @private {!Command}
     * @const
     */
    this.deleteCommand_ = assertInstanceof(
        queryRequiredElement(
            '#delete', assert(this.toolbar_.ownerDocument.body)),
        Command);

    /**
     * @type {!Command}
     * @const
     */
    this.moveToTrashCommand = assertInstanceof(
        queryRequiredElement(
            '#move-to-trash', assert(this.toolbar_.ownerDocument.body)),
        Command);

    /**
     * @private {!Command}
     * @const
     */
    this.restoreFromTrashCommand_ = assertInstanceof(
        queryRequiredElement(
            '#restore-from-trash', assert(this.toolbar_.ownerDocument.body)),
        Command);

    /**
     * @private {!Command}
     * @const
     */
    this.emptyTrashCommand_ = assertInstanceof(
        queryRequiredElement(
            '#empty-trash', assert(this.toolbar_.ownerDocument.body)),
        Command);

    /**
     * @private {!Command}
     * @const
     */
    this.refreshCommand_ = assertInstanceof(
        queryRequiredElement(
            '#refresh', assert(this.toolbar_.ownerDocument.body)),
        Command);

    /**
     * @private {!Command}
     * @const
     */
    this.newFolderCommand_ = assertInstanceof(
        queryRequiredElement(
            '#new-folder', assert(this.toolbar_.ownerDocument.body)),
        Command);

    /**
     * @private {!Command}
     * @const
     */
    this.invokeSharesheetCommand_ = assertInstanceof(
        queryRequiredElement(
            '#invoke-sharesheet', assert(this.toolbar_.ownerDocument.body)),
        Command);

    /**
     * @private {!Command}
     * @const
     */
    this.togglePinnedCommand_ = assertInstanceof(
        queryRequiredElement(
            '#toggle-pinned', assert(this.toolbar_.ownerDocument.body)),
        Command);

    /**
     * @private {!HTMLElement}
     * @const
     */
    this.navigationList_ = navigationList;

    /**
     * @private {!ListContainer}
     * @const
     */
    this.listContainer_ = listContainer;

    /**
     * @private {!FileSelectionHandler}
     * @const
     */
    this.selectionHandler_ = selectionHandler;

    /**
     * @private {!DirectoryModel}
     * @const
     */
    this.directoryModel_ = directoryModel;

    /**
     * @private {!VolumeManager}
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private {!FileOperationManager}
     * @const
     */
    this.fileOperationManager_ = fileOperationManager;

    /**
     * @private {!A11yAnnounce}
     * @const
     */
    this.a11y_ = a11y;

    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE,
        this.onSelectionChanged_.bind(this));

    // Using CHANGE_THROTTLED because updateSharesheetCommand_() uses async
    // API and can update the state out-of-order specially when updating to
    // an empty selection.
    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE_THROTTLED,
        this.updateSharesheetCommand_.bind(this));

    chrome.fileManagerPrivate.onAppsUpdated.addListener(
        this.updateSharesheetCommand_.bind(this));

    this.cancelSelectionButton_.addEventListener(
        'click', this.onCancelSelectionButtonClicked_.bind(this));

    this.deleteButton_.addEventListener(
        'click', this.onDeleteButtonClicked_.bind(this));

    this.moveToTrashButton_.addEventListener(
        'click', this.onMoveToTrashButtonClicked_.bind(this));

    this.restoreFromTrashButton_.addEventListener(
        'click', this.onRestoreFromTrashButtonClicked_.bind(this));

    this.sharesheetButton_.addEventListener(
        'click', this.onSharesheetButtonClicked_.bind(this));

    this.togglePinnedCommand_.addEventListener(
        'checkedChange', this.updatePinnedToggle_.bind(this));

    this.moveToTrashCommand.addEventListener(
        'hiddenChange', this.updateMoveToTrashCommand_.bind(this));

    this.moveToTrashCommand.addEventListener(
        'disabledChange', this.updateMoveToTrashCommand_.bind(this));

    this.togglePinnedCommand_.addEventListener(
        'disabledChange', this.updatePinnedToggle_.bind(this));

    this.togglePinnedCommand_.addEventListener(
        'hiddenChange', this.updatePinnedToggle_.bind(this));

    this.pinnedToggle_.addEventListener(
        'change', this.onPinnedToggleChanged_.bind(this));

    this.directoryModel_.addEventListener(
        'directory-changed', this.updateCurrentDirectoryButtons_.bind(this));
  }

  /**
   * Updates toolbar's UI elements which are related to current directory.
   * @private
   */
  updateCurrentDirectoryButtons_() {
    this.updateRefreshCommand_();

    this.newFolderCommand_.canExecuteChange(this.listContainer_.currentList);

    const currentDirectory = this.directoryModel_.getCurrentDirEntry();
    const locationInfo = currentDirectory &&
        this.volumeManager_.getLocationInfo(currentDirectory);
    // Normally, isReadOnly can be used to show the label. This property
    // is always true for fake volumes (eg. Google Drive root). However, "Linux
    // files" and GuestOS volumes are fake volume on first access until the VM
    // is loaded and the mount point is initialised. The volume is technically
    // read-only since the temporary fake volume can (and should) not be
    // written to. However, showing the read only label is not appropriate since
    // the volume will become read-write once all loading has completed.
    this.readOnlyIndicator_.hidden =
        !(locationInfo && locationInfo.isReadOnly &&
          locationInfo.rootType !== VolumeManagerCommon.RootType.CROSTINI &&
          locationInfo.rootType !== VolumeManagerCommon.RootType.GUEST_OS);
  }

  /** @private */
  updateRefreshCommand_() {
    this.refreshCommand_.canExecuteChange(this.listContainer_.currentList);
  }

  /**
   * Handles selection's change event to update the UI.
   * @private
   */
  onSelectionChanged_() {
    const selection = this.selectionHandler_.selection;
    this.updateRefreshCommand_();

    // Update the label "x files selected." on the header.
    let text;
    if (selection.totalCount === 0) {
      text = '';
    } else if (selection.totalCount === 1) {
      if (selection.directoryCount == 0) {
        text = str('ONE_FILE_SELECTED');
      } else if (selection.fileCount == 0) {
        text = str('ONE_DIRECTORY_SELECTED');
      }
    } else {
      if (selection.directoryCount == 0) {
        text = strf('MANY_FILES_SELECTED', selection.fileCount);
      } else if (selection.fileCount == 0) {
        text = strf('MANY_DIRECTORIES_SELECTED', selection.directoryCount);
      } else {
        text = strf('MANY_ENTRIES_SELECTED', selection.totalCount);
      }
    }
    this.filesSelectedLabel_.textContent = text;

    // Update visibility of the delete and move to trash buttons.
    this.deleteButton_.hidden =
        (selection.totalCount === 0 ||
         !this.directoryModel_.canDeleteEntries() ||
         selection.hasReadOnlyEntry() ||
         selection.entries.some(
             entry => util.isNonModifiable(this.volumeManager_, entry)));
    // Show 'Move to Trash' rather than 'Delete' if possible. The
    // `moveToTrashCommand` needs to be set to hidden to ensure the
    // `canExecuteChange` invokes the `hiddenChange` event in the case where
    // Trash should be shown.
    this.moveToTrashButton_.hidden = true;
    this.moveToTrashCommand.disabled = true;
    if (!this.deleteButton_.hidden && util.isTrashEnabled()) {
      this.moveToTrashCommand.canExecuteChange(this.listContainer_.currentList);
    }

    // Update visibility of the restore-from-trash button.
    this.restoreFromTrashButton_.hidden = (selection.totalCount == 0) ||
        this.directoryModel_.getCurrentRootType() !==
            VolumeManagerCommon.RootType.TRASH;

    this.togglePinnedCommand_.canExecuteChange(this.listContainer_.currentList);

    // Set .selecting class to containing element to change the view
    // accordingly.
    // TODO(fukino): This code changes the state of body, not the toolbar, to
    // update the checkmark visibility on grid view. This should be moved to a
    // controller which controls whole app window. Or, both toolbar and FileGrid
    // should listen to the FileSelectionHandler.
    if (this.directoryModel_.getFileListSelection().multiple) {
      const bodyClassList =
          this.filesSelectedLabel_.ownerDocument.body.classList;
      bodyClassList.toggle('selecting', selection.totalCount > 0);
      if (bodyClassList.contains('check-select') !=
          /** @type {!FileListSelectionModel} */
          (this.directoryModel_.getFileListSelection()).getCheckSelectMode()) {
        bodyClassList.toggle('check-select');
      }
    }
  }

  /**
   * Handles click event for cancel button to change the selection state.
   * @private
   */
  onCancelSelectionButtonClicked_() {
    this.directoryModel_.selectEntries([]);
    this.a11y_.speakA11yMessage(str('SELECTION_CANCELLATION'));
  }

  /**
   * Handles click event for delete button to execute the delete command.
   * @private
   */
  onDeleteButtonClicked_() {
    this.deleteCommand_.canExecuteChange(this.listContainer_.currentList);
    this.deleteCommand_.execute(this.listContainer_.currentList);
  }

  /**
   * Handles click event for move to trash button to execute the move to trash
   * command.
   * @private
   */
  onMoveToTrashButtonClicked_() {
    this.moveToTrashCommand.canExecuteChange(this.listContainer_.currentList);
    this.moveToTrashCommand.execute(this.listContainer_.currentList);
  }

  /**
   * Handles click event for restore from trash button to execute the restore
   * command.
   * @private
   */
  onRestoreFromTrashButtonClicked_() {
    this.restoreFromTrashCommand_.canExecuteChange(
        this.listContainer_.currentList);
    this.restoreFromTrashCommand_.execute(this.listContainer_.currentList);
  }

  /**
   * Handles click event for sharesheet button to set button background color.
   * @private
   */
  onSharesheetButtonClicked_() {
    this.sharesheetButton_.setAttribute('menu-shown', '');
    this.toolbar_.ownerDocument.body.addEventListener('focusin', (e) => {
      this.sharesheetButton_.removeAttribute('menu-shown');
    }, {once: true});
  }

  /** @private */
  updateSharesheetCommand_() {
    this.invokeSharesheetCommand_.canExecuteChange(
        this.listContainer_.currentList);
  }

  /** @private */
  updatePinnedToggle_() {
    this.pinnedToggleWrapper_.hidden = this.togglePinnedCommand_.hidden;
    this.pinnedToggle_.checked = this.togglePinnedCommand_.checked;
    this.pinnedToggle_.disabled = this.togglePinnedCommand_.disabled;
  }

  /** @private */
  onPinnedToggleChanged_() {
    this.togglePinnedCommand_.execute(this.listContainer_.currentList);

    // Optimistally update the command's properties so we get notified if they
    // change back.
    this.togglePinnedCommand_.checked = this.pinnedToggle_.checked;
  }

  /** @private */
  updateMoveToTrashCommand_() {
    if (!this.deleteButton_.hidden) {
      this.deleteButton_.hidden = !this.moveToTrashCommand.disabled;
      this.moveToTrashButton_.hidden = this.moveToTrashCommand.disabled;
    }
  }
}
