// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {getDlpRestrictionDetails, getHoldingSpaceState, startIOTask} from '../../common/js/api.js';
import {isModal} from '../../common/js/dialog_type.js';
import {getFocusedTreeItem, isDirectoryTree, isDirectoryTreeItem} from '../../common/js/dom_utils.js';
import {entriesToURLs, isFakeEntry, isGrandRootEntryInDrives, isNonModifiable, isRecentRootType, isTeamDriveRoot, isTeamDrivesGrandRoot, isTrashEntry, isTrashRoot, unwrapEntry} from '../../common/js/entry_utils.js';
import {getExtension, getType, isEncrypted} from '../../common/js/file_type.js';
import {EntryList} from '../../common/js/files_app_entry_types.js';
import {isDlpEnabled, isDriveFsBulkPinningEnabled, isMirrorSyncEnabled, isNewDirectoryTreeEnabled, isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {recordEnum, recordUserAction} from '../../common/js/metrics.js';
import {getFileErrorString, str, strf} from '../../common/js/translations.js';
import {deleteIsForever, RestoreFailedType, RestoreFailedTypesUMA, RestoreFailedUMA, shouldMoveToTrash, TrashEntry} from '../../common/js/trash.js';
import {visitURL} from '../../common/js/util.js';
import {FileSystemType, isRecentArcEntry, RootType, VolumeError, VolumeType} from '../../common/js/volume_manager_types.js';
import {CommandHandlerDeps} from '../../externs/command_handler_deps.js';
import {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {DialogType, State} from '../../externs/ts/state.js';
import {readSubDirectories} from '../../state/ducks/all_entries.js';
import {changeDirectory} from '../../state/ducks/current_directory.js';
import {getStore} from '../../state/store.js';
import {XfTreeItem} from '../../widgets/xf_tree_item.js';

import {CommonActionId, InternalActionId} from './actions_model.js';
import {MenuCommandsForUma, recordMenuItemSelected} from './command_handler.js';
import {canExecuteVisibleOnDriveInNormalAppModeOnly, containsNonInteractiveEntry, currentVolumeIsInteractive, getCommandEntries, getCommandEntry, getElementVolumeInfo, getEventEntry, getOnlyOneSelectedDirectory, getParentEntry, getSharesheetLaunchSource, hasCapability, isDriveEntries, isFromSelectionMenu, isOnlyMyDriveEntries, isOnTrashRoot, isRootEntry, shouldIgnoreEvents, shouldShowMenuItemsForEntry} from './file_manager_commands_util.js';
import {HoldingSpaceUtil} from './holding_space_util.js';
import {PathComponent} from './path_component.js';
import {Command} from './ui/command.js';
import {DirectoryItem, DirectoryTree} from './ui/directory_tree.js';
import {FilesConfirmDialog} from './ui/files_confirm_dialog.js';

/**
 * TODO(TS): Remove when converting to TS.
 *
 * @param {!Event} event
 */
function getCommand(event) {
  return /** @type {import('ui/command.js').CommandEvent} */ (event)
      .detail.command;
}

/**
 * A command.
 * @abstract
 */
class FilesCommand {
  /**
   * Handles the execute event.
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
   * @abstract
   */
  // @ts-ignore: error TS6133: 'fileManager' is declared but its value is never
  // read.
  execute(event, fileManager) {}

  /**
   * Handles the can execute event.
   * By default, sets the command as always enabled.
   * @param {!Event} event Can execute event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
   */
  // @ts-ignore: error TS6133: 'fileManager' is declared but its value is never
  // read.
  canExecute(event, fileManager) {
    // @ts-ignore: error TS2339: Property 'canExecute' does not exist on type
    // 'Event'.
    event.canExecute = true;
  }
}

/**
 * Unmounts external drive.
 */
export class UnmountCommand extends FilesCommand {
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
   * @private
   */
  async executeImpl_(event, fileManager) {
    /** @param {VolumeType=} opt_volumeType */
    const errorCallback = opt_volumeType => {
      if (opt_volumeType === VolumeType.REMOVABLE) {
        fileManager.ui.alertDialog.showHtml(
            '', str('UNMOUNT_FAILED'), null, null, null);
      } else {
        fileManager.ui.alertDialog.showHtml(
            '', str('UNMOUNT_PROVIDED_FAILED'), null, null, null);
      }
    };

    // Find volumes to unmount.
    let volumes = [];
    let label = '';
    // @ts-ignore: error TS2345: Argument of type 'EventTarget | null' is not
    // assignable to parameter of type 'EventTarget'.
    const entry = getCommandEntry(fileManager, event.target);
    if (entry instanceof EntryList) {
      // The element is a group of removable partitions.
      if (!entry) {
        errorCallback();
        return;
      }
      // Add child partitions to the list of volumes to be unmounted.
      // @ts-ignore: error TS2339: Property 'volumeInfo' does not exist on type
      // 'FileSystemEntry | FilesAppEntry'.
      volumes = entry.getUiChildren().map(child => child.volumeInfo);
      label = entry.label || '';
    } else {
      // The element is a removable volume with no partitions.
      const volumeInfo =
          // @ts-ignore: error TS2345: Argument of type 'EventTarget | null' is
          // not assignable to parameter of type 'EventTarget'.
          getElementVolumeInfo(event.target, fileManager);
      if (!volumeInfo) {
        errorCallback();
        return;
      }
      volumes.push(volumeInfo);
      label = volumeInfo.label || '';
    }

    // Eject volumes of which there may be multiple.
    const promises = volumes.map(async (volume) => {
      try {
        await fileManager.volumeManager.unmount(volume);
      } catch (error) {
        console.warn('Cannot unmount (redacted):', error);
        console.debug(`Cannot unmount '${volume.volumeId}':`, error);
        if (error != VolumeError.PATH_NOT_MOUNTED) {
          errorCallback(volume.volumeType);
        }
      }
    });

    await Promise.all(promises);
    fileManager.ui.speakA11yMessage(strf('A11Y_VOLUME_EJECT', label));
  }

  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    this.executeImpl_(event, fileManager);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const volumeInfo = getElementVolumeInfo(event.target, fileManager);
    const entry = getCommandEntry(fileManager, event.target);

    let volumeType;
    if (entry && entry instanceof EntryList) {
      volumeType = entry.rootType;
    } else if (volumeInfo) {
      volumeType = volumeInfo.volumeType;
    } else {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    event.canExecute =
        (volumeType === VolumeType.ARCHIVE ||
         volumeType === VolumeType.REMOVABLE ||
         volumeType === VolumeType.PROVIDED || volumeType === VolumeType.SMB);
    event.command.setHidden(!event.canExecute);

    switch (volumeType) {
      case VolumeType.ARCHIVE:
      case VolumeType.PROVIDED:
      case VolumeType.SMB:
        event.command.label = str('CLOSE_VOLUME_BUTTON_LABEL');
        break;
      case VolumeType.REMOVABLE:
        event.command.label = str('UNMOUNT_DEVICE_BUTTON_LABEL');
        break;
    }
  }
}

/**
 * Formats external drive.
 */
export class FormatCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const directoryModel = fileManager.directoryModel;
    let root;
    if (fileManager.ui.directoryTree.contains(
            /** @type {Node} */ (event.target))) {
      // The command is executed from the directory tree context menu.
      root = getCommandEntry(fileManager, event.target);
    } else {
      // The command is executed from the gear menu.
      root = directoryModel.getCurrentDirEntry();
    }
    // If an entry is not found from the event target, use the current
    // directory. This can happen for the format button for unsupported and
    // unrecognized volumes.
    if (!root) {
      root = directoryModel.getCurrentDirEntry();
    }

    const volumeInfo = fileManager.volumeManager.getVolumeInfo(assert(root));
    if (volumeInfo) {
      fileManager.ui.formatDialog.showModal(volumeInfo);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const directoryModel = fileManager.directoryModel;
    let root;
    if (fileManager.ui.directoryTree.contains(
            /** @type {Node} */ (event.target))) {
      // The command is executed from the directory tree context menu.
      root = getCommandEntry(fileManager, event.target);
    } else {
      // The command is executed from the gear menu.
      root = directoryModel.getCurrentDirEntry();
    }

    // |root| is null for unrecognized volumes. Enable format command for such
    // volumes.
    const isUnrecognizedVolume = (root == null);
    // See the comment in execute() for why doing this.
    if (!root) {
      root = directoryModel.getCurrentDirEntry();
    }
    const location = root && fileManager.volumeManager.getLocationInfo(root);
    const writable = location && !location.isReadOnly;
    const isRoot = location && location.isRootEntry;

    // Enable the command if this is a removable device (e.g. a USB drive).
    const removableRoot =
        location && isRoot && location.rootType === RootType.REMOVABLE;
    event.canExecute = removableRoot && (isUnrecognizedVolume || writable);

    if (isSinglePartitionFormatEnabled()) {
      let isDevice = false;
      if (root && root instanceof EntryList) {
        // root entry is device node if it has child (partition).
        isDevice = !!removableRoot && root.getUiChildren().length > 0;
      }
      // Disable format command on device when SinglePartitionFormat on,
      // erase command will be available.
      event.command.setHidden(!removableRoot || isDevice);
    } else {
      event.command.setHidden(!removableRoot);
    }
  }
}

/**
 * Deletes removable device partition, creates single partition and formats it.
 */
export class EraseDeviceCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const root = getEventEntry(event, fileManager);

    if (root && root instanceof EntryList) {
      // @ts-ignore: error TS2304: Cannot find name 'FilesFormatDialog'.
      /** @type {FilesFormatDialog} */ (fileManager.ui.formatDialog)
          .showEraseModal(root);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    if (!isSinglePartitionFormatEnabled()) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const root = getEventEntry(event, fileManager);
    const location = root && fileManager.volumeManager.getLocationInfo(root);
    const writable = location && !location.isReadOnly;
    const isRoot = location && location.isRootEntry;

    const removableRoot =
        location && isRoot && location.rootType === RootType.REMOVABLE;

    let isDevice = false;
    if (root && root instanceof EntryList) {
      // root entry is device node if it has child (partition).
      isDevice = !!removableRoot && root.getUiChildren().length > 0;
    }

    event.canExecute = removableRoot && !writable;
    // Enable the command if this is a removable and device node.
    event.command.setHidden(!removableRoot || !isDevice);
  }
}

/**
 * Initiates new folder creation.
 */
export class NewFolderCommand extends FilesCommand {
  constructor() {
    super();

    /**
     * Whether a new-folder is in progress.
     * @private @type {boolean}
     */
    this.busy_ = false;
  }

  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    // @ts-ignore: error TS7034: Variable 'targetDirectory' implicitly has type
    // 'any' in some locations where its type cannot be determined.
    let targetDirectory;
    // @ts-ignore: error TS7034: Variable 'executedFromDirectoryTree' implicitly
    // has type 'any' in some locations where its type cannot be determined.
    let executedFromDirectoryTree;

    if (isDirectoryTree(event.target)) {
      // @ts-ignore: error TS2339: Property 'entry' does not exist on type
      // 'XfTreeItem | DirectoryItem'.
      targetDirectory = getFocusedTreeItem(event.target)?.entry;
      executedFromDirectoryTree = true;
    } else if (isDirectoryTreeItem(event.target)) {
      targetDirectory = event.target.entry;
      executedFromDirectoryTree = true;
    } else {
      targetDirectory = fileManager.directoryModel.getCurrentDirEntry();
      executedFromDirectoryTree = false;
    }

    const directoryModel = fileManager.directoryModel;
    const listContainer = fileManager.ui.listContainer;
    this.busy_ = true;

    // @ts-ignore: error TS7006: Parameter 'newName' implicitly has an 'any'
    // type.
    this.generateNewDirectoryName_(targetDirectory).then((newName) => {
      // @ts-ignore: error TS7005: Variable 'executedFromDirectoryTree'
      // implicitly has an 'any' type.
      if (!executedFromDirectoryTree) {
        listContainer.startBatchUpdates();
      }

      return new Promise(
                 // @ts-ignore: error TS7005: Variable 'targetDirectory'
                 // implicitly has an 'any' type.
                 targetDirectory.getDirectory.bind(
                     // @ts-ignore: error TS7005: Variable 'targetDirectory'
                     // implicitly has an 'any' type.
                     targetDirectory, newName, {create: true, exclusive: true}))
          .then(
              (newDirectory) => {
                recordUserAction('CreateNewFolder');

                // Select new directory and start rename operation.
                // @ts-ignore: error TS7005: Variable
                // 'executedFromDirectoryTree' implicitly has an 'any' type.
                if (executedFromDirectoryTree) {
                  if (isNewDirectoryTreeEnabled()) {
                    // After new directory is created on parent directory, we
                    // need to trigger a re-read for the parent directory to the
                    // store.
                    // @ts-ignore: error TS7005: Variable 'targetDirectory'
                    // implicitly has an 'any' type.
                    getStore().dispatch(readSubDirectories(targetDirectory));
                    fileManager.ui.directoryTreeContainer
                        .renameItemWithKeyWhenRendered(newDirectory.toURL());
                  } else {
                    const directoryTree =
                        /** @type {DirectoryTree} */ (
                            fileManager.ui.directoryTree);
                    directoryTree.updateAndSelectNewDirectory(
                        // @ts-ignore: error TS7005: Variable 'targetDirectory'
                        // implicitly has an 'any' type.
                        targetDirectory, newDirectory);
                    fileManager.directoryTreeNamingController.attachAndStart(
                        assert(directoryTree.selectedItem), false, null);
                  }
                  this.busy_ = false;
                } else {
                  directoryModel.updateAndSelectNewDirectory(newDirectory)
                      .then(() => {
                        listContainer.endBatchUpdates();
                        fileManager.namingController.initiateRename();
                        this.busy_ = false;
                      })
                      // @ts-ignore: error TS7006: Parameter 'error' implicitly
                      // has an 'any' type.
                      .catch(error => {
                        listContainer.endBatchUpdates();
                        this.busy_ = false;
                        console.warn(error);
                      });
                }
              },
              (error) => {
                // @ts-ignore: error TS7005: Variable
                // 'executedFromDirectoryTree' implicitly has an 'any' type.
                if (!executedFromDirectoryTree) {
                  listContainer.endBatchUpdates();
                }

                this.busy_ = false;

                fileManager.ui.alertDialog.show(
                    strf(
                        'ERROR_CREATING_FOLDER', newName,
                        getFileErrorString(error.name)),
                    null, null);
              });
    });
  }

  /**
   * Generates new directory name.
   * @param {!DirectoryEntry} parentDirectory
   * @param {number=} opt_index
   * @private
   */
  // @ts-ignore: error TS7023: 'generateNewDirectoryName_' implicitly has return
  // type 'any' because it does not have a return type annotation and is
  // referenced directly or indirectly in one of its return expressions.
  generateNewDirectoryName_(parentDirectory, opt_index) {
    const index = opt_index || 0;

    const defaultName = str('DEFAULT_NEW_FOLDER_NAME');
    const newName =
        index === 0 ? defaultName : defaultName + ' (' + index + ')';

    return new Promise(parentDirectory.getDirectory.bind(
                           parentDirectory, newName, {create: false}))
        // @ts-ignore: error TS6133: 'newEntry' is declared but its value is
        // never read.
        .then(newEntry => {
          return this.generateNewDirectoryName_(parentDirectory, index + 1);
        })
        .catch(() => {
          return newName;
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const entries = getCommandEntries(fileManager, event.target);
    // If there is a selected entry on a non-interactive volume, remove
    // new-folder command.
    if (entries.length > 0 &&
        !containsNonInteractiveEntry(entries, fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    if (isDirectoryTree(event.target) || isDirectoryTreeItem(event.target)) {
      const entry = entries[0];
      if (!entry || isFakeEntry(entry) || isTeamDrivesGrandRoot(entry)) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }

      const locationInfo = fileManager.volumeManager.getLocationInfo(entry);
      event.canExecute = locationInfo && !locationInfo.isReadOnly &&
          hasCapability(fileManager, [entry], 'canAddChildren');
      event.command.setHidden(false);
    } else {
      // If blank space was clicked and current volume is non-interactive,
      // remove new-folder command.
      // @ts-ignore: error TS2367: This comparison appears to be unintentional
      // because the types 'FileSystemEntry[]' and 'number' have no overlap.
      if (entries.length === 0 && !currentVolumeIsInteractive(fileManager)) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }
      const directoryModel = fileManager.directoryModel;
      const directoryEntry = fileManager.getCurrentDirectoryEntry();
      event.canExecute = !fileManager.directoryModel.isReadOnly() &&
          !fileManager.namingController.isRenamingInProgress() &&
          !directoryModel.isSearching() &&
          hasCapability(fileManager, [directoryEntry], 'canAddChildren');
      event.command.setHidden(false);
    }
    if (this.busy_) {
      event.canExecute = false;
    }
  }
}

/**
 * Initiates new window creation.
 */
export class NewWindowCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    fileManager.launchFileManager({
      currentDirectoryURL: fileManager.getCurrentDirectoryEntry() &&
          fileManager.getCurrentDirectoryEntry().toURL(),
    });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    event.canExecute = fileManager.getCurrentDirectoryEntry() &&
        (fileManager.dialogType === DialogType.FULL_PAGE);
  }
}

export class SelectAllCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    fileManager.directoryModel.getFileListSelection().setCheckSelectMode(true);
    fileManager.directoryModel.getFileListSelection().selectAll();
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    // Check we can select multiple items.
    const multipleSelect =
        fileManager.directoryModel.getFileListSelection().multiple;
    // Check we are not inside an input element (e.g. the search box).
    const inputElementActive =
        document.activeElement instanceof HTMLInputElement ||
        document.activeElement instanceof HTMLTextAreaElement ||
        // @ts-ignore: error TS18047: 'document.activeElement' is possibly
        // 'null'.
        document.activeElement.tagName.toLowerCase() === 'cr-input';
    event.canExecute = multipleSelect && !inputElementActive &&
        fileManager.directoryModel.getFileList().length > 0;
  }
}

export class ToggleHiddenFilesCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    const visible = !fileManager.fileFilter.isHiddenFilesVisible();
    fileManager.fileFilter.setHiddenFilesVisible(visible);
    const command = getCommand(event);
    command.checked = visible;  // Check-mark for "Show hidden files".
    recordMenuItemSelected(
        visible ? MenuCommandsForUma.HIDDEN_FILES_SHOW :
                  MenuCommandsForUma.HIDDEN_FILES_HIDE);
  }
}

/**
 * Toggles visibility of top-level Android folders which are not visible by
 * default.
 */
export class ToggleHiddenAndroidFoldersCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    const visible = !fileManager.fileFilter.isAllAndroidFoldersVisible();
    fileManager.fileFilter.setAllAndroidFoldersVisible(visible);
    const command = getCommand(event);
    command.checked = visible;
    recordMenuItemSelected(
        visible ? MenuCommandsForUma.HIDDEN_ANDROID_FOLDERS_SHOW :
                  MenuCommandsForUma.HIDDEN_ANDROID_FOLDERS_HIDE);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    const hasAndroidFilesVolumeInfo =
        !!fileManager.volumeManager.getCurrentProfileVolumeInfo(
            VolumeType.ANDROID_FILES);
    const currentRootType = fileManager.directoryModel.getCurrentRootType();
    const isInMyFiles = currentRootType == RootType.MY_FILES ||
        currentRootType == RootType.DOWNLOADS ||
        currentRootType == RootType.CROSTINI ||
        currentRootType == RootType.ANDROID_FILES;
    event.canExecute = hasAndroidFilesVolumeInfo && isInMyFiles;
    event.command.setHidden(!event.canExecute);
    event.command.checked = fileManager.fileFilter.isAllAndroidFoldersVisible();
  }
}

/**
 * Toggles drive sync settings.
 */
export class DriveSyncSettingsCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    const nowDriveSyncEnabledOnMeteredNetwork =
        fileManager.ui.gearMenu.syncButton.hasAttribute('checked');
    const changeInfo = {
      driveSyncEnabledOnMeteredNetwork: !nowDriveSyncEnabledOnMeteredNetwork,
    };
    chrome.fileManagerPrivate.setPreferences(changeInfo);
    recordMenuItemSelected(
        nowDriveSyncEnabledOnMeteredNetwork ?
            MenuCommandsForUma.MOBILE_DATA_ON :
            MenuCommandsForUma.MOBILE_DATA_OFF);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    event.canExecute = fileManager.directoryModel.isOnDrive();
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Delete / Move to Trash command.
 */
export class DeleteCommand extends FilesCommand {
  /**
   * @param {Event} event
   * @param {!CommandHandlerDeps} fileManager
   * @override
   */
  execute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const command = getCommand(event);
    const permanentlyDelete = command.id === 'delete';

    // Execute might be called without a call of canExecute method, e.g.,
    // called directly from code, crbug.com/509483. See toolbar controller
    // delete button handling, for an example.
    this.deleteEntries(entries, fileManager, permanentlyDelete);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);

    // If entries contain fake, non-interactive or root entry, remove delete
    // option.
    if (!entries.every(shouldShowMenuItemsForEntry.bind(
            null, fileManager.volumeManager))) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    // Block fusebox volumes in SelectFileAsh (Lacros) file picker mode.
    if (fileManager.volumeManager.getFuseBoxOnlyFilterEnabled()) {
      // TODO(crbug/1292825) Make it work with fusebox volumes: MTP, etc.
      if (fileManager.directoryModel.isOnFuseBox()) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }
    }

    event.canExecute = this.canDeleteEntries_(entries, fileManager);

    // Remove if nothing is selected, e.g. user clicked in an empty
    // space in the file list.
    const noEntries = entries.length === 0;
    event.command.setHidden(noEntries);

    const isTrashDisabled =
        !shouldMoveToTrash(entries, fileManager.volumeManager) ||
        !fileManager.trashEnabled;

    if (event.command.id === 'move-to-trash' && isTrashDisabled) {
      event.canExecute = false;
      event.command.setHidden(true);
    }

    // If the "move-to-trash" command is enabled, don't show the Delete command
    // but still leave it executable.
    if (event.command.id === 'delete' && !isTrashDisabled) {
      event.command.setHidden(true);
    }
  }

  /**
   * Delete the entries (if the entries can be deleted).
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @param {boolean} permanentlyDelete if true, entries are permanently deleted
   *     rather than moved to trash.
   * @param {?FilesConfirmDialog} dialog An optional delete confirm dialog.
   *    The default delete confirm dialog will be used if |dialog| is null.
   * @public
   */
  deleteEntries(entries, fileManager, permanentlyDelete, dialog = null) {
    // Verify that the entries are not fake, non-interactive or root entries,
    // and that they can be deleted.
    if (!entries.every(shouldShowMenuItemsForEntry.bind(
            null, fileManager.volumeManager)) ||
        !this.canDeleteEntries_(entries, fileManager)) {
      return;
    }

    // Trashing an item shows an "Undo" visual signal instead of a confirmation
    // dialog.
    if (!permanentlyDelete &&
        shouldMoveToTrash(entries, fileManager.volumeManager) &&
        fileManager.trashEnabled) {
      startIOTask(
          chrome.fileManagerPrivate.IOTaskType.TRASH, entries,
          /*params=*/ {});
      return;
    }

    if (!dialog) {
      dialog = fileManager.ui.deleteConfirmDialog;
    } else if (dialog.showModalElement) {
      dialog.showModalElement();
    }

    const dialogDoneCallback = () => {
      // @ts-ignore: error TS18047: 'dialog' is possibly 'null'.
      dialog.doneCallback && dialog.doneCallback();
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      document.querySelector('files-tooltip').hideTooltip();
    };

    const deleteAction = () => {
      dialogDoneCallback();
      // Start the permanent delete.
      startIOTask(
          chrome.fileManagerPrivate.IOTaskType.DELETE, entries, /*params=*/ {});
    };

    const cancelAction = () => {
      dialogDoneCallback();
    };

    // Files that are deleted from locations that are trash enabled (except
    // Drive) should instead show copy indicating the files will be permanently
    // deleted. For all other filesystem the permanent deletion can't
    // necessarily be verified (e.g. a copy may be moved to the underlying
    // filesystems version of trash).
    if (deleteIsForever(entries, fileManager.volumeManager)) {
      const title = entries.length === 1 ?
          // @ts-ignore: error TS2555: Expected at least 2 arguments, but got 1.
          strf('CONFIRM_PERMANENTLY_DELETE_ONE_TITLE') :
          // @ts-ignore: error TS2555: Expected at least 2 arguments, but got 1.
          strf('CONFIRM_PERMANENTLY_DELETE_SOME_TITLE');

      const message = entries.length === 1 ?
          // @ts-ignore: error TS2532: Object is possibly 'undefined'.
          strf('CONFIRM_PERMANENTLY_DELETE_ONE_DESC', entries[0].name) :
          strf('CONFIRM_PERMANENTLY_DELETE_SOME_DESC', entries.length);

      dialog.setOkLabel(str('PERMANENTLY_DELETE_FOREVER'));
      dialog.showWithTitle(title, message, deleteAction, cancelAction, null);
      return;
    }

    const deleteMessage = entries.length === 1 ?
        // @ts-ignore: error TS2532: Object is possibly 'undefined'.
        strf('CONFIRM_DELETE_ONE', entries[0].name) :
        strf('CONFIRM_DELETE_SOME', entries.length);
    dialog.setOkLabel(str('DELETE_BUTTON_LABEL'));
    // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
    // parameter of type 'Function | undefined'.
    dialog.show(deleteMessage, deleteAction, cancelAction, null);
  }

  /**
   * Returns true if all entries can be deleted. Note: This does not check for
   * root or fake entries.
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @return {boolean}
   * @private
   */
  canDeleteEntries_(entries, fileManager) {
    return entries.length > 0 &&
        !this.containsReadOnlyEntry_(entries, fileManager) &&
        fileManager.directoryModel.canDeleteEntries() &&
        hasCapability(fileManager, entries, 'canDelete');
  }

  /**
   * Returns True if entries can be deleted.
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @return {boolean}
   * @public
   */
  canDeleteEntries(entries, fileManager) {
    // Verify that the entries are not fake, non-interactive or root entries,
    // and that they can be deleted.
    if (!entries.every(shouldShowMenuItemsForEntry.bind(
            null, fileManager.volumeManager)) ||
        !this.canDeleteEntries_(entries, fileManager)) {
      return false;
    }

    return true;
  }

  /**
   * Returns true if any entry belongs to a read-only volume or is
   * forced to be read-only like MyFiles>Downloads.
   * @param {!Array<!Entry|!FilesAppEntry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @return {boolean}
   * @private
   */
  containsReadOnlyEntry_(entries, fileManager) {
    return entries.some(entry => {
      const locationInfo = fileManager.volumeManager.getLocationInfo(entry);
      return (locationInfo && locationInfo.isReadOnly) ||
          isNonModifiable(fileManager.volumeManager, entry);
    });
  }
}

/**
 * Restores selected files from trash.
 */
export class RestoreFromTrashCommand extends FilesCommand {
  /** @private */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  async execute_(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);

    const infoEntries = [];
    // @ts-ignore: error TS7034: Variable 'failedParents' implicitly has type
    // 'any[]' in some locations where its type cannot be determined.
    const failedParents = [];
    for (const e of entries) {
      const entry = /** @type {!TrashEntry} */ (e);
      try {
        const {exists, parentName} = await this.getParentName(
            entry.restoreEntry, fileManager.volumeManager);
        if (!exists) {
          failedParents.push({fileName: entry.restoreEntry.name, parentName});
        } else {
          infoEntries.push(entry.infoEntry);
        }
      } catch (err) {
        console.warn('Failed getting parent metadata for:', err);
      }
    }
    if (failedParents && failedParents.length > 0) {
      // Only a single item is being trashed and the parent doesn't exist.
      if (failedParents.length === 1 && infoEntries.length === 0) {
        recordEnum(
            RestoreFailedUMA, RestoreFailedType.SINGLE_ITEM,
            RestoreFailedTypesUMA);
        fileManager.ui.alertDialog.show(
            // @ts-ignore: error TS2532: Object is possibly 'undefined'.
            strf('CANT_RESTORE_SINGLE_ITEM', failedParents[0].parentName));
        return;
      }
      // More than one item has been trashed but all the items have their
      // parent removed.
      if (failedParents.length > 1 && infoEntries.length === 0) {
        const isParentFolderSame = failedParents.every(
            // @ts-ignore: error TS7005: Variable 'failedParents' implicitly has
            // an 'any[]' type.
            p => p.parentName === failedParents[0].parentName);
        // All the items were from the same parent folder.
        if (isParentFolderSame) {
          recordEnum(
              RestoreFailedUMA, RestoreFailedType.MULTIPLE_ITEMS_SAME_PARENTS,
              RestoreFailedTypesUMA);
          fileManager.ui.alertDialog.show(strf(
              'CANT_RESTORE_MULTIPLE_ITEMS_SAME_PARENTS',
              // @ts-ignore: error TS2532: Object is possibly 'undefined'.
              failedParents[0].parentName));
          return;
        }
        // All the items are from different parent folders.
        recordEnum(
            RestoreFailedUMA,
            RestoreFailedType.MULTIPLE_ITEMS_DIFFERENT_PARENTS,
            RestoreFailedTypesUMA);
        fileManager.ui.alertDialog.show(
            str('CANT_RESTORE_MULTIPLE_ITEMS_DIFFERENT_PARENTS'));
        return;
      }
      // A mix of items with parents and without parents are attempting to be
      // restored.
      recordEnum(
          RestoreFailedUMA, RestoreFailedType.MULTIPLE_ITEMS_MIXED,
          RestoreFailedTypesUMA);
      fileManager.ui.alertDialog.show(str('CANT_RESTORE_SOME_ITEMS'));
      return;
    }
    startIOTask(
        chrome.fileManagerPrivate.IOTaskType.RESTORE, infoEntries,
        /*params=*/ {});
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    this.execute_(event, fileManager);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);

    const enabled = entries.length > 0 && entries.every(e => isTrashEntry(e)) &&
        fileManager.trashEnabled;
    event.canExecute = enabled;
    event.command.setHidden(!enabled);
  }

  /**
   * Check whether the parent exists from a supplied entry and return the folder
   * name (if it exists or doesn't).
   * @param {!Entry} entry The entry to identify the parent from.
   * @param {!import('../../externs/volume_manager.js').VolumeManager}
   *     volumeManager
   * @returns {Promise<{exists: boolean, parentName: string}>}
   */
  async getParentName(entry, volumeManager) {
    return new Promise((resolve, reject) => {
      entry.getParent(
          parent => resolve({exists: true, parentName: parent.name}), err => {
            // If this failed, it may be because the parent doesn't exist.
            // Extract the parent from the path components in that case.
            if (err.name === 'NotFoundError') {
              const components = PathComponent.computeComponentsFromEntry(
                  entry, volumeManager);
              resolve({
                exists: false,
                // @ts-ignore: error TS2532: Object is possibly 'undefined'.
                parentName: components[components.length - 2].name,
              });
              return;
            }
            reject(err);
          });
    });
  }
}

/**
 * Empties (permanently deletes all) files from trash.
 */
export class EmptyTrashCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    fileManager.ui.emptyTrashConfirmDialog.showWithTitle(
        str('CONFIRM_EMPTY_TRASH_TITLE'), str('CONFIRM_EMPTY_TRASH_DESC'),
        () => {
          startIOTask(
              chrome.fileManagerPrivate.IOTaskType.EMPTY_TRASH, /*entries=*/[],
              /*params=*/ {});
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined'
    // is not assignable to parameter of type 'FileSystemEntry | FilesAppEntry'.
    const trashRoot = entries.length === 1 && isTrashRoot(entries[0]) &&
        fileManager.trashEnabled;
    event.canExecute = trashRoot || isOnTrashRoot(fileManager);
    event.command.setHidden(!trashRoot);
  }
}

/**
 * Pastes files from clipboard.
 */
export class PasteCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    fileManager.document.execCommand(getCommand(event).id);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const fileTransferController = fileManager.fileTransferController;

    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(
            fileManager.directoryModel.getCurrentDirEntry());

    // Hide this command if only one folder is selected.
    event.command.setHidden(
        !!getOnlyOneSelectedDirectory(fileManager.getSelection()));

    const entries = getCommandEntries(fileManager, event.target);
    // If there is a selected entry on a non-interactive volume, remove paste
    // command.
    if (entries.length > 0 &&
        !containsNonInteractiveEntry(entries, fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    } else if (
        // @ts-ignore: error TS2367: This comparison appears to be unintentional
        // because the types 'FileSystemEntry[]' and 'number' have no overlap.
        entries.length === 0 && !currentVolumeIsInteractive(fileManager)) {
      // If blank space was clicked and current volume is non-interactive,
      // remove paste command.
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
  }
}

/**
 * Pastes files from clipboard. This is basically same as 'paste'.
 * This command is used for always showing the Paste command to gear menu.
 */
export class PasteIntoCurrentFolderCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    fileManager.document.execCommand('paste');
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    const fileTransferController = fileManager.fileTransferController;

    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(
            fileManager.directoryModel.getCurrentDirEntry());
  }
}

/**
 * Pastes files from clipboard into the selected folder.
 */
export class PasteIntoFolderCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    const entries = getCommandEntries(fileManager, event.target);
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    if (entries.length !== 1 || !entries[0].isDirectory ||
        !shouldShowMenuItemsForEntry(
            // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
            // undefined' is not assignable to parameter of type
            // 'FileSystemEntry | FakeEntry'.
            fileManager.volumeManager, entries[0])) {
      return;
    }

    // This handler tweaks the Event object for 'paste' event so that
    // the FileTransferController can distinguish this 'paste-into-folder'
    // command and know the destination directory.
    // @ts-ignore: error TS7006: Parameter 'inEvent' implicitly has an 'any'
    // type.
    const handler = inEvent => {
      inEvent.destDirectory = entries[0];
    };
    fileManager.document.addEventListener('paste', handler, true);
    fileManager.document.execCommand('paste');
    fileManager.document.removeEventListener('paste', handler, true);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const entries = getCommandEntries(fileManager, event.target);

    // Show this item only when one directory is selected.
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    if (entries.length !== 1 || !entries[0].isDirectory ||
        !shouldShowMenuItemsForEntry(
            // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
            // undefined' is not assignable to parameter of type
            // 'FileSystemEntry | FakeEntry'.
            fileManager.volumeManager, entries[0])) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const fileTransferController = fileManager.fileTransferController;
    const directoryEntry =
        /** @type {DirectoryEntry|FakeEntry} */ (entries[0]);
    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(directoryEntry);
    event.command.setHidden(false);
  }
}

/**
 * Cut/Copy command.
 * @private @const @type {FilesCommand}
 */
export class CutCopyCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    // Cancel check-select-mode on cut/copy.  Any further selection of a dir
    // should start a new selection rather than add to the existing selection.
    fileManager.directoryModel.getFileListSelection().setCheckSelectMode(false);
    fileManager.document.execCommand(getCommand(event).id);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const fileTransferController = fileManager.fileTransferController;

    if (!fileTransferController) {
      // File Open and SaveAs dialogs do not have a fileTransferController.
      event.command.setHidden(true);
      event.canExecute = false;
      return;
    }

    const command = event.command;
    const isMove = command.id === 'cut';
    // Disable Copy command in Trash.
    if (!isMove && isOnTrashRoot(fileManager)) {
      event.command.setHidden(true);
      event.canExecute = false;
      return;
    }

    const entries = getCommandEntries(fileManager, event.target);
    const target = event.target;
    const volumeManager = fileManager.volumeManager;
    command.setHidden(false);

    /** @returns {boolean} If the operation is allowed in the Directory Tree. */
    function canDoDirectoryTree() {
      let entry;
      if (target.entry) {
        entry = target.entry;
        // @ts-ignore: error TS2339: Property 'entry' does not exist on type
        // 'XfTreeItem | DirectoryItem'.
      } else if (getFocusedTreeItem(target)?.entry) {
        // @ts-ignore: error TS2339: Property 'entry' does not exist on type
        // 'XfTreeItem | DirectoryItem'.
        entry = getFocusedTreeItem(target).entry;
      } else {
        return false;
      }

      // If entry is fake, non-interactive or root, remove cut/copy option.
      if (!shouldShowMenuItemsForEntry(volumeManager, entry)) {
        command.setHidden(true);
        return false;
      }

      // For MyFiles/Downloads and MyFiles/PluginVm we only allow copy.
      if (isMove && isNonModifiable(volumeManager, entry)) {
        return false;
      }

      // Cut is unavailable on Shared Drive roots.
      if (isTeamDriveRoot(entry)) {
        return false;
      }

      const metadata =
          fileManager.metadataModel.getCache([entry], ['canCopy', 'canDelete']);
      assert(metadata.length === 1);

      if (!isMove) {
        return metadata[0].canCopy !== false;
      }

      // We need to check source volume is writable for move operation.
      const volumeInfo = volumeManager.getVolumeInfo(entry);
      return !volumeInfo.isReadOnly && metadata[0].canCopy !== false &&
          metadata[0].canDelete !== false;
    }

    /** @returns {boolean} If the operation is allowed in the File List. */
    function canDoFileList() {
      if (shouldIgnoreEvents(assert(fileManager.document))) {
        return false;
      }

      // If entries contain fake, non-interactive or root entry, remove cut/copy
      // option.
      if (!fileManager.getSelection().entries.every(
              shouldShowMenuItemsForEntry.bind(null, volumeManager))) {
        command.setHidden(true);
        return false;
      }

      // If blank space was clicked and current volume is non-interactive,
      // remove cut/copy command.
      // @ts-ignore: error TS2367: This comparison appears to be unintentional
      // because the types 'FileSystemEntry[]' and 'number' have no overlap.
      if (entries.length === 0 && !currentVolumeIsInteractive(fileManager)) {
        command.setHidden(true);
        return false;
      }

      // For MyFiles/Downloads we only allow copy.
      if (isMove &&
          fileManager.getSelection().entries.some(
              isNonModifiable.bind(null, volumeManager))) {
        return false;
      }

      return isMove ? fileTransferController.canCutOrDrag() :
                      fileTransferController.canCopyOrDrag();
    }

    const canDo =
        fileManager.ui.directoryTree.contains(/** @type {Node} */ (target)) ?
        canDoDirectoryTree() :
        canDoFileList();
    event.canExecute = canDo;
    command.disabled = !canDo;
  }
}

/**
 * Initiates file renaming.
 */
export class RenameCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const entry = getCommandEntry(fileManager, event.target);
    if (isNonModifiable(fileManager.volumeManager, entry)) {
      return;
    }
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    let isRemovableRoot = false;
    let volumeInfo = null;
    if (entry) {
      volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
      // Checks whether the target is an external drive.
      if (volumeInfo && isRootEntry(fileManager.volumeManager, entry)) {
        isRemovableRoot = true;
      }
    }
    if (isDirectoryTree(event.target) || isDirectoryTreeItem(event.target)) {
      if (isDirectoryTree(event.target)) {
        assert(fileManager.directoryTreeNamingController)
            .attachAndStart(
                assert(getFocusedTreeItem(event.target)), isRemovableRoot,
                volumeInfo);
      } else if (isDirectoryTreeItem(event.target)) {
        const directoryItem =
            /** @type {!DirectoryItem|!XfTreeItem} */ (event.target);
        assert(fileManager.directoryTreeNamingController)
            .attachAndStart(directoryItem, isRemovableRoot, volumeInfo);
      }
    } else {
      fileManager.namingController.initiateRename(isRemovableRoot, volumeInfo);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    // Block fusebox volumes in SelectFileAsh (Lacros) file picker mode.
    if (fileManager.volumeManager.getFuseBoxOnlyFilterEnabled()) {
      // TODO(crbug/1292825) Make it work with fusebox volumes: MTP, etc.
      if (fileManager.directoryModel.isOnFuseBox()) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }
    }

    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    // Check if it is removable drive
    if ((() => {
          const root = getCommandEntry(fileManager, event.target);
          // |root| is null for unrecognized volumes. Do not enable rename
          // command for such volumes because they need to be formatted prior to
          // rename.
          if (!root || !isRootEntry(fileManager.volumeManager, root)) {
            return false;
          }
          const volumeInfo = fileManager.volumeManager.getVolumeInfo(root);
          const location = fileManager.volumeManager.getLocationInfo(root);
          if (!volumeInfo || !location) {
            event.command.setHidden(true);
            event.canExecute = false;
            return true;
          }
          const writable = !location.isReadOnly;
          const removable = location.rootType === RootType.REMOVABLE;
          event.canExecute =
              removable && writable && volumeInfo.diskFileSystemType && [
                FileSystemType.EXFAT,
                FileSystemType.VFAT,
                FileSystemType.NTFS,
              ].indexOf(volumeInfo.diskFileSystemType) > -1;
          event.command.setHidden(!removable);
          return removable;
        })()) {
      return;
    }

    // Check if it is file or folder
    const renameTarget = isFromSelectionMenu(event) ?
        fileManager.ui.listContainer.currentList :
        event.target;
    const entries = getCommandEntries(fileManager, renameTarget);
    if (entries.length === 0 ||
        !shouldShowMenuItemsForEntry(
            // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
            // undefined' is not assignable to parameter of type
            // 'FileSystemEntry | FakeEntry'.
            fileManager.volumeManager, entries[0]) ||
        entries.some(isNonModifiable.bind(null, fileManager.volumeManager))) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const parentEntry =
        getParentEntry(renameTarget, fileManager.directoryModel);
    const locationInfo = parentEntry ?
        fileManager.volumeManager.getLocationInfo(parentEntry) :
        null;
    const volumeIsNotReadOnly = !!locationInfo && !locationInfo.isReadOnly;
    // ARC doesn't support rename for now. http://b/232152680
    // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined'
    // is not assignable to parameter of type 'FileSystemEntry | null'.
    const recentArcEntry = isRecentArcEntry(entries[0]);
    // Drive grand roots do not support rename.
    // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | undefined'
    // is not assignable to parameter of type 'FileSystemEntry | null'.
    const isDriveGrandRoot = isGrandRootEntryInDrives(entries[0]);

    event.canExecute = entries.length === 1 && volumeIsNotReadOnly &&
        !recentArcEntry && !isDriveGrandRoot &&
        hasCapability(fileManager, entries, 'canRename');
    event.command.setHidden(false);
  }
}

/**
 * Opens settings/files sub page.
 */
export class FilesSettingsCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage('files');
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    event.canExecute = true;
  }
}

/**
 * Opens drive help.
 */
export class VolumeHelpCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (fileManager.directoryModel.isOnDrive()) {
      visitURL(str('GOOGLE_DRIVE_HELP_URL'));
      recordMenuItemSelected(MenuCommandsForUma.DRIVE_HELP);
    } else {
      visitURL(str('FILES_APP_HELP_URL'));
      recordMenuItemSelected(MenuCommandsForUma.HELP);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    // Hides the help menu in modal dialog mode. It does not make much sense
    // because after all, users cannot view the help without closing, and
    // besides that the help page is about the Files app as an app, not about
    // the dialog mode itself. It can also lead to hard-to-fix bug
    // crbug.com/339089.
    const hideHelp = isModal(fileManager.dialogType);
    event.canExecute = !hideHelp;
    event.command.setHidden(hideHelp);
  }
}

/**
 * Opens the send feedback window.
 */
export class SendFeedbackCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.sendFeedback();
  }
}

/**
 * Opens drive buy-more-space url.
 */
export class DriveBuyMoreSpaceCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    visitURL(str('GOOGLE_DRIVE_BUY_STORAGE_URL'));
    recordMenuItemSelected(MenuCommandsForUma.DRIVE_BUY_MORE_SPACE);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    canExecuteVisibleOnDriveInNormalAppModeOnly(event, fileManager);
  }
}

/**
 * Opens drive.google.com.
 */
export class DriveGoToDriveCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    visitURL(str('GOOGLE_DRIVE_ROOT_URL'));
    recordMenuItemSelected(MenuCommandsForUma.DRIVE_GO_TO_DRIVE);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    canExecuteVisibleOnDriveInNormalAppModeOnly(event, fileManager);
  }
}

/**
 * Opens a file with default task.
 */
export class DefaultTaskCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    fileManager.taskController.executeDefaultTask();
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    event.canExecute = fileManager.taskController.canExecuteDefaultTask();
    event.command.setHidden(fileManager.taskController.shouldHideDefaultTask());
  }
}

/**
 * Displays "open with" dialog for current selection.
 */
export class OpenWithCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    console.assert(
        // @ts-ignore: error TS2345: Argument of type 'string' is not assignable
        // to parameter of type 'boolean | undefined'.
        `open-with command doesn't execute, ` +
        `instead it only opens the sub-menu`);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteOpenActions();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
}

/**
 * Invoke Sharesheet.
 */
export class InvokeSharesheetCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    const entries = fileManager.selectionHandler.selection.entries;
    const launchSource = getSharesheetLaunchSource(event);
    const dlpSourceUrls = fileManager.metadataModel
                              .getCache(entries, ['sourceUrl'])
                              // @ts-ignore: error TS7006: Parameter 'm'
                              // implicitly has an 'any' type.
                              .map(m => m.sourceUrl || '');
    chrome.fileManagerPrivate.invokeSharesheet(
        // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
        entries.map(e => unwrapEntry(e)), launchSource, dlpSourceUrls, () => {
          if (chrome.runtime.lastError) {
            console.warn(chrome.runtime.lastError.message);
            return;
          }
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const entries = fileManager.selectionHandler.selection.entries;

    if (!entries || entries.length === 0 ||
        // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an 'any'
        // type.
        (entries.some(entry => entry.isDirectory) &&
         (!isDriveEntries(entries, fileManager.volumeManager) ||
          entries.length > 1))) {
      event.canExecute = false;
      event.command.setHidden(true);
      event.command.disabled = true;
      return;
    }

    event.canExecute = true;
    // In the case where changing focus to action bar elements, it is safe
    // to keep the command enabled if it was visible before, because there
    // should be no change to the selected entries.
    event.command.disabled = !fileManager.ui.actionbar.contains(
        /** @type {Node} */ (event.target));

    chrome.fileManagerPrivate.sharesheetHasTargets(
        // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
        entries.map(e => unwrapEntry(e)), hasTargets => {
          if (chrome.runtime.lastError) {
            console.warn(chrome.runtime.lastError.message);
            return;
          }
          event.command.setHidden(!hasTargets);
          event.canExecute = hasTargets;
          event.command.disabled = !hasTargets;
        });
  }
}

export class ToggleHoldingSpaceCommand extends FilesCommand {
  constructor() {
    super();
    /**
     * Whether the command adds or removed items from holding space. The
     * value is set in <code>canExecute()</code>. It will be true unless all
     * selected items are already in the holding space.
     * @private @type {boolean|undefined}
     */
    this.addsItems_;
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    if (this.addsItems_ === undefined) {
      return;
    }

    // Filter out entries from unsupported volumes.
    const allowedVolumeTypes = HoldingSpaceUtil.getAllowedVolumeTypes();
    const entries =
        // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an
        // 'any' type.
        fileManager.selectionHandler.selection.entries.filter(entry => {
          const volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
          return volumeInfo &&
              allowedVolumeTypes.includes(volumeInfo.volumeType);
        });

    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        entries, this.addsItems_, () => {});

    if (this.addsItems_) {
      HoldingSpaceUtil.maybeStoreTimeOfFirstPin();
    }

    recordMenuItemSelected(
        this.addsItems_ ? MenuCommandsForUma.PIN_TO_HOLDING_SPACE :
                          MenuCommandsForUma.UNPIN_FROM_HOLDING_SPACE);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    const command = event.command;

    const allowedVolumeTypes = HoldingSpaceUtil.getAllowedVolumeTypes();
    const currentRootType = fileManager.directoryModel.getCurrentRootType();
    if (!isRecentRootType(currentRootType)) {
      const volumeInfo = fileManager.directoryModel.getCurrentVolumeInfo();
      if (!volumeInfo || !allowedVolumeTypes.includes(volumeInfo.volumeType)) {
        event.canExecute = false;
        command.setHidden(true);
        return;
      }
    }

    // Filter out entries from unsupported volumes.
    const entries =
        // @ts-ignore: error TS7006: Parameter 'entry' implicitly has an
        // 'any' type.
        fileManager.selectionHandler.selection.entries.filter(entry => {
          const volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
          return volumeInfo &&
              allowedVolumeTypes.includes(volumeInfo.volumeType);
        });

    if (entries.length === 0) {
      event.canExecute = false;
      command.setHidden(true);
      return;
    }

    event.canExecute = true;
    command.setHidden(false);

    this.checkHoldingSpaceState(entries, command);
  }

  /**
   * @param {!Array<Entry|FilesAppEntry>} entries
   * @param {!Command} command
   */
  async checkHoldingSpaceState(entries, command) {
    // Update the command to add or remove holding space items depending on
    // the current holding space state - the command will remove items only
    // if all currently selected items are already in the holding space.
    let state;
    try {
      state = await getHoldingSpaceState();
    } catch (e) {
      console.warn('Error getting holding space state', e);
    }
    if (!state) {
      // @ts-ignore: error TS2551: Property 'setHidden' does not exist on
      // type 'Command'. Did you mean 'hidden'?
      command.setHidden(true);
      return;
    }

    const itemsSet = {};
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type
    // because expression of type 'string' can't be used to index type '{}'.
    state.itemUrls.forEach((item) => itemsSet[item] = true);

    // @ts-ignore: error TS2345: Argument of type '(FileSystemEntry |
    // FilesAppEntry)[]' is not assignable to parameter of type
    // 'FileSystemEntry[]'.
    const selectedUrls = entriesToURLs(entries);
    // @ts-ignore: error TS7053: Element implicitly has an 'any' type
    // because expression of type 'string' can't be used to index type '{}'.
    this.addsItems_ = selectedUrls.some(url => !itemsSet[url]);

    command.label = this.addsItems_ ? str('HOLDING_SPACE_PIN_COMMAND_LABEL') :
                                      str('HOLDING_SPACE_UNPIN_COMMAND_LABEL');
  }
}

/**
 * Opens containing folder of the focused file.
 */
export class GoToFileLocationCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    if (entries.length !== 1) {
      return;
    }

    const components = PathComponent.computeComponentsFromEntry(
        // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry |
        // undefined' is not assignable to parameter of type
        // 'FileSystemEntry | FilesAppEntry'.
        entries[0], fileManager.volumeManager);
    // Entries in file list table should always have its containing folder.
    // (i.e. Its path have at least two components: its parent and itself.)
    assert(components.length >= 2);
    const parentComponent = components[components.length - 2];
    // @ts-ignore: error TS18048: 'parentComponent' is possibly 'undefined'.
    parentComponent.resolveEntry().then(entry => {
      if (entry && entry.isDirectory) {
        fileManager.directoryModel.changeDirectoryEntry(
            /** @type {!(DirectoryEntry|FilesAppDirEntry)} */ (entry));
      }
    });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    // Available in Recents, Audio, Images, and Videos.
    if (!isRecentRootType(fileManager.directoryModel.getCurrentRootType())) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    // Available for a single entry.
    const entries = getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1;
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Displays QuickView for current selection.
 */
export class GetInfoCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    // 'get-info' command is executed by 'command' event handler in
    // QuickViewController.
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    // QuickViewModel refers the file selection instead of event target.
    const entries = fileManager.getSelection().entries;
    if (entries.length === 0) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    event.canExecute = entries.length >= 1;
    event.command.setHidden(false);
  }
}

/**
 * Displays the Data Leak Prevention (DLP) Restriction details.
 */
export class DlpRestrictionDetailsCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  async executeImpl_(event, fileManager) {
    const entries = fileManager.getSelection().entries;

    const metadata = fileManager.metadataModel.getCache(entries, ['sourceUrl']);
    if (!metadata || metadata.length !== 1 || !metadata[0].sourceUrl) {
      return;
    }

    const sourceUrl = /** @type {string} */ (metadata[0].sourceUrl);
    try {
      const details = await getDlpRestrictionDetails(sourceUrl);
      fileManager.ui.dlpRestrictionDetailsDialog
          .showDlpRestrictionDetailsDialog(details);
    } catch (e) {
      console.warn(`Error showing DLP restriction details `, e);
    }
  }

  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    this.executeImpl_(event, fileManager);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    if (!isDlpEnabled()) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const entries = fileManager.getSelection().entries;

    // Show this item only when one file is selected.
    if (entries.length !== 1) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const metadata =
        fileManager.metadataModel.getCache(entries, ['isDlpRestricted']);
    if (!metadata || metadata.length !== 1) {
      event.canExecute = false;
      event.command.setHidden(true);
    }

    const isDlpRestricted = metadata[0].isDlpRestricted;
    event.canExecute = isDlpRestricted;
    event.command.setHidden(!isDlpRestricted);
  }
}

/**
 * Focuses search input box.
 */
export class SearchCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    // If the current root is Trash we do nothing on search command. Preventing
    // it from execution (in canExecute) does not work correctly, as then chrome
    // start native search for an app window. Thus we always allow it and do
    // nothing in trash.
    const currentRootType = fileManager.directoryModel.getCurrentRootType();
    if (currentRootType !== RootType.TRASH) {
      // Cancel item selection.
      fileManager.directoryModel.clearSelection();
      // Open the query input via the search container.
      fileManager.ui.searchContainer.openSearch();
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    event.canExecute = !fileManager.namingController.isRenamingInProgress();
  }
}

export class VolumeSwitchCommand extends FilesCommand {
  /**
   * @param {number} index
   */
  constructor(index) {
    super();
    this.index_ = index;
  }

  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    if (isNewDirectoryTreeEnabled()) {
      const items = fileManager.ui.directoryTree.items;
      if (items[this.index_ - 1]?.entry) {
        getStore().dispatch(
            changeDirectory({toKey: items[this.index_ - 1].entry.toURL()}));
      }
    } else {
      fileManager.ui.directoryTree.activateByIndex(this.index_ - 1);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    event.canExecute = this.index_ > 0 &&
        this.index_ <= fileManager.ui.directoryTree.items.length;
  }
}

/**
 * Flips 'available offline' flag on the file.
 */
export class TogglePinnedCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const entries = fileManager.getSelection().entries;
    const actionsController = fileManager.actionsController;

    actionsController.getActionsForEntries(entries).then(
        // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
        // 'any' type.
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const saveForOfflineAction =
              actionsModel.getAction(CommonActionId.SAVE_FOR_OFFLINE);
          const offlineNotNeededAction =
              actionsModel.getAction(CommonActionId.OFFLINE_NOT_NECESSARY);
          // Saving for offline has a priority if both actions are available.
          let action = offlineNotNeededAction;
          if (saveForOfflineAction && saveForOfflineAction.canExecute()) {
            action = saveForOfflineAction;
          }
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = fileManager.getSelection().entries;
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    // When the bulk pinning panel is enabled, the "Available offline" toggle
    // should not be visible as the underlying functionality is handled
    // automatically.
    if (isDriveFsBulkPinningEnabled()) {
      const state = /** @type {State} */ (getStore().getState());
      // @ts-ignore: error TS18048: 'state.preferences' is possibly 'undefined'.
      const bulkPinningPref = state.preferences.driveFsBulkPinningEnabled;
      if (bulkPinningPref && isOnlyMyDriveEntries(entries, state)) {
        command.setHidden(true);
        event.canExecute = false;
        return;
      }
    }

    command.setHidden(false);

    // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
    // 'any' type.
    function canExecutePinned_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const saveForOfflineAction =
          actionsModel.getAction(CommonActionId.SAVE_FOR_OFFLINE);
      const offlineNotNeededAction =
          actionsModel.getAction(CommonActionId.OFFLINE_NOT_NECESSARY);
      let action = offlineNotNeededAction;
      command.checked = !!offlineNotNeededAction;
      if (saveForOfflineAction && saveForOfflineAction.canExecute()) {
        action = saveForOfflineAction;
        command.checked = false;
      }
      event.canExecute = action && action.canExecute();
      command.disabled = !event.canExecute;
    }

    // Run synchrounously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecutePinned_(actionsModel);
      return;
    }

    event.canExecute = true;
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(canExecutePinned_);
  }
}

/**
 * Extracts content of ZIP files in the current selection.
 */
export class ExtractAllCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    let dirEntry = fileManager.getCurrentDirectoryEntry();
    if (!dirEntry ||
        !fileManager.getSelection().entries.every(
            shouldShowMenuItemsForEntry.bind(
                null, fileManager.volumeManager))) {
      return;
    }

    const selectionEntries = fileManager.getSelection().entries;
    if (fileManager.directoryModel.isReadOnly()) {
      dirEntry = fileManager.directoryModel.getMyFiles();
    }
    fileManager.taskController.startExtractIoTask(
        selectionEntries, /** @type {!DirectoryEntry} */ (dirEntry));
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    const selection = fileManager.getSelection();

    if (isOnTrashRoot(fileManager) || !dirEntry || !selection ||
        selection.totalCount === 0) {
      event.command.setHidden(true);
      event.canExecute = false;
    } else {
      // Check the selected entries for a ZIP archive in the selected set.
      for (const entry of selection.entries) {
        if (getExtension(entry) === '.zip') {
          event.command.setHidden(false);
          event.canExecute = true;
          return;
        }
      }
      // Didn't find any ZIP files, disable extract-all.
      event.command.setHidden(true);
      event.canExecute = false;
    }
  }
}

/**
 * Creates ZIP file for current selection.
 */
export class ZipSelectionCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    if (!dirEntry ||
        !fileManager.getSelection().entries.every(
            shouldShowMenuItemsForEntry.bind(
                null, fileManager.volumeManager))) {
      return;
    }

    const selectionEntries = fileManager.getSelection().entries;
    startIOTask(
        chrome.fileManagerPrivate.IOTaskType.ZIP, selectionEntries,
        {destinationFolder: /** @type {!DirectoryEntry} */ (dirEntry)});
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const dirEntry = fileManager.getCurrentDirectoryEntry();
    const selection = fileManager.getSelection();

    // Hide ZIP selection for single ZIP file selected.
    if (selection.entries.length === 1 &&
        getExtension(selection.entries[0]) === '.zip') {
      event.command.setHidden(true);
      event.canExecute = false;
      return;
    }

    if (!selection.entries.every(shouldShowMenuItemsForEntry.bind(
            null, fileManager.volumeManager))) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    // Hide if there isn't anything selected, meaning user clicked in an empty
    // space in the file list.
    const noEntries = selection.entries.length === 0;
    event.command.setHidden(noEntries);

    // TODO(crbug/1226915) Make it work with MTP.
    const isOnEligibleLocation = fileManager.directoryModel.isOnNative();

    // Hide if any encrypted files are selected, as we can't read them.
    const hasEncryptedFile =
        fileManager.metadataModel
            .getCache(selection.entries, ['contentMimeType'])
            .some(
                // @ts-ignore: error TS7006: Parameter 'i' implicitly has an
                // 'any' type.
                (metadata, i) => isEncrypted(
                    selection.entries[i], metadata.contentMimeType));

    event.canExecute = dirEntry && !fileManager.directoryModel.isReadOnly() &&
        isOnEligibleLocation && selection && selection.totalCount > 0 &&
        !hasEncryptedFile;
  }
}

/**
 * Shows the share dialog for the current selection (single only).
 */
export class ShareCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
        // 'any' type.
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const action = actionsModel.getAction(CommonActionId.SHARE);
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
    // 'any' type.
    function canExecuteShare_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const action = actionsModel.getAction(CommonActionId.SHARE);
      event.canExecute = action && action.canExecute();
      command.disabled = !event.canExecute;
      command.setHidden(!action);
    }

    // Run synchrounously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecuteShare_(actionsModel);
      return;
    }

    event.canExecute = true;
    command.setHidden(false);
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(canExecuteShare_);
  }
}

/**
 * Opens the file in Drive for the user to manage sharing permissions etc.
 */
export class ManageInDriveCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
        // 'any' type.
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const action =
              actionsModel.getAction(InternalActionId.MANAGE_IN_DRIVE);
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
    // 'any' type.
    function canExecuteManageInDrive_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const action = actionsModel.getAction(InternalActionId.MANAGE_IN_DRIVE);
      if (action) {
        command.setHidden(!action);
        event.canExecute = action && action.canExecute();
        command.disabled = !event.canExecute;
      }
    }

    // Run synchronously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecuteManageInDrive_(actionsModel);
      return;
    }

    event.canExecute = true;
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(
        canExecuteManageInDrive_);
  }
}

/**
 * Opens the Manage MirrorSync dialog if the flag is enabled.
 */
export class ManageMirrorsyncCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openManageSyncSettings();
  }

  /**
   * @override
   */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    // MirrorSync is only available to sync local directories, only show the
    // folder when navigated to a local directory.
    const currentRootType = fileManager.directoryModel.getCurrentRootType();
    event.canExecute = (currentRootType === RootType.MY_FILES ||
                        currentRootType === RootType.DOWNLOADS) &&
        isMirrorSyncEnabled();
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * A command to share the target folder with the specified Guest OS.
 */
export class GuestOsShareCommand extends FilesCommand {
  /**
   * @param {string} vmName Name of the vm to share into.
   * @param {string} typeForStrings VM type to identify the strings used for
   *     this VM e.g. LINUX or PLUGIN_VM.
   * @param {string} settingsPath Path to the page in settings to manage
   *     sharing.
   * @param {!MenuCommandsForUma} manageUma MenuCommandsForUma
   *     entry this command should emit metrics under when the toast to manage
   *     sharing is clicked on.
   * @param {!MenuCommandsForUma} shareUma MenuCommandsForUma
   *     entry this command should emit metrics under.
   */
  constructor(vmName, typeForStrings, settingsPath, manageUma, shareUma) {
    super();
    this.vmName_ = vmName;
    this.typeForStrings_ = typeForStrings;
    this.settingsPath_ = settingsPath;
    this.manageUma_ = manageUma;
    this.shareUma_ = shareUma;

    this.validateTranslationStrings_();
  }

  /**
   * Asserts that the necessary strings have been loaded into loadTimeData.
   */
  validateTranslationStrings_() {
    if (!loadTimeData.isInitialized()) {
      // Tests might not set loadTimeData.
      return;
    }
    const translations = [
      `FOLDER_SHARED_WITH_${this.typeForStrings_}`,
      `SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_TITLE`,
      `SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}`,
      `SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_DRIVE`,
    ];
    for (const translation of translations) {
      console.assert(
          loadTimeData.valueExists(translation),
          `VM ${this.vmName_} doesn't have the translation string ${
              translation}`);
    }
  }

  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const entry = getCommandEntry(fileManager, event.target);
    if (!entry || !entry.isDirectory) {
      return;
    }
    const info = fileManager.volumeManager.getLocationInfo(entry);
    if (!info) {
      return;
    }
    const share = () => {
      // Always persist shares via right-click > Share with Linux.
      chrome.fileManagerPrivate.sharePathsWithCrostini(
          // @ts-ignore: error TS2322: Type 'FileSystemEntry | FilesAppEntry' is
          // not assignable to type 'FileSystemEntry'.
          this.vmName_, [unwrapEntry(entry)], true /* persist */, () => {
            if (chrome.runtime.lastError) {
              console.warn(
                  'Error sharing with guest: ' +
                  chrome.runtime.lastError.message);
            }
          });
      // Show the 'Manage $typeForStrings sharing' toast immediately, since
      // the guest may take a while to start.
      fileManager.ui.toast.show(
          str(`FOLDER_SHARED_WITH_${this.typeForStrings_}`), {
            text: str('MANAGE_TOAST_BUTTON_LABEL'),
            callback: () => {
              chrome.fileManagerPrivate.openSettingsSubpage(this.settingsPath_);
              recordMenuItemSelected(this.manageUma_);
            },
          });
    };
    // Show a confirmation dialog if we are sharing the root of a volume.
    // Non-Drive volume roots are always '/'.
    if (entry.fullPath == '/') {
      fileManager.ui.confirmDialog.showHtml(
          // @ts-ignore: error TS2555: Expected at least 2 arguments, but got 1.
          strf(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_TITLE`),
          strf(
              `SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}`,
              info.volumeInfo.label),
          share, () => {});
    } else if (
        info.isRootEntry &&
        (info.rootType == RootType.DRIVE ||
         info.rootType == RootType.COMPUTERS_GRAND_ROOT ||
         info.rootType == RootType.SHARED_DRIVES_GRAND_ROOT)) {
      // Only show the dialog for My Drive, Shared Drives Grand Root and
      // Computers Grand Root.  Do not show for roots of a single Shared
      // Drive or Computer.
      fileManager.ui.confirmDialog.showHtml(
          // @ts-ignore: error TS2555: Expected at least 2 arguments, but got 1.
          strf(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_TITLE`),
          // @ts-ignore: error TS2555: Expected at least 2 arguments, but got 1.
          strf(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_DRIVE`), share,
          () => {});
    } else {
      // This is not a root, share it without confirmation dialog.
      share();
    }
    recordMenuItemSelected(this.shareUma_);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    // Must be single directory not already shared.
    const entries = getCommandEntries(fileManager, event.target);
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        !fileManager.crostini.isPathShared(this.vmName_, entries[0]) &&
        fileManager.crostini.canSharePath(
            this.vmName_, entries[0], true /* persist */);
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Creates a command for the gear icon to manage sharing.
 */
export class GuestOsManagingSharingGearCommand extends FilesCommand {
  /**
   * @param {string} vmName Name of the vm to share into.
   * @param {string} settingsPath Path to the page in settings to manage
   *     sharing.
   * @param {!MenuCommandsForUma} manageUma MenuCommandsForUma
   *     entry this command should emit metrics under when the toast to manage
   *     sharing is clicked on.
   */
  constructor(vmName, settingsPath, manageUma) {
    super();
    this.vmName_ = vmName;
    this.settingsPath_ = settingsPath;
    this.manageUma_ = manageUma;
  }
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage(this.settingsPath_);
    recordMenuItemSelected(this.manageUma_);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    event.canExecute = fileManager.crostini.isEnabled(this.vmName_);
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Creates a command for managing sharing.
 */
export class GuestOsManagingSharingCommand extends FilesCommand {
  /**
   * @param {string} vmName Name of the vm to share into.
   * @param {string} settingsPath Path to the page in settings to manage
   *     sharing.
   * @param {!MenuCommandsForUma} manageUma MenuCommandsForUma
   *     entry this command should emit metrics under when the toast to manage
   *     sharing is clicked on.
   */
  constructor(vmName, settingsPath, manageUma) {
    super();
    this.vmName_ = vmName;
    this.settingsPath_ = settingsPath;
    this.manageUma_ = manageUma;
  }
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage(this.settingsPath_);
    recordMenuItemSelected(this.manageUma_);
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    // @ts-ignore: error TS2532: Object is possibly 'undefined'.
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        fileManager.crostini.isPathShared(this.vmName_, entries[0]);
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Creates a shortcut of the selected folder (single only).
 */
export class PinFolderCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
        // 'any' type.
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const action =
              actionsModel.getAction(InternalActionId.CREATE_FOLDER_SHORTCUT);
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
    // 'any' type.
    function canExecuteCreateShortcut_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const action =
          actionsModel.getAction(InternalActionId.CREATE_FOLDER_SHORTCUT);
      event.canExecute = action && action.canExecute();
      command.disabled = !event.canExecute;
      command.setHidden(!action);
    }

    // Run synchrounously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecuteCreateShortcut_(actionsModel);
      return;
    }

    event.canExecute = true;
    command.setHidden(false);
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(
        canExecuteCreateShortcut_);
  }
}

/**
 * Removes the folder shortcut.
 */
export class UnpinFolderCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
        // 'any' type.
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const action =
              actionsModel.getAction(InternalActionId.REMOVE_FOLDER_SHORTCUT);
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const entries = getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    // @ts-ignore: error TS7006: Parameter 'actionsModel' implicitly has an
    // 'any' type.
    function canExecuteRemoveShortcut_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const action =
          actionsModel.getAction(InternalActionId.REMOVE_FOLDER_SHORTCUT);
      command.setHidden(!action);
      event.canExecute = action && action.canExecute();
      command.disabled = !event.canExecute;
    }

    // Run synchrounously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecuteRemoveShortcut_(actionsModel);
      return;
    }

    event.canExecute = true;
    command.setHidden(false);
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(
        canExecuteRemoveShortcut_);
  }
}

/**
 * Zoom in to the Files app.
 */
export class ZoomInCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.IN);
  }
}

/**
 * Zoom out from the Files app.
 */
export class ZoomOutCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.OUT);
  }
}

/**
 * Reset the zoom factor.
 */
export class ZoomResetCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.RESET);
  }
}

/**
 * Sort the file list by name (in ascending order).
 */
export class SortByNameCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('name', 'asc');
      const msg = strf('COLUMN_SORTED_ASC', str('NAME_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
}

/**
 * Sort the file list by size (in descending order).
 */
export class SortBySizeCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('size', 'desc');
      const msg = strf('COLUMN_SORTED_DESC', str('SIZE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
}

/**
 * Sort the file list by type (in ascending order).
 */
export class SortByTypeCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('type', 'asc');
      const msg = strf('COLUMN_SORTED_ASC', str('TYPE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
}

/**
 * Sort the file list by date-modified (in descending order).
 */
export class SortByDateCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('modificationTime', 'desc');
      const msg = strf('COLUMN_SORTED_DESC', str('DATE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
}

/**
 * Open inspector for foreground page.
 */
export class InspectNormalCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.NORMAL);
  }
}

/**
 * Open inspector for foreground page and bring focus to the console.
 */
export class InspectConsoleCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.CONSOLE);
  }
}

/**
 * Open inspector for foreground page in inspect element mode.
 */
export class InspectElementCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.ELEMENT);
  }
}

/**
 * Opens the gear menu.
 */
export class OpenGearMenuCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    fileManager.ui.gearButton.showMenu(true);
  }
}

/**
 * Focus the first button visible on action bar (at the top).
 */
export class FocusActionBarCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    fileManager.ui.actionbar
        .querySelector('button:not([hidden]), cr-button:not([hidden])')
        .focus();
  }
}

/**
 * Handle back button.
 */
export class BrowserBackCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    // TODO(fukino): It should be better to minimize Files app only when there
    // is no back stack, and otherwise use BrowserBack for history navigation.
    // https://crbug.com/624100.
    // TODO(https://crbug.com/1097066): Implement minimize for files SWA, then
    // call its minimize() function here.
  }
}

/**
 * Configures the currently selected volume.
 */
export class ConfigureCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    const volumeInfo = getElementVolumeInfo(event.target, fileManager);
    if (volumeInfo && volumeInfo.configurable) {
      fileManager.volumeManager.configure(volumeInfo);
    }
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const volumeInfo = getElementVolumeInfo(event.target, fileManager);
    event.canExecute = volumeInfo && volumeInfo.configurable;
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Refreshes the currently selected directory.
 */
export class RefreshCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    fileManager.directoryModel.rescan(true /* refresh */);
    fileManager.spinnerController.blink();
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    const currentDirEntry = fileManager.directoryModel.getCurrentDirEntry();
    const volumeInfo = currentDirEntry &&
        fileManager.volumeManager.getVolumeInfo(currentDirEntry);
    event.canExecute = volumeInfo && !volumeInfo.watchable;
    event.command.setHidden(
        !event.canExecute ||
        fileManager.directoryModel.getFileListSelection().getCheckSelectMode());
  }
}

/**
 * Sets the system wallpaper to the selected file.
 */
export class SetWallpaperCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    const entry = fileManager.getSelection().entries[0];
    new Promise((resolve, reject) => {
      entry.file(resolve, reject);
    })
        .then(blob => {
          const fileReader = new FileReader();
          return new Promise((resolve, reject) => {
            fileReader.onload = () => {
              resolve(fileReader.result);
            };
            fileReader.onerror = () => {
              reject(fileReader.error);
            };
            fileReader.readAsArrayBuffer(blob);
          });
        })
        .then((/** @type {!ArrayBuffer} */ arrayBuffer) => {
          return new Promise((resolve, reject) => {
            // @ts-ignore: error TS2339: Property 'wallpaper' does not exist on
            // type 'typeof chrome'.
            chrome.wallpaper.setWallpaper(
                {
                  data: arrayBuffer,
                  // @ts-ignore: error TS2339: Property 'wallpaper' does not
                  // exist on type 'typeof chrome'.
                  layout: chrome.wallpaper.WallpaperLayout.CENTER_CROPPED,
                  filename: 'wallpaper',
                },
                () => {
                  if (chrome.runtime.lastError) {
                    reject(chrome.runtime.lastError);
                  } else {
                    resolve(null);
                  }
                });
          });
        })
        .catch(() => {
          fileManager.ui.alertDialog.showHtml(
              '', str('ERROR_INVALID_WALLPAPER'), null, null, null);
        });
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const entries = fileManager.getSelection().entries;
    if (entries.length === 0) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const type = getType(entries[0]);
    if (entries.length !== 1 || type.type !== 'image') {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    event.canExecute = type.subtype === 'JPEG' || type.subtype === 'PNG';
    event.command.setHidden(false);
  }
}

/**
 * Opens settings/storage sub page.
 */
export class VolumeStorageCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage('storage');
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an 'any'
  // type.
  canExecute(event, fileManager) {
    event.canExecute = false;
    const currentVolumeInfo = fileManager.directoryModel.getCurrentVolumeInfo();
    if (!currentVolumeInfo) {
      return;
    }

    // Can execute only for local file systems.
    if (currentVolumeInfo.volumeType == VolumeType.MY_FILES ||
        currentVolumeInfo.volumeType == VolumeType.DOWNLOADS ||
        currentVolumeInfo.volumeType == VolumeType.CROSTINI ||
        currentVolumeInfo.volumeType == VolumeType.GUEST_OS ||
        currentVolumeInfo.volumeType == VolumeType.ANDROID_FILES ||
        currentVolumeInfo.volumeType == VolumeType.DOCUMENTS_PROVIDER) {
      event.canExecute = true;
    }
  }
}
/**
 * Opens "providers menu" to allow users to use providers/FSPs.
 */
export class ShowProvidersSubmenuCommand extends FilesCommand {
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  execute(event, fileManager) {
    fileManager.ui.gearButton.showSubMenu();
  }

  /** @override */
  // @ts-ignore: error TS7006: Parameter 'fileManager' implicitly has an
  // 'any' type.
  canExecute(event, fileManager) {
    if (fileManager.dialogType !== DialogType.FULL_PAGE) {
      event.canExecute = false;
    } else {
      event.canExecute = !fileManager.guestMode;
    }
  }
}
