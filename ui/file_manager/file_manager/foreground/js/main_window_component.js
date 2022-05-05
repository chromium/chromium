// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {DialogType} from '../../common/js/dialog_type.js';
import {metrics} from '../../common/js/metrics.js';
import {str, util} from '../../common/js/util.js';
import {DirectoryChangeEvent} from '../../externs/directory_change_event.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {AppStateController} from './app_state_controller.js';
import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {FileSelectionHandler} from './file_selection.js';
import {NamingController} from './naming_controller.js';
import {TaskController} from './task_controller.js';
import {FileManagerUI} from './ui/file_manager_ui.js';
import {FileTapHandler} from './ui/file_tap_handler.js';
import {ListContainer} from './ui/list_container.js';

/**
 * Component for the main window.
 *
 * The class receives UI events from UI components that does not have their own
 * controller, and do corresponding action by using models/other controllers.
 *
 * The class also observes model/browser API's event to update the misc
 * components.
 */
export class MainWindowComponent {
  /**
   * @param {DialogType} dialogType
   * @param {!FileManagerUI} ui
   * @param {!VolumeManager} volumeManager
   * @param {!DirectoryModel} directoryModel
   * @param {!FileFilter} fileFilter
   * @param {!FileSelectionHandler} selectionHandler
   * @param {!NamingController} namingController
   * @param {!AppStateController} appStateController
   * @param {!TaskController} taskController
   */
  constructor(
      dialogType, ui, volumeManager, directoryModel, fileFilter,
      selectionHandler, namingController, appStateController, taskController) {
    /**
     * @type {DialogType}
     * @const
     * @private
     */
    this.dialogType_ = dialogType;

    /**
     * @type {!FileManagerUI}
     * @const
     * @private
     */
    this.ui_ = ui;

    /**
     * @type {!VolumeManager}
     * @const
     * @private
     */
    this.volumeManager_ = volumeManager;

    /**
     * @type {!DirectoryModel}
     * @const
     * @private
     */
    this.directoryModel_ = directoryModel;

    /**
     * @type {!FileFilter}
     * @const
     * @private
     */
    this.fileFilter_ = fileFilter;

    /**
     * @type {!FileSelectionHandler}
     * @const
     * @private
     */
    this.selectionHandler_ = selectionHandler;

    /**
     * @type {!NamingController}
     * @const
     * @private
     */
    this.namingController_ = namingController;

    /**
     * @type {!AppStateController}
     * @const
     * @private
     */
    this.appStateController_ = appStateController;

    /**
     * @type {!TaskController}
     * @const
     * @private
     */
    this.taskController_ = taskController;

    /**
     * True while a user is pressing <Tab>.
     * This is used for identifying the trigger causing the filelist to
     * be focused.
     * @type {boolean}
     * @private
     */
    this.pressingTab_ = false;

    // Register events.
    ui.listContainer.element.addEventListener(
        'keydown', this.onListKeyDown_.bind(this));
    ui.directoryTree.addEventListener(
        'keydown', this.onDirectoryTreeKeyDown_.bind(this));
    ui.listContainer.element.addEventListener(
        ListContainer.EventType.TEXT_SEARCH, this.onTextSearch_.bind(this));
    ui.listContainer.table.list.addEventListener(
        'dblclick', this.onDoubleClick_.bind(this));
    ui.listContainer.grid.addEventListener(
        'dblclick', this.onDoubleClick_.bind(this));
    ui.listContainer.table.list.addEventListener(
        'touchstart', this.handleTouchEvents_.bind(this));
    ui.listContainer.grid.addEventListener(
        'touchstart', this.handleTouchEvents_.bind(this));
    ui.listContainer.table.list.addEventListener(
        'touchend', this.handleTouchEvents_.bind(this));
    ui.listContainer.grid.addEventListener(
        'touchend', this.handleTouchEvents_.bind(this));
    ui.listContainer.table.list.addEventListener(
        'touchmove', this.handleTouchEvents_.bind(this));
    ui.listContainer.grid.addEventListener(
        'touchmove', this.handleTouchEvents_.bind(this));
    ui.listContainer.table.list.addEventListener(
        'focus', this.onFileListFocus_.bind(this));
    ui.listContainer.grid.addEventListener(
        'focus', this.onFileListFocus_.bind(this));
    ui.breadcrumbController.addEventListener(
        'pathclick', this.onBreadcrumbClick_.bind(this));
    /**
     * We are binding both click/keyup event here because "click" event will
     * be triggered multiple times if the Enter/Space key is being pressed
     * without releasing (because the focus is always on the button).
     */
    ui.toggleViewButton.addEventListener(
        'click', this.onToggleViewButtonClick_.bind(this));
    ui.toggleViewButton.addEventListener(
        'keyup', this.onToggleViewButtonClick_.bind(this));
    directoryModel.addEventListener(
        'directory-changed', this.onDirectoryChanged_.bind(this));
    volumeManager.addEventListener(
        'drive-connection-changed', this.onDriveConnectionChanged_.bind(this));
    this.onDriveConnectionChanged_();
    document.addEventListener('keydown', this.onKeyDown_.bind(this));
    document.addEventListener('keyup', this.onKeyUp_.bind(this));
    window.addEventListener('focus', this.onWindowFocus_.bind(this));

    /**
     * @type {!FileTapHandler}
     * @private
     * @const
     */
    this.tapHandler_ = new FileTapHandler();
  }

  /**
   * Handles touch events.
   * @param {!Event} event
   * @private
   */
  handleTouchEvents_(event) {
    // We only need to know that a tap happens somewhere in the list.
    // Also the 2nd parameter of handleTouchEvents is just passed back to the
    // callback. Therefore we can pass a dummy value -1.
    this.tapHandler_.handleTouchEvents(event, -1, (e, index, eventType) => {
      if (eventType == FileTapHandler.TapEvent.TAP) {
        // Taps on the checkmark should only toggle select the item.
        if (event.target.classList.contains('detail-checkmark') ||
            event.target.classList.contains('detail-icon')) {
          return false;
        }

        return this.handleOpenDefault_(event);
      }

      return false;
    });
  }

  /**
   * @param {Event} event Click event.
   * @private
   */
  onBreadcrumbClick_(event) {
    this.directoryModel_.changeDirectoryEntry(event.entry);
  }

  /**
   * File list focus handler. Used to select the top most element on the list
   * if nothing was selected.
   *
   * @private
   */
  onFileListFocus_() {
    // If the file list is focused by <Tab>, select the first item if no item
    // is selected.
    if (this.pressingTab_) {
      const selection = this.selectionHandler_.selection;
      if (selection && selection.totalCount === 0) {
        const selectionModel = this.directoryModel_.getFileListSelection();
        const targetIndex =
            selectionModel.anchorIndex && selectionModel.anchorIndex !== -1 ?
            selectionModel.anchorIndex :
            0;
        this.directoryModel_.selectIndex(targetIndex);
      }
    }
  }

  /**
   * Handles a double click event.
   *
   * @param {Event} event The dblclick event.
   * @private
   */
  onDoubleClick_(event) {
    this.handleOpenDefault_(event);
  }

  /**
   * Opens the selected item by the default command.
   * If the item is a directory, change current directory to it.
   * Otherwise, accepts the current selection.
   *
   * @param {Event} event The dblclick event.
   * @return {boolean} true if successfully opened the item.
   * @private
   */
  handleOpenDefault_(event) {
    if (this.namingController_.isRenamingInProgress()) {
      // Don't pay attention to clicks or taps during a rename.
      return false;
    }

    // It is expected that the target item should have already been selected
    // by previous touch or mouse event processing.
    const listItem = this.ui_.listContainer.findListItemForNode(
        event.touchedElement || event.srcElement);
    const selection = this.selectionHandler_.selection;
    if (!listItem || !listItem.selected || selection.totalCount !== 1) {
      return false;
    }

    const entry = selection.entries[0];
    if (entry.isDirectory) {
      this.directoryModel_.changeDirectoryEntry(
          /** @type {!DirectoryEntry} */ (entry));
      return false;
    }

    return this.acceptSelection_();
  }

  /**
   * Accepts the current selection depending on the files app dialog mode.
   * @return {boolean} true if successfully accepted the current selection.
   * @private
   */
  acceptSelection_() {
    if (this.dialogType_ === DialogType.FULL_PAGE) {
      this.taskController_.getFileTasks()
          .then(tasks => {
            tasks.executeDefault();
          })
          .catch(error => {
            if (error) {
              console.warn(error.stack || error);
            }
          });
      return true;
    }

    if (!this.ui_.dialogFooter.okButton.disabled) {
      this.ui_.dialogFooter.okButton.click();
      return true;
    }

    return false;
  }

  /**
   * Handles click/keyup event on the toggle-view button.
   * @param {Event} event Click or keyup event.
   * @private
   */
  onToggleViewButtonClick_(event) {
    /**
     * This callback can be triggered by both mouse click and Enter/Space key,
     * so we explicitly check if the "click" event is triggered by keyboard
     * or not, if so, do nothing because this callback will be triggered
     * again by "keyup" event when users release the Enter/Space key.
     */
    if (event.type === 'click') {
      const pointerEvent = /** @type {PointerEvent} */ (event);
      if (pointerEvent.detail === 0) {  // Click is triggered by keyboard.
        return;
      }
    }
    if (event.type === 'keyup') {
      const keyboardEvent = /** @type {KeyboardEvent} */ (event);
      if (keyboardEvent.code !== 'Space' && keyboardEvent.code !== 'Enter') {
        return;
      }
    }
    const listType = this.ui_.listContainer.currentListType ===
            ListContainer.ListType.DETAIL ?
        ListContainer.ListType.THUMBNAIL :
        ListContainer.ListType.DETAIL;
    this.ui_.setCurrentListType(listType);
    const msgId = listType === ListContainer.ListType.DETAIL ?
        'FILE_LIST_CHANGED_TO_LIST_VIEW' :
        'FILE_LIST_CHANGED_TO_LIST_THUMBNAIL_VIEW';
    this.ui_.speakA11yMessage(str(msgId));
    this.appStateController_.saveViewOptions();

    // The aria-label of toggleViewButton has been updated, we need to
    // explicitly show the tooltip.
    this.ui_.filesTooltip.updateTooltipText(
        /** @type {!HTMLElement} */ (this.ui_.toggleViewButton));
    metrics.recordEnum(
        'ToggleFileListType', listType, ListContainer.ListTypesForUMA);
  }

  /**
   * KeyDown event handler for the document.
   * @param {Event} event Key event.
   * @private
   */
  onKeyDown_(event) {
    if (event.keyCode === 9) {  // Tab
      this.pressingTab_ = true;
    }

    if (event.srcElement === this.ui_.listContainer.renameInput) {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (util.getKeyModifiers(event) + event.key) {
      case 'Escape':  // Escape => Cancel dialog.
      case 'Ctrl-w':  // Ctrl+W => Cancel dialog.
        if (this.dialogType_ != DialogType.FULL_PAGE) {
          // If there is nothing else for ESC to do, then cancel the dialog.
          event.preventDefault();
          this.ui_.dialogFooter.cancelButton.click();
        }
        break;
    }
  }

  /**
   * KeyUp event handler for the document.
   * @param {Event} event Key event.
   * @private
   */
  onKeyUp_(event) {
    if (event.keyCode === 9) {  // Tab
      this.pressingTab_ = false;
    }
  }

  /**
   * KeyDown event handler for the directory tree element.
   * @param {Event} event Key event.
   * @private
   */
  onDirectoryTreeKeyDown_(event) {
    // Enter => Change directory or perform default action.
    if (util.getKeyModifiers(event) + event.key === 'Enter') {
      const selectedItem = this.ui_.directoryTree.selectedItem;
      if (!selectedItem) {
        return;
      }
      selectedItem.activate();
      if (this.dialogType_ !== DialogType.FULL_PAGE &&
          !selectedItem.hasAttribute('renaming') &&
          util.isSameEntry(
              this.directoryModel_.getCurrentDirEntry(), selectedItem.entry) &&
          !this.ui_.dialogFooter.okButton.disabled) {
        this.ui_.dialogFooter.okButton.click();
      }
    }
  }

  /**
   * KeyDown event handler for the div#list-container element.
   * @param {Event} event Key event.
   * @private
   */
  onListKeyDown_(event) {
    switch (util.getKeyModifiers(event) + event.key) {
      case 'Backspace':  // Backspace => Up one directory.
        event.preventDefault();
        const components =
            this.ui_.breadcrumbController.getCurrentPathComponents();
        if (components.length < 2) {
          break;
        }
        const parentPathComponent = components[components.length - 2];
        parentPathComponent.resolveEntry().then((parentEntry) => {
          this.directoryModel_.changeDirectoryEntry(
              /** @type {!DirectoryEntry} */ (parentEntry));
        });
        break;

      case 'Enter':  // Enter => Change directory or perform default action.
        const selection = this.selectionHandler_.selection;
        if (selection.totalCount === 1 && selection.entries[0].isDirectory &&
            !DialogType.isFolderDialog(this.dialogType_)) {
          const item = this.ui_.listContainer.currentList.getListItemByIndex(
              selection.indexes[0]);
          // If the item is in renaming process, we don't allow to change
          // directory.
          if (item && !item.hasAttribute('renaming')) {
            event.preventDefault();
            this.directoryModel_.changeDirectoryEntry(
                /** @type {!DirectoryEntry} */ (selection.entries[0]));
          }
        } else if (this.acceptSelection_()) {
          event.preventDefault();
        }
        break;
    }
  }

  /**
   * Performs a 'text search' - selects a first list entry with name
   * starting with entered text (case-insensitive).
   * @private
   */
  onTextSearch_() {
    const text = this.ui_.listContainer.textSearchState.text;
    const dm = this.directoryModel_.getFileList();
    for (let index = 0; index < dm.length; ++index) {
      const name = dm.item(index).name;
      if (name.substring(0, text.length).toLowerCase() == text) {
        this.ui_.listContainer.currentList.selectionModel.selectedIndexes =
            [index];
        return;
      }
    }

    this.ui_.listContainer.textSearchState.text = '';
  }

  /**
   * Update the UI when the current directory changes.
   *
   * @param {Event} event The directory-changed event.
   * @private
   */
  onDirectoryChanged_(event) {
    event = /** @type {DirectoryChangeEvent} */ (event);

    const newVolumeInfo = event.newDirEntry ?
        this.volumeManager_.getVolumeInfo(event.newDirEntry) :
        null;

    // Update unformatted volume status.
    const unformatted = !!(newVolumeInfo && newVolumeInfo.error);
    this.ui_.element.toggleAttribute('unformatted', /*force=*/ unformatted);

    if (event.newDirEntry) {
      this.ui_.breadcrumbController.show(event.newDirEntry);
      // Updates UI.
      if (this.dialogType_ === DialogType.FULL_PAGE) {
        const locationInfo =
            this.volumeManager_.getLocationInfo(event.newDirEntry);
        if (locationInfo) {
          const label = util.getEntryLabel(locationInfo, event.newDirEntry);
          document.title = `${str('FILEMANAGER_APP_NAME')} - ${label}`;
        } else {
          console.warn(
              'Could not find location info for entry: ' +
              event.newDirEntry.fullPath);
        }
      }
    } else {
      this.ui_.breadcrumbController.hide();
    }
  }

  /**
   * @private
   */
  onDriveConnectionChanged_() {
    const connection = this.volumeManager_.getDriveConnectionState();
    this.ui_.dialogContainer.setAttribute('connection', connection.type);
  }

  /**
   * @private
   */
  onWindowFocus_() {
    // When the window have got a focus while the current directory is Recent
    // root, refresh the contents.
    if (util.isRecentRootType(this.directoryModel_.getCurrentRootType())) {
      this.directoryModel_.rescan(true /* refresh */);
      // Do not start the spinner here to silently refresh the contents.
    }
  }
}
