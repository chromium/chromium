// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Manages actions for the current selection.
 */
class ActionsController {
  /**
   * @param {!VolumeManager} volumeManager
   * @param {!MetadataModel} metadataModel
   * @param {!DirectoryModel} directoryModel
   * @param {!FolderShortcutsDataModel} shortcutsModel
   * @param {!DriveSyncHandler} driveSyncHandler
   * @param {!FileSelectionHandler} selectionHandler
   * @param {!FileManagerUI} ui
   */
  constructor(
      volumeManager, metadataModel, directoryModel, shortcutsModel,
      driveSyncHandler, selectionHandler, ui) {
    /** @private @const {!VolumeManager} */
    this.volumeManager_ = volumeManager;

    /** @private @const {!MetadataModel} */
    this.metadataModel_ = metadataModel;

    /** @private @const {!DirectoryModel} */
    this.directoryModel_ = directoryModel;

    /** @private @const {!FolderShortcutsDataModel} */
    this.shortcutsModel_ = shortcutsModel;

    /** @private @const {!DriveSyncHandler} */
    this.driveSyncHandler_ = driveSyncHandler;

    /** @private @const {!FileSelectionHandler} */
    this.selectionHandler_ = selectionHandler;

    /** @private @const {!FileManagerUI} */
    this.ui_ = ui;

    /** @private @const {Map<string, ActionsModel>} */
    this.readyModels_ = new Map();

    /** @private @const {Map<string, Promise<ActionsModel>>} */
    this.initializingdModels_ = new Map();

    /**
     * Key for in-memory state for current directory.
     * @private {?string}
     */
    this.currentDirKey_ = null;

    /**
     * Key for in-memory state for current selection in the file list.
     * @private {?string}
     */
    this.currentSelectionKey_ = null;

    /**
     * Id for an UI update, when an async update happens we only send the state
     * to the DOM if the sequence hasn't changed since its start.
     *
     * @private {number}
     */
    this.updateUiSequence_ = 0;

    // Attach listeners to non-user events which will only update the in-memory
    // ActionsModel.
    this.ui_.directoryTree.addEventListener(
        'change', this.onNavigationListSelectionChanged_.bind(this), true);
    this.selectionHandler_.addEventListener(
        FileSelectionHandler.EventType.CHANGE_THROTTLED,
        this.onSelectionChanged_.bind(this));

    // Attach listeners to events based on user action to show the menu, which
    // updates the DOM.
    cr.ui.contextMenuHandler.addEventListener(
        'show', this.onContextMenuShow_.bind(this));
    this.ui_.selectionMenuButton.addEventListener(
        'menushow', this.onMenuShow_.bind(this));
    this.ui_.gearButton.addEventListener(
        'menushow', this.onMenuShow_.bind(this));
  }

  /**
   * @param {Element} element
   * @return {!Array<Entry|FileEntry>}
   * @private
   */
  getEntriesFor_(element) {
    // Element can be null, eg. when invoking a command via a keyboard shortcut.
    if (!element) {
      return [];
    }

    if (this.ui_.listContainer.element.contains(element) ||
        this.ui_.toolbar.contains(element) ||
        this.ui_.fileContextMenu.contains(element) ||
        document.body === element) {
      return this.selectionHandler_.selection.entries;
    }
    if (this.ui_.directoryTree.contains(element) ||
        this.ui_.directoryTree.contextMenuForRootItems.contains(element) ||
        this.ui_.directoryTree.contextMenuForSubitems.contains(element)) {
      if (element.entry) {
        // DirectoryItem has "entry" attribute.
        return [element.entry];
      }
      if (element.selectedItem && element.selectedItem.entry) {
        // DirectoryTree has the selected item.
        return [element.selectedItem.entry];
      }
    }

    return [];
  }

  /**
   * @param {!Array<Entry|FileEntry>} entries
   * @return {string}
   * @private
   * */
  getEntriesKey_(entries) {
    return entries.map(entry => entry.toURL()).join(';');
  }

  /**
   * @param {!string} key Key to be cleared.
   * @private
   */
  clearLocalCache_(key) {
    this.readyModels_.delete(key);
    this.initializingdModels_.delete(key);
  }

  /**
   * @param {Element} element
   * @private
   */
  updateUI_(element) {
    const entries = this.getEntriesFor_(/** @type {Element} */ (element));

    // Try to update synchronously.
    const actionsModel = this.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      this.ui_.actionsSubmenu.setActionsModel(actionsModel, element);
      return;
    }

    // Asynchronously update the UI, after fetching actions from the backend.
    const sequence = ++this.updateUiSequence_;
    this.getActionsForEntries(entries).then((actionsModel) => {
      // Only update if there wasn't another UI update started while the promise
      // was resolving, which could be for different entries and avoids multiple
      // updates for the same entries.
      if (sequence === this.updateUiSequence_) {
        this.ui_.actionsSubmenu.setActionsModel(actionsModel, element);
      }
    });
  }

  /**
   * @param {!Event} event
   * @private
   */
  onContextMenuShow_(event) {
    this.updateUI_(event.element);
  }

  /**
   * @param {!Event} event
   * @private
   */
  onMenuShow_(event) {
    this.updateUI_(/** @type {Element} */ (event.target));
  }

  /**
   * @private
   */
  onSelectionChanged_() {
    const entries = this.selectionHandler_.selection.entries;

    if (!entries.length) {
      this.currentSelectionKey_ = null;
      return;
    }

    const currentKey = this.getEntriesKey_(entries);
    this.currentSelectionKey_ = currentKey;

    this.getActionsForEntries(entries);
  }

  /**
   * @private
   */
  onNavigationListSelectionChanged_() {
    const entry = this.ui_.directoryTree.selectedItem &&
        this.ui_.directoryTree.selectedItem.entry;

    if (!entry) {
      this.currentDirKey_ = null;
      return;
    }

    // Force to recalculate for the new current directory.
    const key = this.getEntriesKey_([entry]);
    this.clearLocalCache_(key);
    this.currentDirKey_ = key;

    this.getActionsForEntries([entry]);
  }

  /**
   * @param {!Array<Entry|FileEntry>} entries
   * @return {?ActionsModel}
   */
  getInitializedActionsForEntries(entries) {
    const key = this.getEntriesKey_(entries);
    return this.readyModels_.get(key);
  }

  /**
   * @param {!Array<Entry|FileEntry>} entries
   * @return {Promise<ActionsModel>}
   */
  getActionsForEntries(entries) {
    const key = this.getEntriesKey_(entries);
    if (!key) {
      return Promise.resolve();
    }

    // If it's still initializing, return the cached promise.
    let promise = this.initializingdModels_.get(key);
    if (promise) {
      return promise;
    }

    // If it's already initialized, resolve with the model.
    let actionsModel = this.readyModels_.get(key);
    if (actionsModel) {
      return Promise.resolve(actionsModel);
    }

    actionsModel = new ActionsModel(
        this.volumeManager_, this.metadataModel_, this.shortcutsModel_,
        this.driveSyncHandler_, this.ui_, entries);

    actionsModel.addEventListener(
        'invalidated', this.clearLocalCache_.bind(this, key), {once: true});

    // Once it's initialized, move to readyModels_ so we don't have to construct
    // and initialized again.
    const init = actionsModel.initialize().then(() => {
      this.initializingdModels_.delete(key);
      this.readyModels_.set(key, actionsModel);
      return actionsModel;
    });

    // Cache in the waiting initialization map.
    this.initializingdModels_.set(key, init);
    return init;
  }

  /**
   * @param {Action} action
   */
  executeAction(action) {
    const entries = action.getEntries();
    const key = this.getEntriesKey_(entries);
    // Invalidate the model early so new UI has to refresh it.
    this.readyModels_.delete(key);
    action.execute();
  }
}
