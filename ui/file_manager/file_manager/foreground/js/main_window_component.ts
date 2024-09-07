// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertInstanceof} from 'chrome://resources/js/assert.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {isFolderDialogType} from '../../common/js/dialog_type.js';
import {getFocusedTreeItem, getKeyModifiers} from '../../common/js/dom_utils.js';
import {isDirectoryEntry, isRecentRootType, isSameEntry, isTrashEntry} from '../../common/js/entry_utils.js';
import {recordEnum} from '../../common/js/metrics.js';
import {str} from '../../common/js/translations.js';
import type {TrashEntry} from '../../common/js/trash.js';
import {RootType} from '../../common/js/volume_manager_types.js';
import {changeDirectory} from '../../state/ducks/current_directory.js';
import {DialogType} from '../../state/state.js';
import {getStore} from '../../state/store.js';

import type {AppStateController} from './app_state_controller.js';
import type {DirectoryChangeEvent, DirectoryModel} from './directory_model.js';
import type {FileSelectionHandler} from './file_selection.js';
import type {NamingController} from './naming_controller.js';
import type {TaskController} from './task_controller.js';
import {Command} from './ui/command.js';
import type {FileManagerUI} from './ui/file_manager_ui.js';
import {FileTapHandler, TapEvent} from './ui/file_tap_handler.js';
import {EventType, ListType, ListTypesForUMA} from './ui/list_container.js';

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
   * True while a user is pressing <Tab>.
   * This is used for identifying the trigger causing the filelist to
   * be focused.
   */
  private pressingTab_ = false;

  private tapHandler_: FileTapHandler = new FileTapHandler();

  constructor(
      private dialogType_: DialogType, private ui_: FileManagerUI,
      private volumeManager_: VolumeManager,
      private directoryModel_: DirectoryModel,
      private selectionHandler_: FileSelectionHandler,
      private namingController_: NamingController,
      private appStateController_: AppStateController,
      private taskController_: TaskController) {
    // Register events.
    this.ui_.listContainer.element.addEventListener(
        'keydown', this.onListKeyDown_.bind(this));
    this.ui_.directoryTree?.addEventListener(
        'keydown', this.onDirectoryTreeKeyDown_.bind(this));
    this.ui_.listContainer.element.addEventListener(
        EventType.TEXT_SEARCH, this.onTextSearch_.bind(this));
    this.ui_.listContainer.table.list.addEventListener(
        'dblclick', this.onDoubleClick_.bind(this));
    this.ui_.listContainer.grid.addEventListener(
        'dblclick', this.onDoubleClick_.bind(this));
    this.ui_.listContainer.table.list.addEventListener(
        'touchstart', this.handleTouchEvents_.bind(this));
    this.ui_.listContainer.grid.addEventListener(
        'touchstart', this.handleTouchEvents_.bind(this));
    this.ui_.listContainer.table.list.addEventListener(
        'touchend', this.handleTouchEvents_.bind(this));
    this.ui_.listContainer.grid.addEventListener(
        'touchend', this.handleTouchEvents_.bind(this));
    this.ui_.listContainer.table.list.addEventListener(
        'touchmove', this.handleTouchEvents_.bind(this));
    this.ui_.listContainer.grid.addEventListener(
        'touchmove', this.handleTouchEvents_.bind(this));
    this.ui_.listContainer.table.list.addEventListener(
        'focus', this.onFileListFocus_.bind(this));
    this.ui_.listContainer.grid.addEventListener(
        'focus', this.onFileListFocus_.bind(this));
    /**
     * We are binding both click/keyup event here because "click" event will
     * be triggered multiple times if the Enter/Space key is being pressed
     * without releasing (because the focus is always on the button).
     */
    this.ui_.toggleViewButton.addEventListener(
        'click', this.onToggleViewButtonClick_.bind(this));
    this.ui_.toggleViewButton.addEventListener(
        'keyup', this.onToggleViewButtonClick_.bind(this));
    this.directoryModel_.addEventListener(
        'directory-changed',
        this.onDirectoryChanged_.bind(this) as
            EventListenerOrEventListenerObject);
    this.volumeManager_.addEventListener(
        'drive-connection-changed', this.onDriveConnectionChanged_.bind(this));
    this.onDriveConnectionChanged_();
    document.addEventListener('keydown', this.onKeyDown_.bind(this));
    document.addEventListener('keyup', this.onKeyUp_.bind(this));
    window.addEventListener('focus', this.onWindowFocus_.bind(this));
    addIsFocusedMethod();
  }

  /**
   * Handles touch events.
   */
  private handleTouchEvents_(event: TouchEvent) {
    // We only need to know that a tap happens somewhere in the list.
    // Also the 2nd parameter of handleTouchEvents is just passed back to the
    // callback. Therefore we can pass a dummy value -1.
    this.tapHandler_.handleTouchEvents(event, -1, (_e, _index, eventType) => {
      if (eventType === TapEvent.TAP) {
        const target = event.target as HTMLElement;
        // Taps on the checkmark should only toggle select the item.
        if (target.classList.contains('detail-checkmark') ||
            target.classList.contains('detail-icon')) {
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
   */
  private onFileListFocus_() {
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
   * @param event The dblclick event.
   */
  private onDoubleClick_(event: MouseEvent) {
    this.handleOpenDefault_(event);
  }

  /**
   * Opens the selected item by the default command.
   * If the item is a directory, change current directory to it.
   * Otherwise, accepts the current selection.
   *
   * @param event The dblclick event.
   * @return true if successfully opened the item.
   */
  private handleOpenDefault_(event: MouseEvent|TouchEvent): boolean {
    if (this.namingController_.isRenamingInProgress()) {
      // Don't pay attention to clicks or taps during a rename.
      return false;
    }

    // It is expected that the target item should have already been selected
    // by previous touch or mouse event processing.
    const node = 'touchedElement' in event ?
        event.touchedElement as HTMLElement :
        event.srcElement as HTMLElement;
    const listItem = this.ui_.listContainer.findListItemForNode(node);
    const selection = this.selectionHandler_.selection;
    if (!listItem || !listItem.selected || selection.totalCount !== 1) {
      return false;
    }
    const trashEntries = selection.entries.filter(isTrashEntry);
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
    if (entry && isDirectoryEntry(entry)) {
      this.directoryModel_.changeDirectoryEntry(entry);
      return false;
    }

    return this.acceptSelection_();
  }

  /**
   * Accepts the current selection depending on the files app dialog mode.
   * @return true if successfully accepted the current selection.
   */
  private acceptSelection_(): boolean {
    if (this.dialogType_ === DialogType.FULL_PAGE) {
      // Files within the trash root should not have default tasks. They should
      // be restored first.
      if (this.directoryModel_.getCurrentRootType() === RootType.TRASH) {
        const selection = this.selectionHandler_.selection;
        if (!selection) {
          return true;
        }
        const trashEntries = selection.entries.filter(isTrashEntry);
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
   * @param trashEntries The current selection.
   */
  private showFailedToOpenTrashItemDialog_(trashEntries: TrashEntry[]) {
    let msgTitle = str('OPEN_TRASHED_FILE_ERROR_TITLE');
    let msgDesc = str('OPEN_TRASHED_FILE_ERROR_DESC');
    if (trashEntries.length > 1) {
      msgTitle = str('OPEN_TRASHED_FILES_ERROR_TITLE');
      msgDesc = str('OPEN_TRASHED_FILES_ERROR_DESC');
    }
    const restoreCommand = document.getElementById('restore-from-trash');
    assertInstanceof(restoreCommand, Command);
    this.ui_.restoreConfirmDialog.showWithTitle(msgTitle, msgDesc, () => {
      restoreCommand.canExecuteChange(this.ui_.listContainer.currentList);
      restoreCommand.execute(this.ui_.listContainer.currentList);
    });
  }

  /**
   * Handles click/keyup event on the toggle-view button.
   * @param event Click or keyup event.
   */
  private onToggleViewButtonClick_(event: Event) {
    /**
     * This callback can be triggered by both mouse click and Enter/Space key,
     * so we explicitly check if the "click" event is triggered by keyboard
     * or not, if so, do nothing because this callback will be triggered
     * again by "keyup" event when users release the Enter/Space key.
     */
    if (event.type === 'click') {
      const pointerEvent = event as PointerEvent;
      if (pointerEvent.detail === 0) {  // Click is triggered by keyboard.
        return;
      }
    }
    if (event.type === 'keyup') {
      const keyboardEvent = event as KeyboardEvent;
      if (keyboardEvent.code !== 'Space' && keyboardEvent.code !== 'Enter') {
        return;
      }
    }
    const listType =
        this.ui_.listContainer.currentListType === ListType.DETAIL ?
        ListType.THUMBNAIL :
        ListType.DETAIL;
    this.ui_.setCurrentListType(listType);
    const msgId = listType === ListType.DETAIL ?
        'FILE_LIST_CHANGED_TO_LIST_VIEW' :
        'FILE_LIST_CHANGED_TO_LIST_THUMBNAIL_VIEW';
    this.ui_.speakA11yMessage(str(msgId));
    this.appStateController_.saveViewOptions();

    // The aria-label of toggleViewButton has been updated, we need to
    // explicitly show the tooltip.
    const toggleViewButton = this.ui_.toggleViewButton as HTMLElement;
    this.ui_.filesTooltip.updateTooltipText(toggleViewButton);
    recordEnum('ToggleFileListType', listType, ListTypesForUMA);
  }

  /**
   * KeyDown event handler for the document.
   * @param event Key event.
   */
  private onKeyDown_(event: KeyboardEvent) {
    if (event.keyCode === 9) {  // Tab
      this.pressingTab_ = true;
    }

    if (event.srcElement === this.ui_.listContainer.renameInput) {
      // Ignore keydown handler in the rename input box.
      return;
    }

    switch (getKeyModifiers(event) + event.key) {
      case 'Escape':  // Escape => Cancel dialog.
      case 'Ctrl-w':  // Ctrl+W => Cancel dialog.
        if (this.dialogType_ !== DialogType.FULL_PAGE) {
          // If there is nothing else for ESC to do, then cancel the dialog.
          event.preventDefault();
          this.ui_.dialogFooter.cancelButton.click();
        }
        break;
    }
  }

  /**
   * KeyUp event handler for the document.
   * @param event Key event.
   */
  private onKeyUp_(event: KeyboardEvent) {
    if (event.keyCode === 9) {  // Tab
      this.pressingTab_ = false;
    }
  }

  /**
   * KeyDown event handler for the directory tree element.
   * @param event Key event.
   */
  private onDirectoryTreeKeyDown_(event: KeyboardEvent) {
    // Enter => Change directory or perform default action.
    if (getKeyModifiers(event) + event.key === 'Enter') {
      const focusedItem = getFocusedTreeItem(this.ui_.directoryTree);
      if (!focusedItem) {
        return;
      }
      focusedItem.selected = true;
      if (this.dialogType_ !== DialogType.FULL_PAGE &&
          !focusedItem.hasAttribute('renaming') &&
          isSameEntry(
              this.directoryModel_.getCurrentDirEntry(),
              (focusedItem as any).entry) &&
          !this.ui_.dialogFooter.okButton.disabled) {
        this.ui_.dialogFooter.okButton.click();
      }
    }
  }

  /**
   * KeyDown event handler for the div#list-container element.
   * @param event Key event.
   */
  private onListKeyDown_(event: KeyboardEvent) {
    switch (getKeyModifiers(event) + event.key) {
      case 'Backspace':  // Backspace => Up one directory.
        event.preventDefault();
        const store = getStore();
        const state = store.getState();
        const components = state.currentDirectory?.pathComponents;
        if (!components || components.length < 2) {
          break;
        }
        const parent = components[components.length - 2]!;
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
        if (selection.totalCount === 1 &&
            isDirectoryEntry(selection.entries[0]!) &&
            !isFolderDialogType(this.dialogType_) &&
            !selection.entries.some(isTrashEntry)) {
          const item = this.ui_.listContainer.currentList.getListItemByIndex(
              selection.indexes[0]!);
          // If the item is in renaming process we don't allow to change
          // directory.
          if (item && !item.hasAttribute('renaming')) {
            event.preventDefault();
            this.directoryModel_.changeDirectoryEntry(selection.entries[0]!);
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
   */
  private onTextSearch_() {
    const text = this.ui_.listContainer.textSearchState.text;
    const dm = this.directoryModel_.getFileList();
    for (let index = 0; index < dm.length; ++index) {
      const name = dm.item(index)!.name;
      if (name.substring(0, text.length).toLowerCase() === text) {
        const selectionModel =
            this.ui_.listContainer.currentList.selectionModel;
        if (selectionModel) {
          selectionModel.selectedIndexes = [index];
        }
        return;
      }
    }

    this.ui_.listContainer.textSearchState.text = '';
  }

  /**
   * Update the UI when the current directory changes.
   *
   * @param event The directory-changed event.
   */
  private onDirectoryChanged_(_event: DirectoryChangeEvent) {
    // Update unformatted volume status.
    const newVolumeInfo = this.directoryModel_.getCurrentVolumeInfo();
    const unformatted = !!(newVolumeInfo?.error);
    this.ui_.element.toggleAttribute('unformatted', unformatted);

    // Updates UI.
    if (this.dialogType_ === DialogType.FULL_PAGE) {
      const label = this.directoryModel_.getCurrentDirName();
      document.title = `${str('FILEMANAGER_APP_NAME')} - ${label}`;
    }
  }

  private onDriveConnectionChanged_() {
    const connection = this.volumeManager_.getDriveConnectionState();
    this.ui_.dialogContainer.setAttribute('connection', connection.type);
  }

  private onWindowFocus_() {
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
   * @return True if focused.
   */
  window.isFocused = () => {
    return focused;
  };
};
