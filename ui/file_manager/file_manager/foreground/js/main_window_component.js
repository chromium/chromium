// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/ash/common/assert.js';

import {DialogType, isFolderDialogType} from '../../common/js/dialog_type.js';
import {getFocusedTreeItem, getKeyModifiers} from '../../common/js/dom_utils.js';
import {isRecentRootType, isSameEntry, isTrashEntry} from '../../common/js/entry_utils.js';
import {isNewDirectoryTreeEnabled} from '../../common/js/flags.js';
import {recordEnum} from '../../common/js/metrics.js';
import {getEntryLabel, str} from '../../common/js/translations.js';
import {TrashEntry} from '../../common/js/trash.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {DirectoryChangeEvent} from '../../externs/directory_change_event.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {changeDirectory} from '../../state/ducks/current_directory.js';
import {getStore} from '../../state/store.js';

import {AppStateController} from './app_state_controller.js';
import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {FileSelectionHandler} from './file_selection.js';
import {NamingController} from './naming_controller.js';
import {TaskController} from './task_controller.js';
import {Command} from './ui/command.js';
import {FileManagerUI} from './ui/file_manager_ui.js';
import {FileTapHandler} from './ui/file_tap_handler.js';
import {ListContainer} from './ui/list_container.js';
import {ListSelectionModel} from './ui/list_selection_model.js';

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
    // @ts-ignore: error TS18047: 'ui.directoryTree' is possibly 'null'.
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
    addIsFocusedMethod();

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
    // @ts-ignore: error TS6133: 'index' is declared but its value is never
    // read.
    this.tapHandler_.handleTouchEvents(event, -1, (e, index, eventType) => {
      if (eventType == FileTapHandler.TapEvent.TAP) {
        // Taps on the checkmark should only toggle select the item.
        // @ts-ignore: error TS2339: Property 'classList' does not exist on type
        // 'EventTarget'.
        if (event.target.classList.contains('detail-checkmark') ||
            // @ts-ignore: error TS2339: Property 'classList' does not exist on
            // type 'EventTarget'.
            event.target.classList.contains('detail-icon')) {
          return false;
        }

        return this.handleOpenDefault_(event);
      }

      return false;
    });
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
        // @ts-ignore: error TS2339: Property 'touchedElement' does not exist on
        // type 'Event'.
        event.touchedElement || event.srcElement);
    const selection = this.selectionHandler_.selection;
    if (!listItem || !listItem.selected || selection.totalCount !== 1) {
      return false;
    }
    const trashEntries = /** @type {!Array<!TrashEntry>} */ (
        selection.entries.filter(isTrashEntry));
    if (trashEntries.length > 0) {
      this.showFailedToOpenTrashItemDialog_(trashEntries);
      return false;
    }
    // If the selection is blocked by DLP restrictions, we don't allow to change
    // directory or the default action.
    if (this.selectionHandler_.isDlpBlocked()) {
      return false;
    }
    const entry = selection.entries[0];
    // @ts-ignore: error TS18048: 'entry' is possibly 'undefined'.
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
      // Files within the trash root should not have default tasks. They should
      // be restored first.
      if (this.directoryModel_.getCurrentRootType() ===
          VolumeManagerCommon.RootType.TRASH) {
        const selection = this.selectionHandler_.selection;
        if (!selection) {
          return true;
        }
        const trashEntries = /** @type {!Array<!TrashEntry>} */ (
            selection.entries.filter(isTrashEntry));
        this.showFailedToOpenTrashItemDialog_(trashEntries);
        return true;
      }
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
   * Show a confirm dialog that shows whether the current selection can't be
   * opened and offer to restore instead.
   * @param {!Array<!TrashEntry>} trashEntries The current selection.
   */
  showFailedToOpenTrashItemDialog_(trashEntries) {
    let msgTitle = str('OPEN_TRASHED_FILE_ERROR_TITLE');
    let msgDesc = str('OPEN_TRASHED_FILE_ERROR_DESC');
    if (trashEntries.length > 1) {
      msgTitle = str('OPEN_TRASHED_FILES_ERROR_TITLE');
      msgDesc = str('OPEN_TRASHED_FILES_ERROR_DESC');
    }
    const restoreCommand = assertInstanceof(
        document.getElementById('restore-from-trash'), Command);
    this.ui_.restoreConfirmDialog.showWithTitle(msgTitle, msgDesc, () => {
      restoreCommand.canExecuteChange(this.ui_.listContainer.currentList);
      restoreCommand.execute(this.ui_.listContainer.currentList);
    });
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
    recordEnum('ToggleFileListType', listType, ListContainer.ListTypesForUMA);
  }

  /**
   * KeyDown event handler for the document.
   * @param {Event} event Key event.
   * @private
   */
  onKeyDown_(event) {
    // @ts-ignore: error TS2339: Property 'keyCode' does not exist on type
    // 'Event'.
    if (event.keyCode === 9) {  // Tab
      this.pressingTab_ = true;
    }

    if (event.srcElement === this.ui_.listContainer.renameInput) {
      // Ignore keydown handler in the rename input box.
      return;
    }

    // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
    switch (getKeyModifiers(event) + event.key) {
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
    // @ts-ignore: error TS2339: Property 'keyCode' does not exist on type
    // 'Event'.
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
    // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
    if (getKeyModifiers(event) + event.key === 'Enter') {
      const focusedItem = getFocusedTreeItem(this.ui_.directoryTree);
      if (!focusedItem) {
        return;
      }
      if (isNewDirectoryTreeEnabled()) {
        focusedItem.selected = true;
      } else {
        // @ts-ignore: error TS2339: Property 'activate' does not exist on type
        // 'XfTreeItem | DirectoryItem'.
        focusedItem.activate();
      }
      if (this.dialogType_ !== DialogType.FULL_PAGE &&
          !focusedItem.hasAttribute('renaming') &&
          isSameEntry(
              // @ts-ignore: error TS2339: Property 'entry' does not exist on
              // type 'XfTreeItem | DirectoryItem'.
              this.directoryModel_.getCurrentDirEntry(), focusedItem.entry) &&
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
    // @ts-ignore: error TS2339: Property 'key' does not exist on type 'Event'.
    switch (getKeyModifiers(event) + event.key) {
      case 'Backspace':  // Backspace => Up one directory.
        event.preventDefault();
        const store = getStore();
        const state = store.getState();
        const components = state.currentDirectory?.pathComponents;
        if (!components || components.length < 2) {
          break;
        }
        const parent = components[components.length - 2];
        // @ts-ignore: error TS18048: 'parent' is possibly 'undefined'.
        store.dispatch(changeDirectory({toKey: parent.key}));
        break;

      case 'Enter':  // Enter => Change directory or perform default action.
                     // If the selection is blocked by DLP restrictions, we
                     // don't allow to
        // change directory or the default action.
        if (this.selectionHandler_.isDlpBlocked()) {
          break;
        }
        const selection = this.selectionHandler_.selection;
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        if (selection.totalCount === 1 && selection.entries[0].isDirectory &&
            !isFolderDialogType(this.dialogType_) &&
            !selection.entries.some(isTrashEntry)) {
          const item = this.ui_.listContainer.currentList.getListItemByIndex(
              // @ts-ignore: error TS2345: Argument of type 'number | undefined'
              // is not assignable to parameter of type 'number'.
              selection.indexes[0]);
          // If the item is in renaming process we don't allow to change
          // directory.
          if (item && !item.hasAttribute('renaming')) {
            event.preventDefault();
            this.directoryModel_.changeDirectoryEntry(
                /** @type {!DirectoryEntry} */ (selection.entries[0]));
          }
          break;
        }
        if (this.acceptSelection_()) {
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
        const selectionModel = /** @type {ListSelectionModel} */ (
            this.ui_.listContainer.currentList.selectionModel);
        selectionModel.selectedIndexes = [index];
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

    // @ts-ignore: error TS2339: Property 'newDirEntry' does not exist on type
    // 'Event'.
    const newVolumeInfo = event.newDirEntry ?
        // @ts-ignore: error TS2339: Property 'newDirEntry' does not exist on
        // type 'Event'.
        this.volumeManager_.getVolumeInfo(event.newDirEntry) :
        null;

    // Update unformatted volume status.
    const unformatted = !!(newVolumeInfo && newVolumeInfo.error);
    this.ui_.element.toggleAttribute('unformatted', /*force=*/ unformatted);

    // @ts-ignore: error TS2339: Property 'newDirEntry' does not exist on type
    // 'Event'.
    if (event.newDirEntry) {
      // Updates UI.
      if (this.dialogType_ === DialogType.FULL_PAGE) {
        const locationInfo =
            // @ts-ignore: error TS2339: Property 'newDirEntry' does not exist
            // on type 'Event'.
            this.volumeManager_.getLocationInfo(event.newDirEntry);
        // @ts-ignore: error TS2339: Property 'newDirEntry' does not exist on
        // type 'Event'.
        const label = getEntryLabel(locationInfo, event.newDirEntry);
        document.title = `${str('FILEMANAGER_APP_NAME')} - ${label}`;
      }
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
    if (isRecentRootType(this.directoryModel_.getCurrentRootType())) {
      this.directoryModel_.rescan(true /* refresh */);
      // Do not start the spinner here to silently refresh the contents.
    }
  }
}

/** Adds an isFocused method to the current window object.  */
const addIsFocusedMethod = () => {
  let focused = true;

  window.addEventListener('focus', () => {
    focused = true;
  });

  window.addEventListener('blur', () => {
    focused = false;
  });

  /**
   * @return {boolean} True if focused.
   */
  // @ts-ignore: error TS2339: Property 'isFocused' does not exist on type
  // 'Window & typeof globalThis'.
  window.isFocused = () => {
    return focused;
  };
};
