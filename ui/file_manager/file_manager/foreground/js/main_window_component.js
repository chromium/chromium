// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Component for the main window.
 *
 * The class receives UI events from UI components that does not have their own
 * controller, and do corresponding action by using models/other controllers.
 *
 * The class also observes model/browser API's event to update the misc
 * components.
 */
class MainWindowComponent {
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
    ui.locationLine.addEventListener(
        'pathclick', this.onBreadcrumbClick_.bind(this));
    ui.toggleViewButton.addEventListener(
        'click', this.onToggleViewButtonClick_.bind(this));
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
    // callback. Therefore we can pass a dummy value to it.
    // TODO(yamaguchi): Revise TapHandler.handleTouchEvents to delete the param.
    this.tapHandler_.handleTouchEvents(event, -1, (e, index, eventType) => {
      if (eventType == FileTapHandler.TapEvent.TAP) {
        if (e.target.classList.contains('detail-checkmark')) {
          // Tap on the checkmark should only toggle select the item just like a
          // mouse click on it.
          return false;
        }
        // The selection model has the single selection at this point.
        // When using touchscreen, the selection should be cleared because
        // we don't want show the file selected when not in check-select
        // mode.
        return this.handleOpenDefault(
            event, true /* clearSelectionAfterLaunch */);
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
    this.handleOpenDefault(event, false);
  }

  /**
   * Opens the selected item by the default command.
   * If the item is a directory, change current directory to it.
   * Otherwise, accepts the current selection.
   *
   * @param {Event} event The dblclick event.
   * @param {boolean} clearSelectionAfterLaunch
   * @return {boolean} true if successfully opened the item.
   * @private
   */
  handleOpenDefault(event, clearSelectionAfterLaunch) {
    if (this.namingController_.isRenamingInProgress()) {
      // Don't pay attention to clicks during a rename.
      return false;
    }

    const listItem = this.ui_.listContainer.findListItemForNode(
        event.touchedElement || event.srcElement);
    // It is expected that the target item should have already been selected in
    // LiseSelectionController.handlePointerDownUp on preceding mousedown event.
    const selection = this.selectionHandler_.selection;
    if (!listItem || !listItem.selected || selection.totalCount != 1) {
      return false;
    }

    const entry = selection.entries[0];
    if (entry.isDirectory) {
      this.directoryModel_.changeDirectoryEntry(
          /** @type {!DirectoryEntry} */ (entry));
    } else {
      return this.acceptSelection_(clearSelectionAfterLaunch);
    }
    return false;
  }

  /**
   * Accepts the current selection depending on the mode.
   * @param {boolean} clearSelectionAfterLaunch
   * @return {boolean} true if successfully accepted the current selection.
   * @private
   */
  acceptSelection_(clearSelectionAfterLaunch) {
    const selection = this.selectionHandler_.selection;
    if (this.dialogType_ == DialogType.FULL_PAGE) {
      this.taskController_.getFileTasks()
          .then(tasks => {
            tasks.executeDefault();
            if (clearSelectionAfterLaunch) {
              this.directoryModel_.clearSelection();
            }
          })
          .catch(error => {
            if (error) {
              console.error(error.stack || error);
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
   * Handles click event on the toggle-view button.
   * @param {Event} event Click event.
   * @private
   */
  onToggleViewButtonClick_(event) {
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

    this.ui_.listContainer.focus();
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
        const components = this.ui_.locationLine.getCurrentPathComponents();
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
        } else if (this.acceptSelection_(
                       false /* clearSelectionAfterLaunch */)) {
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
    if (newVolumeInfo && newVolumeInfo.error) {
      this.ui_.element.setAttribute('unformatted', '');

      if (newVolumeInfo.error ===
          VolumeManagerCommon.VolumeError.UNSUPPORTED_FILESYSTEM) {
        this.ui_.formatPanelError.textContent =
            str('UNSUPPORTED_FILESYSTEM_WARNING');
      } else {
        this.ui_.formatPanelError.textContent =
            str('UNKNOWN_FILESYSTEM_WARNING');
      }
    } else {
      this.ui_.element.removeAttribute('unformatted');
    }

    if (event.newDirEntry) {
      this.ui_.locationLine.show(event.newDirEntry);
      // Updates UI.
      if (this.dialogType_ === DialogType.FULL_PAGE) {
        const locationInfo =
            this.volumeManager_.getLocationInfo(event.newDirEntry);
        if (locationInfo) {
          const label = util.getEntryLabel(locationInfo, event.newDirEntry);
          document.title = `${str('FILEMANAGER_APP_NAME')} - ${label}`;
        } else {
          console.error(
              'Could not find location info for entry: ' +
              event.newDirEntry.fullPath);
        }
      }
    } else {
      this.ui_.locationLine.hide();
    }
  }

  /**
   * @private
   */
  onDriveConnectionChanged_() {
    const connection = this.volumeManager_.getDriveConnectionState();
    this.ui_.dialogContainer.setAttribute('connection', connection.type);
    this.ui_.suggestAppsDialog.onDriveConnectionChanged(connection.type);
  }

  /**
   * @private
   */
  onWindowFocus_() {
    // When the window have got a focus while the current directory is Recent
    // root, refresh the contents.
    if (this.directoryModel_.getCurrentRootType() ===
        VolumeManagerCommon.RootType.RECENT) {
      this.directoryModel_.rescan(true /* refresh */);
      // Do not start the spinner here to silently refresh the contents.
    }
  }
}
