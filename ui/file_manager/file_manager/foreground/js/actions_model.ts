// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {dispatchSimpleEvent} from 'chrome://resources/ash/common/cr_deprecated.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';

import type {VolumeManager} from '../../background/js/volume_manager.js';
import {getEntryProperties} from '../../common/js/api.js';
import {isDirectoryEntry, isSameVolume, unwrapEntry} from '../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {recordBoolean} from '../../common/js/metrics.js';
import {strf} from '../../common/js/translations.js';
import {visitURL} from '../../common/js/util.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';

import {FSP_ACTIONS_HIDDEN} from './constants.js';
import type {FolderShortcutsDataModel} from './folder_shortcuts_data_model.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import type {ActionModelUi} from './ui/action_model_ui.js';

type ActionsMap =
    Partial<Record<CommonActionId|InternalActionId|string, Action>>;

/**
 * A single action, that can be taken on a set of entries.
 */
export abstract class Action {
  /**
   * Executes this action on the set of entries.
   */
  abstract execute(): void;

  /**
   * Checks whether this action can execute on the set of entries.
   *
   * @return True if the function can execute, false if not.
   */
  abstract canExecute(): boolean;

  abstract getTitle(): string|null;

  /**
   * Entries that this Action will execute upon.
   */
  abstract getEntries(): Array<Entry|FilesAppEntry>;
}

class DriveToggleOfflineAction implements Action {
  constructor(
      private entries_: Array<Entry|FilesAppEntry>,
      private metadataModel_: MetadataModel, private ui_: ActionModelUi,
      private value_: boolean, private onExecute_: VoidCallback) {}

  static create(
      entries: Array<Entry|FilesAppEntry>, metadataModel: MetadataModel,
      ui: ActionModelUi, value: boolean, onExecute: VoidCallback) {
    const actionableEntries = entries.filter(
        entry =>
            metadataModel.getCache([entry], ['pinned'])[0]?.pinned !== value);

    if (actionableEntries.length === 0) {
      return null;
    }

    return new DriveToggleOfflineAction(
        actionableEntries, metadataModel, ui, value, onExecute);
  }

  execute() {
    const entries = this.entries_;
    if (entries.length === 0) {
      return;
    }

    let currentEntry: Entry|FilesAppEntry;
    let error = false;

    const steps = {
      // Pick an entry and pin it.
      start: () => {
        // Check if all the entries are pinned or not.
        if (entries.length === 0) {
          return;
        }
        currentEntry = entries.shift()!;
        // Skip files we cannot pin.
        if (this.metadataModel_.getCache([currentEntry], ['canPin'])[0]
                ?.canPin) {
          chrome.fileManagerPrivate.pinDriveFile(
              unwrapEntry(currentEntry) as Entry, this.value_,
              steps.entryPinned);
        } else {
          steps.start();
        }
      },

      // Check the result of pinning.
      entryPinned: () => {
        error = !!chrome.runtime.lastError;
        recordBoolean('DrivePinSuccess', !error);
        if (this.metadataModel_.getCache([currentEntry], ['hosted'])[0]
                ?.hosted) {
          recordBoolean('DriveHostedFilePinSuccess', !error);
        }
        if (error && this.value_) {
          this.metadataModel_.get([currentEntry], ['size']).then(() => {
            steps.showError();
          });
          return;
        }
        this.metadataModel_.notifyEntriesChanged([currentEntry]);
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
            'external', [currentEntry]);
        if (!error) {
          steps.start();
        }
      },

      // Show an error.
      // TODO(crbug.com/40725624): Migrate this error message to a visual signal.
      showError: () => {
        this.ui_.alertDialog.show(
            strf('OFFLINE_FAILURE_MESSAGE', unescape(currentEntry.name)),
            undefined, undefined);
      },
    };
    steps.start();
  }

  canExecute() {
    return this.metadataModel_.getCache(this.entries_, ['canPin'])
        .some(metadata => metadata.canPin);
  }

  getTitle() {
    return null;
  }

  getEntries() {
    return this.entries_;
  }
}


class DriveCreateFolderShortcutAction implements Action {
  constructor(
      private entry_: Entry|FilesAppEntry,
      private shortcutsModel_: FolderShortcutsDataModel,
      private onExecute_: VoidCallback) {}

  static create(
      entries: Array<Entry|FilesAppEntry>, volumeManager: VolumeManager,
      shortcutsModel: FolderShortcutsDataModel, onExecute: VoidCallback) {
    if (entries.length !== 1 || !isDirectoryEntry(entries[0]!)) {
      return null;
    }
    const locationInfo = volumeManager.getLocationInfo(entries[0]);
    if (!locationInfo || locationInfo.isSpecialSearchRoot ||
        locationInfo.isRootEntry) {
      return null;
    }
    return new DriveCreateFolderShortcutAction(
        entries[0], shortcutsModel, onExecute);
  }

  execute() {
    this.shortcutsModel_.add(this.entry_);
    this.onExecute_();
  }

  canExecute() {
    return !this.shortcutsModel_.exists(this.entry_);
  }

  getTitle() {
    return null;
  }

  getEntries() {
    return [this.entry_];
  }
}


class DriveRemoveFolderShortcutAction implements Action {
  constructor(
      private entry_: Entry|FilesAppEntry,
      private shortcutsModel_: FolderShortcutsDataModel,
      private onExecute_: VoidCallback) {}

  static create(
      entries: Array<Entry|FilesAppEntry>,
      shortcutsModel: FolderShortcutsDataModel, onExecute: VoidCallback) {
    if (entries.length !== 1 || !isDirectoryEntry(entries[0]!) ||
        !shortcutsModel.exists(entries[0])) {
      return null;
    }
    return new DriveRemoveFolderShortcutAction(
        entries[0], shortcutsModel, onExecute);
  }

  execute() {
    this.shortcutsModel_.remove(this.entry_);
    this.onExecute_();
  }

  canExecute() {
    return this.shortcutsModel_.exists(this.entry_);
  }

  getTitle() {
    return null;
  }

  getEntries() {
    return [this.entry_];
  }
}


/**
 * Opens the entry in Drive Web for the user to manage permissions etc.
 */
class DriveManageAction implements Action {
  /**
   * @param entry The entry to open the 'Manage' page for.
   */
  constructor(
      private entry_: Entry|FilesAppEntry,
      private volumeManager_: VolumeManager) {}

  /**
   * Creates a new DriveManageAction object.
   * |entries| must contain only a single entry.
   */
  static create(
      entries: Array<Entry|FilesAppEntry>, volumeManager: VolumeManager) {
    if (entries.length !== 1) {
      return null;
    }

    return new DriveManageAction(entries[0]!, volumeManager);
  }

  execute() {
    const props = [chrome.fileManagerPrivate.EntryPropertyName.ALTERNATE_URL];
    getEntryProperties([this.entry_], props).then((results) => {
      if (results.length !== 1) {
        console.warn(
            `getEntryProperties for alternateUrl should return 1 entry ` +
            `(returned ${results.length})`);
        return;
      }
      if (results[0]!.alternateUrl === undefined) {
        console.warn('getEntryProperties alternateUrl is undefined');
        return;
      }
      visitURL(results[0]!.alternateUrl);
    });
  }

  canExecute() {
    return this.volumeManager_.getDriveConnectionState().type !==
        chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE;
  }

  getTitle() {
    return null;
  }

  getEntries() {
    return [this.entry_];
  }
}


/**
 * A custom action set by the FSP API.
 */
class CustomAction implements Action {
  constructor(
      private entries_: Array<Entry|FilesAppEntry>, private id_: string,
      private title_: string|null, private onExecute_: VoidCallback) {}

  execute() {
    chrome.fileManagerPrivate.executeCustomAction(
        this.entries_.map(e => unwrapEntry(e)) as Entry[], this.id_, () => {
          if (chrome.runtime.lastError) {
            console.error(
                'Failed to execute a custom action because of: ' +
                chrome.runtime.lastError.message);
          }
          this.onExecute_();
        });
  }

  canExecute() {
    return true;  // Custom actions are always executable.
  }

  getTitle() {
    return this.title_;
  }

  getEntries() {
    return this.entries_;
  }
}

/**
 * Represents a set of actions for a set of entries. Includes actions set
 * locally in JS, as well as those retrieved from the FSP API.
 */
export class ActionsModel extends EventTarget {
  private actions_: ActionsMap = {};

  private initializePromiseReject_: VoidCallback|null = null;

  private initializePromise_: Promise<void>|null = null;

  private destroyed_ = false;

  constructor(
      private volumeManager_: VolumeManager,
      private metadataModel_: MetadataModel,
      private shortcutsModel_: FolderShortcutsDataModel,
      private ui_: ActionModelUi,
      private entries_: Array<Entry|FilesAppEntry>) {
    super();
  }

  /**
   * Initializes the ActionsModel, including populating the list of available
   * actions for the given entries.
   */
  initialize() {
    if (this.initializePromise_) {
      return this.initializePromise_;
    }

    this.initializePromise_ =
        new Promise((fulfill: (value: ActionsMap) => void, reject) => {
          if (this.destroyed_) {
            reject();
            return;
          }
          this.initializePromiseReject_ = reject;

          const volumeInfo = this.entries_.length >= 1 &&
              this.volumeManager_.getVolumeInfo(this.entries_[0]!);
          // All entries need to be on the same volume to execute ActionsModel
          // commands.
          if (!volumeInfo ||
              !isSameVolume(this.entries_, this.volumeManager_)) {
            fulfill({});
            return;
          }

          const actions: ActionsMap = {};
          switch (volumeInfo.volumeType) {
            // For Drive, actions are constructed directly in the Files app
            // code.
            case VolumeType.DRIVE:
              const saveForOfflineAction = DriveToggleOfflineAction.create(
                  this.entries_, this.metadataModel_, this.ui_, true,
                  this.invalidate_.bind(this));
              if (saveForOfflineAction) {
                actions[CommonActionId.SAVE_FOR_OFFLINE] = saveForOfflineAction;
              }

              const offlineNotNecessaryAction = DriveToggleOfflineAction.create(
                  this.entries_, this.metadataModel_, this.ui_, false,
                  this.invalidate_.bind(this));
              if (offlineNotNecessaryAction) {
                actions[CommonActionId.OFFLINE_NOT_NECESSARY] =
                    offlineNotNecessaryAction;
              }

              const createFolderShortcutAction =
                  DriveCreateFolderShortcutAction.create(
                      this.entries_, this.volumeManager_, this.shortcutsModel_,
                      this.invalidate_.bind(this));
              if (createFolderShortcutAction) {
                actions[InternalActionId.CREATE_FOLDER_SHORTCUT] =
                    createFolderShortcutAction;
              }

              const removeFolderShortcutAction =
                  DriveRemoveFolderShortcutAction.create(
                      this.entries_, this.shortcutsModel_,
                      this.invalidate_.bind(this));
              if (removeFolderShortcutAction) {
                actions[InternalActionId.REMOVE_FOLDER_SHORTCUT] =
                    removeFolderShortcutAction;
              }

              const manageInDriveAction =
                  DriveManageAction.create(this.entries_, this.volumeManager_);
              if (manageInDriveAction) {
                actions[InternalActionId.MANAGE_IN_DRIVE] = manageInDriveAction;
              }

              fulfill(actions);
              break;

            // For FSP, fetch custom actions via an API.
            case VolumeType.PROVIDED:
              chrome.fileManagerPrivate.getCustomActions(
                  this.entries_.map(e => unwrapEntry(e)) as Entry[],
                  (customActions: chrome.fileManagerPrivate
                       .FileSystemProviderAction[]) => {
                    if (chrome.runtime.lastError) {
                      console.warn(
                          'Failed to fetch custom actions because of: ' +
                          chrome.runtime.lastError.message);
                    } else {
                      customActions.forEach(action => {
                        // Skip fake actions that should not be displayed to the
                        // user, for example actions that just expose OneDrive
                        // URLs.
                        if (FSP_ACTIONS_HIDDEN.includes(action.id)) {
                          return;
                        }
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

  getActions() {
    return this.actions_;
  }

  getAction(id: string) {
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
   */
  private invalidate_() {
    if (this.initializePromiseReject_ !== null) {
      const reject = this.initializePromiseReject_;
      this.initializePromiseReject_ = null;
      this.initializePromise_ = null;
      reject();
    }
    dispatchSimpleEvent(this, 'invalidated', true);
  }

  getEntries() {
    return this.entries_;
  }
}

/**
 * List of common actions, used both internally and externally (custom actions).
 * Keep in sync with file_system_provider.idl.
 */
export enum CommonActionId {
  SHARE = 'SHARE',
  SAVE_FOR_OFFLINE = 'SAVE_FOR_OFFLINE',
  OFFLINE_NOT_NECESSARY = 'OFFLINE_NOT_NECESSARY',
}

export enum InternalActionId {
  CREATE_FOLDER_SHORTCUT = 'pin-folder',
  REMOVE_FOLDER_SHORTCUT = 'unpin-folder',
  MANAGE_IN_DRIVE = 'manage-in-drive',
}
