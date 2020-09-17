// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This class controls wires toolbar UI and selection model. When selection
 * status is changed, this class changes the view of toolbar. If cancel
 * selection button is pressed, this class clears the selection.
 */
class ToolbarController {
  /**
   * @param {!HTMLElement} toolbar Toolbar element which contains controls.
   * @param {!HTMLElement} navigationList Navigation list on the left pane. The
   *     position of silesSelectedLabel depends on the navitaion list's width.
   * @param {!ListContainer} listContainer List container.
   * @param {!LocationLine} locationLine Location line shown on the left side of
   *     the toolbar.
   * @param {!FileSelectionHandler} selectionHandler
   * @param {!DirectoryModel} directoryModel
   * @param {!VolumeManager} volumeManager
   * @param {!A11yAnnounce} a11y
   */
  constructor(
      toolbar, navigationList, listContainer, locationLine, selectionHandler,
      directoryModel, volumeManager, a11y) {
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
     * @private {!cr.ui.Command}
     * @const
     */
    this.deleteCommand_ = assertInstanceof(
        queryRequiredElement(
            '#delete', assert(this.toolbar_.ownerDocument.body)),
        cr.ui.Command);

    /**
     * @private {!cr.ui.Command}
     * @const
     */
    this.refreshCommand_ = assertInstanceof(
        queryRequiredElement(
            '#refresh', assert(this.toolbar_.ownerDocument.body)),
        cr.ui.Command);

    /**
     * @private {!cr.ui.Command}
     * @const
     */
    this.newFolderCommand_ = assertInstanceof(
        queryRequiredElement(
            '#new-folder', assert(this.toolbar_.ownerDocument.body)),
        cr.ui.Command);

    /**
     * @private {!cr.ui.Command}
     * @const
     */
    this.invokeSharesheetCommand_ = assertInstanceof(
        queryRequiredElement(
            '#invoke-sharesheet', assert(this.toolbar_.ownerDocument.body)),
        cr.ui.Command);

    /**
     * @private {!cr.ui.Command}
     * @const
     */
    this.togglePinnedCommand_ = assertInstanceof(
        queryRequiredElement(
            '#toggle-pinned', assert(this.toolbar_.ownerDocument.body)),
        cr.ui.Command);

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
     * @private {!LocationLine}
     * @const
     */
    this.locationLine_ = locationLine;

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

    if (util.isFilesNg()) {
      this.togglePinnedCommand_.addEventListener(
          'checkedChange', this.updatePinnedToggle_.bind(this));

      this.togglePinnedCommand_.addEventListener(
          'disabledChange', this.updatePinnedToggle_.bind(this));

      this.togglePinnedCommand_.addEventListener(
          'hiddenChange', this.updatePinnedToggle_.bind(this));

      this.pinnedToggle_.addEventListener(
          'change', this.onPinnedToggleChanged_.bind(this));
    }

    // The old layout needed the cancel selection button to resize every
    // time the splitter was moved. Not needed for files-ng.
    if (!util.isFilesNg()) {
      this.navigationList_.addEventListener(
          'relayout', this.onNavigationListRelayout_.bind(this));
    }

    this.directoryModel_.addEventListener(
        'directory-changed', this.updateCurrentDirectoryButtons_.bind(this));

    // Watch visibility of toolbar buttons to update the width of location line.
    const observer =
        new MutationObserver(this.onToolbarButtonsMutated_.bind(this));
    const toolbarButtons =
        this.toolbar_.querySelectorAll('.icon-button, .combobutton');
    for (let i = 0; i < toolbarButtons.length; i++) {
      observer.observe(
          toolbarButtons[i],
          /** @type MutationObserverInit */ ({attributes: true}));
    }
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
    // files" is a fake volume on first access until the VM is loaded and the
    // mount point is initialised. The volume is technically read-only since the
    // temportary fake volume can (and should) not be written to. However,
    // showing the read only label is not appropriate since the volume will
    // become read-write once all loading has completed.
    this.readOnlyIndicator_.hidden =
        !(locationInfo && locationInfo.isReadOnly &&
          locationInfo.rootType !== VolumeManagerCommon.RootType.CROSTINI);
  }

  /** @private */
  updateRefreshCommand_() {
    const volumeInfo = this.directoryModel_.getCurrentVolumeInfo();
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

    // Update visibility of the delete button.
    this.deleteButton_.hidden =
        (selection.totalCount === 0 || this.directoryModel_.isReadOnly() ||
         selection.hasReadOnlyEntry() ||
         selection.entries.some(
             entry => util.isNonModifiable(this.volumeManager_, entry)));

    if (util.isFilesNg()) {
      this.togglePinnedCommand_.canExecuteChange(
          this.listContainer_.currentList);
    }

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
        // Some custom styles depend on |check-select| class. We need to
        // re-evaluate the custom styles when the class value is changed.
        Polymer.updateStyles();
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
   * Handles the relayout event occurred on the navigation list.
   * @private
   */
  onNavigationListRelayout_() {
    // Not needed for files-ng, see comment above where this function is used.
    if (!util.isFilesNg()) {
      // Make the width of spacer same as the width of navigation list.
      const navWidth =
          parseFloat(window.getComputedStyle(this.navigationList_).width);
      this.cancelSelectionButtonWrapper_.style.width = navWidth + 'px';
    }
  }

  /**
   * Handles the mutation event occurred on attributes of toolbar buttons.
   * Toolbar buttons visibility can affect the available width for location
   * line.
   * @private
   */
  onToolbarButtonsMutated_() {
    this.locationLine_.truncate();
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
}
