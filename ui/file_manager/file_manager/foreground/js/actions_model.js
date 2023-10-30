// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import {isSameVolume, unwrapEntry} from '../../common/js/entry_utils.js';
import {recordBoolean} from '../../common/js/metrics.js';
import {strf} from '../../common/js/translations.js';
import {visitURL} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {constants} from './constants.js';
import {FolderShortcutsDataModel} from './folder_shortcuts_data_model.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {ActionModelUI} from './ui/action_model_ui.js';

/**
 * A single action, that can be taken on a set of entries.
 * @interface
 */
export class Action {
  /**
   * Executes this action on the set of entries.
   */
  execute() {}

  /**
   * Checks whether this action can execute on the set of entries.
   *
   * @return {boolean} True if the function can execute, false if not.
   */
  canExecute() {
    return false;
  }

  /**
   * @return {?string}
   */
  getTitle() {
    return null;
  }

  /**
   * Entries that this Action will execute upon.
   * @return {!Array<!Entry|!FileEntry>}
   */
  getEntries() {
    return [];
  }
}

/** @implements {Action} */
class DriveShareAction {
  /**
   * @param {!Entry} entry
   * @param {!MetadataModel} metadataModel
   * @param {!ActionModelUI} ui
   * @param {!VolumeManager} volumeManager
   */
  constructor(entry, metadataModel, volumeManager, ui) {
    /**
     * @private @type {!Entry}
     * @const
     */
    this.entry_ = entry;

    /**
     * @private @type {!MetadataModel}
     * @const
     */
    this.metadataModel_ = metadataModel;

    /**
     * @private @type {!VolumeManager}
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private @type {!ActionModelUI}
     * @const
     */
    this.ui_ = ui;
  }

  /**
   * @param {!Array<!Entry>} entries
   * @param {!MetadataModel} metadataModel
   * @param {!ActionModelUI} ui
   * @param {!VolumeManager} volumeManager
   * @return {DriveShareAction}
   */
  static create(entries, metadataModel, volumeManager, ui) {
    if (entries.length !== 1) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'DriveShareAction'.
      return null;
    }
    // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined'
    // is not assignable to parameter of type 'FileSystemEntry'.
    return new DriveShareAction(entries[0], metadataModel, volumeManager, ui);
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveShareAction' does not
  // extend another class.
  execute() {
    // Open the Sharing dialog in a new window.
    chrome.fileManagerPrivate.getEntryProperties(
        // @ts-ignore: error TS2322: Type 'FileSystemEntry | FilesAppEntry' is
        // not assignable to type 'FileSystemEntry'.
        [unwrapEntry(this.entry_)], ['shareUrl'], results => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
            return;
          }
          if (results.length != 1) {
            console.warn(
                'getEntryProperties for shareUrl should return 1 entry ' +
                '(returned ' + results.length + ')');
            return;
          }
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          if (results[0].shareUrl === undefined) {
            console.warn('getEntryProperties shareUrl is undefined');
            return;
          }
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          visitURL(assert(results[0].shareUrl));
        });
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveShareAction' does not
  // extend another class.
  canExecute() {
    const metadata = this.metadataModel_.getCache([this.entry_], ['canShare']);
    assert(metadata.length === 1);
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    const canShareItem = metadata[0].canShare !== false;
    return this.volumeManager_.getDriveConnectionState().type !==
        chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE &&
        canShareItem;
  }

  /**
   * @return {?string}
   */
  getTitle() {
    return null;
  }

  /** @override */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveShareAction' does not
  // extend another class.
  getEntries() {
    return [this.entry_];
  }
}


/** @implements {Action} */
class DriveToggleOfflineAction {
  /**
   * @param {!Array<!Entry>} entries
   * @param {!MetadataModel} metadataModel
   * @param {!ActionModelUI} ui
   * @param {!VolumeManager} volumeManager
   * @param {boolean} value
   * @param {function():void} onExecute
   */
  constructor(entries, metadataModel, ui, volumeManager, value, onExecute) {
    /**
     * @private @type {!Array<!Entry>}
     * @const
     */
    this.entries_ = entries;

    /**
     * @private @type {!MetadataModel}
     * @const
     */
    this.metadataModel_ = metadataModel;

    /**
     * @private @type {!VolumeManager}
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private @type {!ActionModelUI}
     * @const
     */
    this.ui_ = ui;

    /**
     * @private @type {boolean}
     * @const
     */
    this.value_ = value;

    /**
     * @private @type {function():void}
     * @const
     */
    this.onExecute_ = onExecute;
  }

  /**
   * @param {!Array<!Entry>} entries
   * @param {!MetadataModel} metadataModel
   * @param {!ActionModelUI} ui
   * @param {!VolumeManager} volumeManager
   * @param {boolean} value
   * @param {function():void} onExecute
   * @return {DriveToggleOfflineAction}
   */
  static create(entries, metadataModel, ui, volumeManager, value, onExecute) {
    const actionableEntries = entries.filter(
        entry =>
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        metadataModel.getCache([entry], ['pinned'])[0].pinned !== value);

    if (actionableEntries.length === 0) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'DriveToggleOfflineAction'.
      return null;
    }

    return new DriveToggleOfflineAction(
        actionableEntries, metadataModel, ui, volumeManager, value, onExecute);
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveToggleOfflineAction'
  // does not extend another class.
  execute() {
    const entries = this.entries_;
    if (entries.length == 0) {
      return;
    }

    // @ts-ignore: error TS7034: Variable 'currentEntry' implicitly has type
    // 'any' in some locations where its type cannot be determined.
    let currentEntry;
    let error = false;

    const steps = {
      // Pick an entry and pin it.
      start: () => {
        // Check if all the entries are pinned or not.
        if (entries.length === 0) {
          return;
        }
        currentEntry = entries.shift();
        // Skip files we cannot pin.
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        if (this.metadataModel_
                // @ts-ignore: error TS2322: Type 'FileSystemEntry | undefined'
                // is not assignable to type 'FileSystemEntry'.
                .getCache([currentEntry], ['canPin'])[0]
                // @ts-ignore: error TS2339: Property 'canPin' does not exist on
                // type 'MetadataItem'.
                .canPin) {
          chrome.fileManagerPrivate.pinDriveFile(
              // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
              // undefined' is not assignable to parameter of type
              // 'FileSystemEntry'.
              currentEntry, this.value_, steps.entryPinned);
        } else {
          steps.start();
        }
      },

      // Check the result of pinning.
      entryPinned: () => {
        error = !!chrome.runtime.lastError;
        recordBoolean('DrivePinSuccess', !error);
        // @ts-ignore: error TS7005: Variable 'currentEntry' implicitly has an
        // 'any' type.
        if (this.metadataModel_.getCache([currentEntry], ['hosted'])[0]
                .hosted) {
          recordBoolean('DriveHostedFilePinSuccess', !error);
        }
        if (error && this.value_) {
          // @ts-ignore: error TS7005: Variable 'currentEntry' implicitly has an
          // 'any' type.
          this.metadataModel_.get([currentEntry], ['size']).then(results => {
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            steps.showError(results[0].size);
          });
          return;
        }
        // @ts-ignore: error TS7005: Variable 'currentEntry' implicitly has an
        // 'any' type.
        this.metadataModel_.notifyEntriesChanged([currentEntry]);
        // @ts-ignore: error TS7005: Variable 'currentEntry' implicitly has an
        // 'any' type.
        this.metadataModel_.get([currentEntry], ['pinned'])
            .then(steps.updateUI);
      },

      // Update the user interface according to the cache state.
      updateUI: () => {
        // After execution of last entry call "onExecute_" to invalidate the
        // model.
        if (entries.length === 0) {
          this.onExecute_();
        }
        this.ui_.listContainer.currentView.updateListItemsMetadata(
            // @ts-ignore: error TS7005: Variable 'currentEntry' implicitly has
            // an 'any' type.
            'external', [currentEntry]);
        if (!error) {
          steps.start();
        }
      },

      // Show an error.
      // TODO(crbug.com/1138744): Migrate this error message to a visual signal.
      // @ts-ignore: error TS7006: Parameter 'size' implicitly has an 'any'
      // type.
      showError: size => {
        this.ui_.alertDialog.show(
            // @ts-ignore: error TS7005: Variable 'currentEntry' implicitly has
            // an 'any' type.
            strf('OFFLINE_FAILURE_MESSAGE', unescape(currentEntry.name)), null,
            // @ts-ignore: error TS2554: Expected 1-3 arguments, but got 4.
            null, null);
      },
    };
    steps.start();
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveToggleOfflineAction'
  // does not extend another class.
  canExecute() {
    return this.metadataModel_
        .getCache(this.entries_, ['canPin'])
        // @ts-ignore: error TS2339: Property 'canPin' does not exist on type
        // 'MetadataItem'.
        .some(metadata => metadata.canPin);
  }

  /**
   * @return {?string}
   */
  getTitle() {
    return null;
  }

  /** @override */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveToggleOfflineAction'
  // does not extend another class.
  getEntries() {
    return this.entries_;
  }
}


/** @implements {Action} */
class DriveCreateFolderShortcutAction {
  /**
   * @param {!Entry} entry
   * @param {!FolderShortcutsDataModel} shortcutsModel
   * @param {function():void} onExecute
   */
  constructor(entry, shortcutsModel, onExecute) {
    /**
     * @private @type {!Entry}
     * @const
     */
    this.entry_ = entry;

    /**
     * @private @type {!FolderShortcutsDataModel}
     * @const
     */
    this.shortcutsModel_ = shortcutsModel;

    /**
     * @private @type {function():void}
     * @const
     */
    this.onExecute_ = onExecute;
  }

  /**
   * @param {!Array<!Entry>} entries
   * @param {!VolumeManager} volumeManager
   * @param {!FolderShortcutsDataModel} shortcutsModel
   * @param {function():void} onExecute
   * @return {DriveCreateFolderShortcutAction}
   */
  static create(entries, volumeManager, shortcutsModel, onExecute) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    if (entries.length !== 1 || entries[0].isFile) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'DriveCreateFolderShortcutAction'.
      return null;
    }
    // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined'
    // is not assignable to parameter of type 'FileSystemEntry | FilesAppEntry'.
    const locationInfo = volumeManager.getLocationInfo(entries[0]);
    if (!locationInfo || locationInfo.isSpecialSearchRoot ||
        locationInfo.isRootEntry) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'DriveCreateFolderShortcutAction'.
      return null;
    }
    return new DriveCreateFolderShortcutAction(
        // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
        // undefined' is not assignable to parameter of type 'FileSystemEntry'.
        entries[0], shortcutsModel, onExecute);
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class
  // 'DriveCreateFolderShortcutAction' does not extend another class.
  execute() {
    this.shortcutsModel_.add(this.entry_);
    this.onExecute_();
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class
  // 'DriveCreateFolderShortcutAction' does not extend another class.
  canExecute() {
    return !this.shortcutsModel_.exists(this.entry_);
  }

  /**
   * @return {?string}
   */
  getTitle() {
    return null;
  }

  /** @override */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class
  // 'DriveCreateFolderShortcutAction' does not extend another class.
  getEntries() {
    return [this.entry_];
  }
}


/** @implements {Action} */
class DriveRemoveFolderShortcutAction {
  /**
   * @param {!Entry} entry
   * @param {!FolderShortcutsDataModel} shortcutsModel
   * @param {function():void} onExecute
   */
  constructor(entry, shortcutsModel, onExecute) {
    /**
     * @private @type {!Entry}
     * @const
     */
    this.entry_ = entry;

    /**
     * @private @type {!FolderShortcutsDataModel}
     * @const
     */
    this.shortcutsModel_ = shortcutsModel;

    /**
     * @private @type {function():void}
     * @const
     */
    this.onExecute_ = onExecute;
  }

  /**
   * @param {!Array<!Entry>} entries
   * @param {!FolderShortcutsDataModel} shortcutsModel
   * @param {function():void} onExecute
   * @return {DriveRemoveFolderShortcutAction}
   */
  static create(entries, shortcutsModel, onExecute) {
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    if (entries.length !== 1 || entries[0].isFile ||
        // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
        // undefined' is not assignable to parameter of type 'FileSystemEntry'.
        !shortcutsModel.exists(entries[0])) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'DriveRemoveFolderShortcutAction'.
      return null;
    }
    return new DriveRemoveFolderShortcutAction(
        // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
        // undefined' is not assignable to parameter of type 'FileSystemEntry'.
        entries[0], shortcutsModel, onExecute);
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class
  // 'DriveRemoveFolderShortcutAction' does not extend another class.
  execute() {
    this.shortcutsModel_.remove(this.entry_);
    this.onExecute_();
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class
  // 'DriveRemoveFolderShortcutAction' does not extend another class.
  canExecute() {
    return this.shortcutsModel_.exists(this.entry_);
  }

  /**
   * @return {?string}
   */
  getTitle() {
    return null;
  }

  /** @override */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class
  // 'DriveRemoveFolderShortcutAction' does not extend another class.
  getEntries() {
    return [this.entry_];
  }
}


/**
 * Opens the entry in Drive Web for the user to manage permissions etc.
 *
 * @implements {Action}
 */
class DriveManageAction {
  /**
   * @param {!Entry} entry The entry to open the 'Manage' page for.
   * @param {!ActionModelUI} ui
   * @param {!VolumeManager} volumeManager
   */
  constructor(entry, volumeManager, ui) {
    /**
     * The entry to open the 'Manage' page for.
     *
     * @private @type {!Entry}
     * @const
     */
    this.entry_ = entry;

    /**
     * @private @type {!VolumeManager}
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private @type {!ActionModelUI}
     * @const
     */
    this.ui_ = ui;
  }

  /**
   * Creates a new DriveManageAction object.
   * |entries| must contain only a single entry.
   *
   * @param {!Array<!Entry>} entries
   * @param {!ActionModelUI} ui
   * @param {!VolumeManager} volumeManager
   * @return {DriveManageAction}
   */
  static create(entries, volumeManager, ui) {
    if (entries.length !== 1) {
      // @ts-ignore: error TS2322: Type 'null' is not assignable to type
      // 'DriveManageAction'.
      return null;
    }

    // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined'
    // is not assignable to parameter of type 'FileSystemEntry'.
    return new DriveManageAction(entries[0], volumeManager, ui);
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveManageAction' does not
  // extend another class.
  execute() {
    chrome.fileManagerPrivate.getEntryProperties(
        // @ts-ignore: error TS2322: Type 'FileSystemEntry | FilesAppEntry' is
        // not assignable to type 'FileSystemEntry'.
        [unwrapEntry(this.entry_)], ['alternateUrl'], results => {
          if (chrome.runtime.lastError) {
            console.error(chrome.runtime.lastError.message);
            return;
          }
          if (results.length != 1) {
            console.warn(
                'getEntryProperties for alternateUrl should return 1 entry ' +
                '(returned ' + results.length + ')');
            return;
          }
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          if (results[0].alternateUrl === undefined) {
            console.warn('getEntryProperties alternateUrl is undefined');
            return;
          }
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          visitURL(assert(results[0].alternateUrl));
        });
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveManageAction' does not
  // extend another class.
  canExecute() {
    return this.volumeManager_.getDriveConnectionState().type !==
        chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE;
  }

  /**
   * @return {?string}
   */
  getTitle() {
    return null;
  }

  /** @override */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'DriveManageAction' does not
  // extend another class.
  getEntries() {
    return [this.entry_];
  }
}


/**
 * A custom action set by the FSP API.
 *
 * @implements {Action}
 */
class CustomAction {
  /**
   * @param {!Array<!Entry>} entries
   * @param {string} id
   * @param {?string} title
   * @param {function():void} onExecute
   */
  constructor(entries, id, title, onExecute) {
    /**
     * @private @type {!Array<!Entry>}
     * @const
     */
    this.entries_ = entries;

    /**
     * @private @type {string}
     * @const
     */
    this.id_ = id;

    /**
     * @private @type {?string}
     * @const
     */
    this.title_ = title;

    /**
     * @private @type {function():void}
     * @const
     */
    this.onExecute_ = onExecute;
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'CustomAction' does not extend
  // another class.
  execute() {
    chrome.fileManagerPrivate.executeCustomAction(
        // @ts-ignore: error TS2345: Argument of type '(FileSystemEntry |
        // FilesAppEntry)[]' is not assignable to parameter of type
        // 'FileSystemEntry[]'.
        this.entries_.map(e => unwrapEntry(e)), this.id_, () => {
          if (chrome.runtime.lastError) {
            console.error(
                'Failed to execute a custom action because of: ' +
                chrome.runtime.lastError.message);
          }
          this.onExecute_();
        });
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'CustomAction' does not extend
  // another class.
  canExecute() {
    return true;  // Custom actions are always executable.
  }

  /**
   * @override
   */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'CustomAction' does not extend
  // another class.
  getTitle() {
    return this.title_;
  }

  /** @override */
  // @ts-ignore: error TS4121: This member cannot have a JSDoc comment with an
  // '@override' tag because its containing class 'CustomAction' does not extend
  // another class.
  getEntries() {
    return this.entries_;
  }
}

/**
 * Represents a set of actions for a set of entries. Includes actions set
 * locally in JS, as well as those retrieved from the FSP API.
 */
export class ActionsModel extends EventTarget {
  /**
   * @param {!VolumeManager} volumeManager
   * @param {!MetadataModel} metadataModel
   * @param {!FolderShortcutsDataModel} shortcutsModel
   * @param {!ActionModelUI} ui
   * @param {!Array<!Entry>} entries
   */
  constructor(volumeManager, metadataModel, shortcutsModel, ui, entries) {
    super();

    /**
     * @private @type {!VolumeManager}
     * @const
     */
    this.volumeManager_ = volumeManager;

    /**
     * @private @type {!MetadataModel}
     * @const
     */
    this.metadataModel_ = metadataModel;

    /**
     * @private @type {!FolderShortcutsDataModel}
     * @const
     */
    this.shortcutsModel_ = shortcutsModel;

    /**
     * @private @type {!ActionModelUI}
     * @const
     */
    this.ui_ = ui;

    /**
     * @private @type {!Array<!Entry>}
     * @const
     */
    this.entries_ = entries;

    /**
     * @private @type {!Record<string, !Action>}
     */
    this.actions_ = {};

    /**
     * @private @type {?function():void}
     */
    this.initializePromiseReject_ = null;

    /**
     * @private @type {?Promise<void>}
     */
    this.initializePromise_ = null;

    /**
     * @private @type {boolean}
     */
    this.destroyed_ = false;
  }

  /**
   * Initializes the ActionsModel, including populating the list of available
   * actions for the given entries.
   * @return {!Promise<void>}
   */
  initialize() {
    if (this.initializePromise_) {
      return this.initializePromise_;
    }

    this.initializePromise_ =
        new Promise((fulfill, reject) => {
          if (this.destroyed_) {
            reject();
            return;
          }
          this.initializePromiseReject_ = reject;

          const volumeInfo = this.entries_.length >= 1 &&
              // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
              // undefined' is not assignable to parameter of type
              // 'FileSystemEntry | FilesAppEntry'.
              this.volumeManager_.getVolumeInfo(this.entries_[0]);
          // All entries need to be on the same volume to execute ActionsModel
          // commands.
          if (!volumeInfo ||
              !isSameVolume(this.entries_, this.volumeManager_)) {
            fulfill({});
            return;
          }

          const actions = {};
          switch (volumeInfo.volumeType) {
            // For Drive, actions are constructed directly in the Files app
            // code.
            case VolumeManagerCommon.VolumeType.DRIVE:
              const shareAction = DriveShareAction.create(
                  this.entries_, this.metadataModel_, this.volumeManager_,
                  this.ui_);
              if (shareAction) {
                // @ts-ignore: error TS7053: Element implicitly has an 'any'
                // type because expression of type 'string' can't be used to
                // index type '{}'.
                actions[ActionsModel.CommonActionId.SHARE] = shareAction;
              }

              const saveForOfflineAction = DriveToggleOfflineAction.create(
                  this.entries_, this.metadataModel_, this.ui_,
                  this.volumeManager_, true, this.invalidate_.bind(this));
              if (saveForOfflineAction) {
                // @ts-ignore: error TS7053: Element implicitly has an 'any'
                // type because expression of type 'string' can't be used to
                // index type '{}'.
                actions[ActionsModel.CommonActionId.SAVE_FOR_OFFLINE] =
                    saveForOfflineAction;
              }

              const offlineNotNecessaryAction = DriveToggleOfflineAction.create(
                  this.entries_, this.metadataModel_, this.ui_,
                  this.volumeManager_, false, this.invalidate_.bind(this));
              if (offlineNotNecessaryAction) {
                // @ts-ignore: error TS7053: Element implicitly has an 'any'
                // type because expression of type 'string' can't be used to
                // index type '{}'.
                actions[ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY] =
                    offlineNotNecessaryAction;
              }

              const createFolderShortcutAction =
                  DriveCreateFolderShortcutAction.create(
                      this.entries_, this.volumeManager_, this.shortcutsModel_,
                      this.invalidate_.bind(this));
              if (createFolderShortcutAction) {
                // @ts-ignore: error TS7053: Element implicitly has an 'any'
                // type because expression of type 'string' can't be used to
                // index type '{}'.
                actions[ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT] =
                    createFolderShortcutAction;
              }

              const removeFolderShortcutAction =
                  DriveRemoveFolderShortcutAction.create(
                      this.entries_, this.shortcutsModel_,
                      this.invalidate_.bind(this));
              if (removeFolderShortcutAction) {
                // @ts-ignore: error TS7053: Element implicitly has an 'any'
                // type because expression of type 'string' can't be used to
                // index type '{}'.
                actions[ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT] =
                    removeFolderShortcutAction;
              }

              const manageInDriveAction = DriveManageAction.create(
                  this.entries_, this.volumeManager_, this.ui_);
              if (manageInDriveAction) {
                // @ts-ignore: error TS7053: Element implicitly has an 'any'
                // type because expression of type 'string' can't be used to
                // index type '{}'.
                actions[ActionsModel.InternalActionId.MANAGE_IN_DRIVE] =
                    manageInDriveAction;
              }

              fulfill(actions);
              break;

            // For FSP, fetch custom actions via an API.
            case VolumeManagerCommon.VolumeType.PROVIDED:
              chrome.fileManagerPrivate.getCustomActions(
                  // @ts-ignore: error TS2345: Argument of type
                  // '(FileSystemEntry | FilesAppEntry)[]' is not assignable to
                  // parameter of type 'FileSystemEntry[]'.
                  this.entries_.map(e => unwrapEntry(e)), customActions => {
                    if (chrome.runtime.lastError) {
                      console.error(
                          'Failed to fetch custom actions because of: ' +
                          chrome.runtime.lastError.message);
                    } else {
                      customActions.forEach(action => {
                        // Skip fake actions that should not be displayed to the
                        // user, for example actions that just expose OneDrive
                        // URLs.
                        // TODO(b/237216270): Restrict to the ODFS extension ID.
                        if (action.id ===
                            constants.FSP_ACTION_HIDDEN_ONEDRIVE_URL) {
                          return;
                        }
                        if (action.id ===
                            constants.FSP_ACTION_HIDDEN_ONEDRIVE_USER_EMAIL) {
                          return;
                        }
                        if (action.id ===
                            constants
                                .FSP_ACTION_HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED) {
                          return;
                        }
                        // @ts-ignore: error TS7053: Element implicitly has an
                        // 'any' type because expression of type 'string' can't
                        // be used to index type '{}'.
                        actions[action.id] = new CustomAction(
                            this.entries_, action.id, action.title || null,
                            this.invalidate_.bind(this));
                      });
                    }
                    fulfill(actions);
                  });
              break;

            default:
              fulfill(actions);
          }
        }).then(actions => {
          this.actions_ = actions;
        });

    return this.initializePromise_;
  }

  /**
   * @return {!Record<string, !Action>}
   */
  getActions() {
    return this.actions_;
  }

  /**
   * @param {string} id
   * @return {Action}
   */
  getAction(id) {
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type because
    // expression of type 'string' can't be used to index type '{}'.
    return this.actions_[id] || null;
  }

  /**
   * Destroys the model and cancels initialization if in progress.
   */
  destroy() {
    this.destroyed_ = true;
    if (this.initializePromiseReject_ !== null) {
      const reject = this.initializePromiseReject_;
      this.initializePromiseReject_ = null;
      reject();
    }
  }

  /**
   * Invalidates the current actions model by emitting an invalidation event.
   * The model has to be initialized again, as the list of actions might have
   * changed.
   *
   * @private
   */
  invalidate_() {
    if (this.initializePromiseReject_ !== null) {
      const reject = this.initializePromiseReject_;
      this.initializePromiseReject_ = null;
      this.initializePromise_ = null;
      reject();
    }
    dispatchSimpleEvent(this, 'invalidated', true);
  }

  /**
   * @return {!Array<!Entry>}
   * @public
   */
  getEntries() {
    return this.entries_;
  }
}

/**
 * List of common actions, used both internally and externally (custom actions).
 * Keep in sync with file_system_provider.idl.
 * @enum {string}
 */
ActionsModel.CommonActionId = {
  SHARE: 'SHARE',
  SAVE_FOR_OFFLINE: 'SAVE_FOR_OFFLINE',
  OFFLINE_NOT_NECESSARY: 'OFFLINE_NOT_NECESSARY',
};

/**
 * @enum {string}
 */
ActionsModel.InternalActionId = {
  CREATE_FOLDER_SHORTCUT: 'pin-folder',
  REMOVE_FOLDER_SHORTCUT: 'unpin-folder',
  MANAGE_IN_DRIVE: 'manage-in-drive',
};
