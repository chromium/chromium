// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_input/cr_input.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assert} from 'chrome://resources/js/assert.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {getDlpRestrictionDetails, getHoldingSpaceState, startIOTask} from '../../common/js/api.js';
import {isModal} from '../../common/js/dialog_type.js';
import {getFocusedTreeItem} from '../../common/js/dom_utils.js';
import {entriesToURLs, getTreeItemEntry, isDirectoryEntry, isFakeEntry, isGrandRootEntryInDrive, isNonModifiable, isReadOnlyForDelete, isRecentRootType, isTeamDriveRoot, isTeamDrivesGrandRoot, isTrashEntry, isTrashRoot, unwrapEntry} from '../../common/js/entry_utils.js';
import {getExtension, getType, isEncrypted} from '../../common/js/file_type.js';
import type {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {EntryList} from '../../common/js/files_app_entry_types.js';
import {isDlpEnabled, isDriveFsBulkPinningEnabled, isMirrorSyncEnabled, isSinglePartitionFormatEnabled} from '../../common/js/flags.js';
import {recordEnum, recordUserAction} from '../../common/js/metrics.js';
import {getFileErrorString, str, strf} from '../../common/js/translations.js';
import type {TrashEntry} from '../../common/js/trash.js';
import {deleteIsForever, RestoreFailedType, RestoreFailedTypesUMA, RestoreFailedUMA, shouldMoveToTrash} from '../../common/js/trash.js';
import {debug, isNullOrUndefined, visitURL} from '../../common/js/util.js';
import {FileSystemType, isRecentArcEntry, RootType, VolumeError, VolumeType} from '../../common/js/volume_manager_types.js';
import {readSubDirectories, updateFileData} from '../../state/ducks/all_entries.js';
import {changeDirectory} from '../../state/ducks/current_directory.js';
import {DialogType} from '../../state/state.js';
import {getStore} from '../../state/store.js';
import {isTreeItem, isXfTree} from '../../widgets/xf_tree_util.js';
import type {FilesTooltip} from '../elements/files_tooltip.js';

import {type ActionsModel, CommonActionId, InternalActionId} from './actions_model.js';
import {type CommandHandlerDeps, MenuCommandsForUma, recordMenuItemSelected} from './command_handler.js';
import {canExecuteVisibleOnDriveInNormalAppModeOnly, containsNonInteractiveEntry, currentVolumeIsInteractive, getCommandEntries, getCommandEntry, getElementVolumeInfo, getEventEntry, getOnlyOneSelectedDirectory, getParentEntry, getSharesheetLaunchSource, hasCapability, isDriveEntries, isFromSelectionMenu, isOnlyMyDriveEntries, isOnTrashRoot, isRootEntry, shouldIgnoreEvents, shouldShowMenuItemsForEntry} from './file_manager_commands_util.js';
import type {PasteWithDestDirectoryEvent} from './file_transfer_controller.js';
import {getAllowedVolumeTypes, maybeStoreTimeOfFirstPin} from './holding_space_util.js';
import {PathComponent} from './path_component.js';
import type {Command} from './ui/command.js';
import {type CanExecuteEvent, type CommandEvent} from './ui/command.js';
import type {FilesConfirmDialog} from './ui/files_confirm_dialog.js';

/**
 * Used to filter out `VolumeInfo` that don't exist and maintain the return
 * array is of type `VolumeInfo[]` without null or undefined.
 */
function isVolumeInfo(volumeInfo: VolumeInfo|null|
                      undefined): volumeInfo is VolumeInfo {
  return !isNullOrUndefined(volumeInfo);
}

/**
 * A command.
 */
abstract class FilesCommand {
  /**
   * Handles the execute event.
   * @param event Command event.
   * @param fileManager CommandHandlerDeps.
   */
  abstract execute(event: CommandEvent, fileManager: CommandHandlerDeps): void;

  /**
   * Handles the can execute event.
   * By default, sets the command as always enabled.
   * @param event Can execute event.
   * @param fileManager CommandHandlerDeps.
   */
  canExecute(event: CanExecuteEvent, _fileManager: CommandHandlerDeps) {
    event.canExecute = true;
  }
}

/**
 * Unmounts external drive.
 */
export class UnmountCommand extends FilesCommand {
  /**
   * @param event Command event.
   * @param fileManager CommandHandlerDeps.
   */
  private async executeImpl_(event: Event, fileManager: CommandHandlerDeps) {
    const errorCallback = (volumeType?: VolumeType) => {
      if (volumeType === VolumeType.REMOVABLE) {
        fileManager.ui.alertDialog.showHtml('', str('UNMOUNT_FAILED'));
      } else {
        fileManager.ui.alertDialog.showHtml('', str('UNMOUNT_PROVIDED_FAILED'));
      }
    };

    // Find volumes to unmount.
    let volumes: VolumeInfo[] = [];
    let label = '';
    const entry = getCommandEntry(fileManager, event.target);
    if (entry instanceof EntryList) {
      // The element is a group of removable partitions.
      if (!entry) {
        errorCallback();
        return;
      }
      // Add child partitions to the list of volumes to be unmounted.
      volumes = entry.getUiChildren()
                    .map(
                        child => ('volumeInfo' in child) ?
                            child.volumeInfo as VolumeInfo :
                            null)
                    .filter(isVolumeInfo);
      label = entry.label || '';
    } else {
      // The element is a removable volume with no partitions.
      const volumeInfo = getElementVolumeInfo(event.target, fileManager);
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
        debug(`Cannot unmount '${volume.volumeId}':`, error);
        if (error !== VolumeError.PATH_NOT_MOUNTED) {
          errorCallback(volume.volumeType);
        }
      }
    });

    await Promise.all(promises);
    fileManager.ui.speakA11yMessage(strf('A11Y_VOLUME_EJECT', label));
  }

  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    this.executeImpl_(event, fileManager);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const directoryModel = fileManager.directoryModel;
    let root: Entry|FilesAppEntry|undefined;
    if (fileManager.ui.directoryTree?.contains(event.target as Node)) {
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

    assert(root);
    const volumeInfo = fileManager.volumeManager.getVolumeInfo(root);
    if (volumeInfo) {
      fileManager.ui.formatDialog.showModal(volumeInfo);
    }
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const directoryModel = fileManager.directoryModel;
    let root;
    if (fileManager.ui.directoryTree?.contains(event.target as Node)) {
      // The command is executed from the directory tree context menu.
      root = getCommandEntry(fileManager, event.target);
    } else {
      // The command is executed from the gear menu.
      root = directoryModel.getCurrentDirEntry();
    }

    // |root| is null for unrecognized volumes. Enable format command for such
    // volumes.
    const isUnrecognizedVolume = (root === null);
    // See the comment in execute() for why doing this.
    if (!root) {
      root = directoryModel.getCurrentDirEntry();
    }
    const location = root && fileManager.volumeManager.getLocationInfo(root);
    const writable = !!location && !location.isReadOnly;
    const isRoot = location && location.isRootEntry;

    // Enable the command if this is a removable device (e.g. a USB drive).
    const removableRoot =
        location && isRoot && location.rootType === RootType.REMOVABLE;
    event.canExecute = !!removableRoot && (isUnrecognizedVolume || writable);

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
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const root = getEventEntry(event, fileManager);

    if (root && root instanceof EntryList) {
      fileManager.ui.formatDialog.showEraseModal(root);
    }
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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

    event.canExecute = !!removableRoot && !writable;
    // Enable the command if this is a removable and device node.
    event.command.setHidden(!removableRoot || !isDevice);
  }
}

/**
 * Initiates new folder creation.
 */
export class NewFolderCommand extends FilesCommand {
  /**
   * Whether a new-folder is in progress.
   */
  private busy_ = false;

  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    let targetDirectory: DirectoryEntry|FilesAppDirEntry|null|undefined;
    let executedFromDirectoryTree: boolean;

    if (isXfTree(event.target)) {
      const focusedTreeItem = getFocusedTreeItem(event.target);
      targetDirectory = getTreeItemEntry(focusedTreeItem) as DirectoryEntry |
          FilesAppDirEntry | null;
      executedFromDirectoryTree = true;
    } else if (isTreeItem(event.target)) {
      targetDirectory = getTreeItemEntry(event.target) as DirectoryEntry |
          FilesAppDirEntry | null;
      executedFromDirectoryTree = true;
    } else {
      targetDirectory = fileManager.directoryModel.getCurrentDirEntry();
      executedFromDirectoryTree = false;
    }

    const directoryModel = fileManager.directoryModel;
    const listContainer = fileManager.ui.listContainer;
    this.busy_ = true;

    assert(targetDirectory);
    const directoryEntry = unwrapEntry(targetDirectory) as DirectoryEntry;
    this.generateNewDirectoryName_(directoryEntry).then((newName) => {
      if (!executedFromDirectoryTree) {
        listContainer.startBatchUpdates();
      }

      return new Promise(
                 directoryEntry.getDirectory.bind(
                     directoryEntry, newName, {create: true, exclusive: true}))
          .then(
              (newDirectory) => {
                recordUserAction('CreateNewFolder');

                // Select new directory and start rename operation.
                if (executedFromDirectoryTree) {
                  const parentFileKey = directoryEntry.toURL();
                  // After new directory is created on parent directory, we
                  // need to expand it otherwise the new child item won't
                  // show, and also trigger a re-scan for the parent
                  // directory.
                  getStore().dispatch(updateFileData({
                    key: parentFileKey,
                    partialFileData: {expanded: true},
                  }));
                  getStore().dispatch(readSubDirectories(parentFileKey));
                  fileManager.ui.directoryTreeContainer
                      ?.renameItemWithKeyWhenRendered(newDirectory.toURL());
                  this.busy_ = false;
                } else {
                  directoryModel.updateAndSelectNewDirectory(newDirectory)
                      .then(() => {
                        listContainer.endBatchUpdates();
                        fileManager.namingController.initiateRename();
                        this.busy_ = false;
                      })
                      .catch(error => {
                        listContainer.endBatchUpdates();
                        this.busy_ = false;
                        console.warn(error);
                      });
                }
              },
              (error) => {
                if (!executedFromDirectoryTree) {
                  listContainer.endBatchUpdates();
                }

                this.busy_ = false;

                fileManager.ui.alertDialog.show(strf(
                    'ERROR_CREATING_FOLDER', newName,
                    getFileErrorString(error.name)));
              });
    });
  }

  /**
   * Generates new directory name.
   */
  private generateNewDirectoryName_(
      parentDirectory: DirectoryEntry, index: number = 0): Promise<string> {
    const defaultName = str('DEFAULT_NEW_FOLDER_NAME');
    const newName =
        index === 0 ? defaultName : defaultName + ' (' + index + ')';

    return new Promise(parentDirectory.getDirectory.bind(
                           parentDirectory, newName, {create: false}))
        .then(_newEntry => {
          return this.generateNewDirectoryName_(parentDirectory, index + 1);
        })
        .catch(() => {
          return newName;
        });
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
    if (isXfTree(event.target) || isTreeItem(event.target)) {
      const entry = entries[0];
      if (!entry || isFakeEntry(entry) || isTeamDrivesGrandRoot(entry)) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }

      const locationInfo = fileManager.volumeManager.getLocationInfo(entry);
      event.canExecute = !!locationInfo && !locationInfo.isReadOnly &&
          hasCapability(fileManager, [entry], 'canAddChildren');
      event.command.setHidden(false);
    } else {
      // If blank space was clicked and current volume is non-interactive,
      // remove new-folder command.
      if (entries.length === 0 && !currentVolumeIsInteractive(fileManager)) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }
      const directoryModel = fileManager.directoryModel;
      const directoryEntry = fileManager.getCurrentDirectoryEntry()!;
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
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.launchFileManager({
      currentDirectoryURL: fileManager.getCurrentDirectoryEntry() &&
          fileManager.getCurrentDirectoryEntry()!.toURL(),
    });
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    event.canExecute = !!fileManager.getCurrentDirectoryEntry() &&
        (fileManager.dialogType === DialogType.FULL_PAGE);
  }
}

export class SelectAllCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.directoryModel.getFileListSelection().setCheckSelectMode(true);
    fileManager.directoryModel.getFileListSelection().selectAll();
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    // Check we can select multiple items.
    const multipleSelect =
        fileManager.directoryModel.getFileListSelection().multiple;
    // Check we are not inside an input element (e.g. the search box).
    const inputElementActive =
        document.activeElement instanceof HTMLInputElement ||
        document.activeElement instanceof HTMLTextAreaElement ||
        document.activeElement?.tagName.toLowerCase() === 'cr-input';
    event.canExecute = multipleSelect && !inputElementActive &&
        fileManager.directoryModel.getFileList().length > 0;
  }
}

export class ToggleHiddenFilesCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const visible = !fileManager.fileFilter.isHiddenFilesVisible();
    fileManager.fileFilter.setHiddenFilesVisible(visible);
    event.detail.command.checked =
        visible;  // Check-mark for "Show hidden files".
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
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const visible = !fileManager.fileFilter.isAllAndroidFoldersVisible();
    fileManager.fileFilter.setAllAndroidFoldersVisible(visible);
    event.detail.command.checked = visible;
    recordMenuItemSelected(
        visible ? MenuCommandsForUma.HIDDEN_ANDROID_FOLDERS_SHOW :
                  MenuCommandsForUma.HIDDEN_ANDROID_FOLDERS_HIDE);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const hasAndroidFilesVolumeInfo =
        !!fileManager.volumeManager.getCurrentProfileVolumeInfo(
            VolumeType.ANDROID_FILES);
    const currentRootType = fileManager.directoryModel.getCurrentRootType();
    const isInMyFiles = currentRootType === RootType.MY_FILES ||
        currentRootType === RootType.DOWNLOADS ||
        currentRootType === RootType.CROSTINI ||
        currentRootType === RootType.ANDROID_FILES;
    event.canExecute = hasAndroidFilesVolumeInfo && isInMyFiles;
    event.command.setHidden(!event.canExecute);
    event.command.checked = fileManager.fileFilter.isAllAndroidFoldersVisible();
  }
}

/**
 * Toggles drive sync settings.
 */
export class DriveSyncSettingsCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
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

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    event.canExecute = fileManager.directoryModel.isOnDrive();
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Delete / Move to Trash command.
 */
export class DeleteCommand extends FilesCommand {
  /**
   */
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    const permanentlyDelete = event.detail.command.id === 'delete';

    // Execute might be called without a call of canExecute method, e.g.,
    // called directly from code, crbug.com/509483. See toolbar controller
    // delete button handling, for an example.
    this.deleteEntries(entries, fileManager, permanentlyDelete);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
   * @param entries
   * @param fileManager
   * @param permanentlyDelete if true, entries are permanently deleted
   *     rather than moved to trash.
   * @param dialog An optional delete confirm dialog.
   *    The default delete confirm dialog will be used if |dialog| is null.
   * @public
   */
  deleteEntries(
      entries: Array<Entry|FilesAppEntry|FakeEntry>,
      fileManager: CommandHandlerDeps, permanentlyDelete: boolean,
      dialog: null|FilesConfirmDialog = null) {
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
          chrome.fileManagerPrivate.IoTaskType.TRASH, entries,
          /*params=*/ {});
      return;
    }

    if (!dialog) {
      dialog = fileManager.ui.deleteConfirmDialog;
    } else if (dialog.showModalElement) {
      dialog.showModalElement();
    }

    const dialogDoneCallback = () => {
      dialog?.doneCallback?.();
      document.querySelector<FilesTooltip>('files-tooltip')?.hideTooltip();
    };

    const deleteAction = () => {
      dialogDoneCallback();
      // Start the permanent delete.
      startIOTask(
          chrome.fileManagerPrivate.IoTaskType.DELETE, entries, /*params=*/ {});
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
          str('CONFIRM_PERMANENTLY_DELETE_ONE_TITLE') :
          str('CONFIRM_PERMANENTLY_DELETE_SOME_TITLE');

      const message = entries.length === 1 ?
          strf('CONFIRM_PERMANENTLY_DELETE_ONE_DESC', entries[0]!.name) :
          strf('CONFIRM_PERMANENTLY_DELETE_SOME_DESC', entries.length);

      dialog.setOkLabel(str('PERMANENTLY_DELETE_FOREVER'));
      dialog.showWithTitle(title, message, deleteAction, cancelAction);
      return;
    }

    const deleteMessage = entries.length === 1 ?
        strf('CONFIRM_DELETE_ONE', entries[0]!.name) :
        strf('CONFIRM_DELETE_SOME', entries.length);
    dialog.setOkLabel(str('DELETE_BUTTON_LABEL'));
    dialog.show(deleteMessage, deleteAction, cancelAction);
  }

  /**
   * Returns true if all entries can be deleted. Note: This does not check for
   * root or fake entries.
   */
  private canDeleteEntries_(
      entries: Array<Entry|FilesAppEntry>,
      fileManager: CommandHandlerDeps): boolean {
    return entries.length > 0 &&
        !this.containsReadOnlyEntry_(entries, fileManager) &&
        hasCapability(fileManager, entries, 'canDelete');
  }

  /**
   * Returns True if entries can be deleted.
   */
  canDeleteEntries(
      entries: Array<Entry|FilesAppEntry>,
      fileManager: CommandHandlerDeps): boolean {
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
   */
  private containsReadOnlyEntry_(
      entries: Array<Entry|FilesAppEntry>,
      fileManager: CommandHandlerDeps): boolean {
    return entries.some(
        entry => isReadOnlyForDelete(fileManager.volumeManager, entry));
  }
}

/**
 * Restores selected files from trash.
 */
export class RestoreFromTrashCommand extends FilesCommand {
  private async execute_(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const entries =
        getCommandEntries(fileManager, event.target) as TrashEntry[];

    const infoEntries = [];
    const failedParents: Array<{fileName: string, parentName: string}> = [];
    for (const entry of entries) {
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
            strf('CANT_RESTORE_SINGLE_ITEM', failedParents[0]!.parentName));
        return;
      }
      // More than one item has been trashed but all the items have their
      // parent removed.
      if (failedParents.length > 1 && infoEntries.length === 0) {
        const isParentFolderSame = failedParents.every(
            p => p.parentName === failedParents[0]!.parentName);
        // All the items were from the same parent folder.
        if (isParentFolderSame) {
          recordEnum(
              RestoreFailedUMA, RestoreFailedType.MULTIPLE_ITEMS_SAME_PARENTS,
              RestoreFailedTypesUMA);
          fileManager.ui.alertDialog.show(strf(
              'CANT_RESTORE_MULTIPLE_ITEMS_SAME_PARENTS',
              failedParents[0]!.parentName));
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
        chrome.fileManagerPrivate.IoTaskType.RESTORE, infoEntries,
        /*params=*/ {});
  }

  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    this.execute_(event, fileManager);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);

    const enabled = entries.length > 0 && entries.every(e => isTrashEntry(e)) &&
        fileManager.trashEnabled;
    event.canExecute = enabled;
    event.command.setHidden(!enabled);
  }

  /**
   * Check whether the parent exists from a supplied entry and return the folder
   * name (if it exists or doesn't).
   * @param entry The entry to identify the parent from.
   *     volumeManager
   */
  async getParentName(entry: Entry, volumeManager: VolumeManager) {
    return new Promise<{exists: boolean, parentName: string}>(
        (resolve, reject) => {
          entry.getParent(
              parent => resolve({exists: true, parentName: parent.name}),
              err => {
                // If this failed, it may be because the parent doesn't exist.
                // Extract the parent from the path components in that case.
                if (err.name === 'NotFoundError') {
                  const components = PathComponent.computeComponentsFromEntry(
                      entry, volumeManager);
                  resolve({
                    exists: false,
                    parentName: components[components.length - 2]?.name ?? '',
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
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.ui.emptyTrashConfirmDialog.showWithTitle(
        str('CONFIRM_EMPTY_TRASH_TITLE'), str('CONFIRM_EMPTY_TRASH_DESC'),
        () => {
          startIOTask(
              chrome.fileManagerPrivate.IoTaskType.EMPTY_TRASH, /*entries=*/[],
              /*params=*/ {});
        });
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    const trashRoot = entries.length === 1 && isTrashRoot(entries[0]!) &&
        fileManager.trashEnabled;
    event.canExecute = trashRoot || isOnTrashRoot(fileManager);
    event.command.setHidden(!trashRoot);
  }
}

/**
 * Pastes files from clipboard.
 */
export class PasteCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    fileManager.document.execCommand(event.detail.command.id);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const fileTransferController = fileManager.fileTransferController;

    event.canExecute = !!fileTransferController &&
        !!fileTransferController.queryPasteCommandEnabled(
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
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.document.execCommand('paste');
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const fileTransferController = fileManager.fileTransferController;

    event.canExecute = !!fileTransferController &&
        !!fileTransferController.queryPasteCommandEnabled(
            fileManager.directoryModel.getCurrentDirEntry());
  }
}

/**
 * Pastes files from clipboard into the selected folder.
 */
export class PasteIntoFolderCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    const entries = getCommandEntries(fileManager, event.target);
    if (entries.length !== 1 || !entries[0]!.isDirectory ||
        !shouldShowMenuItemsForEntry(fileManager.volumeManager, entries[0]!)) {
      return;
    }

    // This handler tweaks the Event object for 'paste' event so that
    // the FileTransferController can distinguish this 'paste-into-folder'
    // command and know the destination directory.
    const handler = (inEvent: PasteWithDestDirectoryEvent) => {
      inEvent.destDirectory = entries[0]!;
    };
    fileManager.document.addEventListener(
        'paste', handler as EventListener, true);
    fileManager.document.execCommand('paste');
    fileManager.document.removeEventListener(
        'paste', handler as EventListener, true);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const entries = getCommandEntries(fileManager, event.target);

    // Show this item only when one directory is selected.
    if (entries.length !== 1 || !entries[0]!.isDirectory ||
        !shouldShowMenuItemsForEntry(fileManager.volumeManager, entries[0]!)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const fileTransferController = fileManager.fileTransferController;
    event.canExecute = !!fileTransferController &&
        !!fileTransferController.queryPasteCommandEnabled(
            entries[0] as DirectoryEntry | FakeEntry);
    event.command.setHidden(false);
  }
}

/**
 * Cut/Copy command.
 */
export class CutCopyCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    // Cancel check-select-mode on cut/copy.  Any further selection of a dir
    // should start a new selection rather than add to the existing selection.
    fileManager.directoryModel.getFileListSelection().setCheckSelectMode(false);
    fileManager.document.execCommand(event.detail.command.id);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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

    /** If the operation is allowed in the Directory Tree. */
    function canDoDirectoryTree(): boolean {
      let entry: Entry|FilesAppEntry|null;
      if (target && 'entry' in target) {
        entry = target.entry as Entry | FilesAppEntry;
      } else if (
          getFocusedTreeItem(target) &&
          getTreeItemEntry(getFocusedTreeItem(target))) {
        entry = getTreeItemEntry(getFocusedTreeItem(target));
      } else {
        return false;
      }

      assert(entry);
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
        return metadata[0]!.canCopy !== false;
      }

      // We need to check source volume is writable for move operation.
      const volumeInfo = volumeManager.getVolumeInfo(entry);
      return !volumeInfo?.isReadOnly && metadata[0]!.canCopy !== false &&
          metadata[0]!.canDelete !== false;
    }

    /** @returns If the operation is allowed in the File List. */
    function canDoFileList() {
      assert(fileManager.document);
      if (shouldIgnoreEvents(fileManager.document)) {
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

      return isMove ? fileTransferController?.canCutOrDrag() :
                      fileTransferController?.canCopyOrDrag();
    }

    const canDo = fileManager.ui.directoryTree?.contains(target as Node) ?
        canDoDirectoryTree() :
        canDoFileList();
    event.canExecute = !!canDo;
    command.disabled = !canDo;
  }
}

/**
 * Initiates file renaming.
 */
export class RenameCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
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
    if (isXfTree(event.target) || isTreeItem(event.target)) {
      assert(fileManager.directoryTreeNamingController);
      assert(volumeInfo);
      if (isXfTree(event.target)) {
        const treeItem = getFocusedTreeItem(event.target);
        assert(treeItem);
        fileManager.directoryTreeNamingController.attachAndStart(
            treeItem, isRemovableRoot, volumeInfo);
      } else if (isTreeItem(event.target)) {
        fileManager.directoryTreeNamingController.attachAndStart(
            event.target, isRemovableRoot, volumeInfo);
      }
    } else {
      fileManager.namingController.initiateRename(isRemovableRoot, volumeInfo);
    }
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
              removable && writable && !!volumeInfo.diskFileSystemType && [
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
        !shouldShowMenuItemsForEntry(fileManager.volumeManager, entries[0]!) ||
        entries.some(isNonModifiable.bind(null, fileManager.volumeManager))) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    assert(renameTarget);
    const parentEntry =
        getParentEntry(renameTarget, fileManager.directoryModel);
    const locationInfo = parentEntry ?
        fileManager.volumeManager.getLocationInfo(parentEntry) :
        null;
    const volumeIsNotReadOnly = !!locationInfo && !locationInfo.isReadOnly;
    // ARC doesn't support rename for now. http://b/232152680
    const recentArcEntry = isRecentArcEntry(unwrapEntry(entries[0]!) as Entry);
    // Drive grand roots do not support rename.
    const isDriveGrandRoot = isGrandRootEntryInDrive(entries[0]!);

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
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.openSettingsSubpage('files');
  }

  override canExecute(
      event: CanExecuteEvent, _fileManager: CommandHandlerDeps) {
    event.canExecute = true;
  }
}

/**
 * Opens drive help.
 */
export class VolumeHelpCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    if (fileManager.directoryModel.isOnDrive()) {
      visitURL(str('GOOGLE_DRIVE_HELP_URL'));
      recordMenuItemSelected(MenuCommandsForUma.DRIVE_HELP);
    } else {
      visitURL(str('FILES_APP_HELP_URL'));
      recordMenuItemSelected(MenuCommandsForUma.HELP);
    }
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.sendFeedback();
  }
}

/**
 * Opens drive buy-more-space url.
 */
export class DriveBuyMoreSpaceCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    visitURL(str('GOOGLE_DRIVE_BUY_STORAGE_URL'));
    recordMenuItemSelected(MenuCommandsForUma.DRIVE_BUY_MORE_SPACE);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    canExecuteVisibleOnDriveInNormalAppModeOnly(event, fileManager);
  }
}

/**
 * Opens drive.google.com.
 */
export class DriveGoToDriveCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    visitURL(str('GOOGLE_DRIVE_ROOT_URL'));
    recordMenuItemSelected(MenuCommandsForUma.DRIVE_GO_TO_DRIVE);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    canExecuteVisibleOnDriveInNormalAppModeOnly(event, fileManager);
  }
}

/**
 * Opens a file with default task.
 */
export class DefaultTaskCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.taskController.executeDefaultTask();
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    event.canExecute = fileManager.taskController.canExecuteDefaultTask();
    event.command.setHidden(fileManager.taskController.shouldHideDefaultTask());
  }
}

/**
 * Displays "open with" dialog for current selection.
 */
export class OpenWithCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    console.error(
        `open-with command doesn't execute, ` +
        `instead it only opens the sub-menu`);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const canExecute = fileManager.taskController.canExecuteOpenActions();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
}

/**
 * Invoke Sharesheet.
 */
export class InvokeSharesheetCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    const entries = fileManager.selectionHandler.selection.entries;
    const launchSource = getSharesheetLaunchSource(event);
    const dlpSourceUrls =
        fileManager.metadataModel.getCache(entries, ['sourceUrl'])
            .map(m => m.sourceUrl || '');
    chrome.fileManagerPrivate
        .invokeSharesheet(entriesToURLs(entries), launchSource, dlpSourceUrls)
        .catch(console.warn);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const entries = fileManager.selectionHandler.selection.entries;

    if (!entries || entries.length === 0 ||
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
    event.command.disabled =
        !fileManager.ui.actionbar.contains(event.target as Node);

    chrome.fileManagerPrivate.sharesheetHasTargets(entriesToURLs(entries))
        .then((hasTargets: boolean) => {
          event.command.setHidden(!hasTargets);
          event.canExecute = hasTargets;
          event.command.disabled = !hasTargets;
        })
        .catch(console.warn);
  }
}

export class ToggleHoldingSpaceCommand extends FilesCommand {
  /**
   * Whether the command adds or removed items from holding space. The
   * value is set in <code>canExecute()</code>. It will be true unless all
   * selected items are already in the holding space.
   */
  private addsItems_?: boolean;

  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    if (this.addsItems_ === undefined) {
      return;
    }

    // Filter out entries from unsupported volumes.
    const allowedVolumeTypes = getAllowedVolumeTypes();
    const entries =
        fileManager.selectionHandler.selection.entries.filter(entry => {
          const volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
          return volumeInfo &&
              allowedVolumeTypes.includes(volumeInfo.volumeType);
        });

    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        entries.map(unwrapEntry) as Entry[], this.addsItems_, () => {});

    if (this.addsItems_) {
      maybeStoreTimeOfFirstPin();
    }

    recordMenuItemSelected(
        this.addsItems_ ? MenuCommandsForUma.PIN_TO_HOLDING_SPACE :
                          MenuCommandsForUma.UNPIN_FROM_HOLDING_SPACE);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const command = event.command;

    const allowedVolumeTypes = getAllowedVolumeTypes();
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

  async checkHoldingSpaceState(
      entries: Array<Entry|FilesAppEntry>, command: Command) {
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
      command.setHidden(true);
      return;
    }

    const itemsSet: Record<string, boolean> = {};
    state.itemUrls.forEach((item: string) => itemsSet[item] = true);

    const selectedUrls = entriesToURLs(entries);
    this.addsItems_ = selectedUrls.some(url => !itemsSet[url]);

    command.label = this.addsItems_ ? str('HOLDING_SPACE_PIN_COMMAND_LABEL') :
                                      str('HOLDING_SPACE_UNPIN_COMMAND_LABEL');
  }
}

/**
 * Opens containing folder of the focused file.
 */
export class GoToFileLocationCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    if (entries.length !== 1) {
      return;
    }

    const components = PathComponent.computeComponentsFromEntry(
        entries[0]!, fileManager.volumeManager);
    // Entries in file list table should always have its containing folder.
    // (i.e. Its path have at least two components: its parent and itself.)
    assert(components.length >= 2);
    const parentComponent = components[components.length - 2];
    parentComponent?.resolveEntry().then(entry => {
      if (entry && isDirectoryEntry(entry)) {
        fileManager.directoryModel.changeDirectoryEntry(entry);
      }
    });
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    // 'get-info' command is executed by 'command' event handler in
    // QuickViewController.
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
  private async executeImpl_(
      _event: CommandEvent, fileManager: CommandHandlerDeps) {
    const entries = fileManager.getSelection().entries;

    const metadata = fileManager.metadataModel.getCache(entries, ['sourceUrl']);
    if (!metadata || metadata.length !== 1 || !metadata[0]!.sourceUrl) {
      return;
    }

    const sourceUrl = metadata[0]!.sourceUrl;
    try {
      const details = await getDlpRestrictionDetails(sourceUrl);
      fileManager.ui.dlpRestrictionDetailsDialog
          ?.showDlpRestrictionDetailsDialog(details);
    } catch (e) {
      console.warn(`Error showing DLP restriction details `, e);
    }
  }

  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    this.executeImpl_(event, fileManager);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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

    const isDlpRestricted = metadata[0]?.isDlpRestricted;
    event.canExecute = !!isDlpRestricted;
    event.command.setHidden(!isDlpRestricted);
  }
}

/**
 * Focuses search input box.
 */
export class SearchCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    // If the current root is Trash we do nothing on search command. Preventing
    // it from execution (in canExecute) does not work correctly, as then chrome
    // start native search for an app window. Thus we always allow it and do
    // nothing in trash.
    const currentRootType = fileManager.directoryModel.getCurrentRootType();
    if (currentRootType !== RootType.TRASH) {
      // Cancel item selection.
      fileManager.directoryModel.clearSelection();
      // Open the query input via the search container.
      fileManager.ui.searchContainer?.openSearch();
    }
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    event.canExecute = !fileManager.namingController.isRenamingInProgress();
  }
}

export class VolumeSwitchCommand extends FilesCommand {
  constructor(private index_: number) {
    super();
  }

  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    const directoryTree = fileManager.ui.directoryTree;
    const items = directoryTree?.items;
    const treeItemEntry = getTreeItemEntry(items && items[this.index_ - 1]);
    if (treeItemEntry) {
      getStore().dispatch(changeDirectory({toKey: treeItemEntry.toURL()}));
    }
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    event.canExecute = this.index_ > 0 &&
        this.index_ <= (fileManager.ui.directoryTree?.items.length ?? 0);
  }
}

/**
 * Flips 'available offline' flag on the file.
 */
export class TogglePinnedCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    const entries = fileManager.getSelection().entries;
    const actionsController = fileManager.actionsController;

    actionsController.getActionsForEntries(entries).then(
        (actionsModel: ActionsModel|void) => {
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

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
      const state = getStore().getState();
      const bulkPinningPref = !!state.preferences?.driveFsBulkPinningEnabled;
      if (bulkPinningPref && isOnlyMyDriveEntries(entries, state)) {
        command.setHidden(true);
        event.canExecute = false;
        return;
      }
    }

    command.setHidden(false);

    function canExecutePinned(actionsModel: ActionsModel|void) {
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
      event.canExecute = !!action && action.canExecute();
      command.disabled = !event.canExecute;
    }

    // Run synchrounously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecutePinned(actionsModel);
      return;
    }

    event.canExecute = true;
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(canExecutePinned);
  }
}

/**
 * Extracts content of ZIP files in the current selection.
 */
export class ExtractAllCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
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
        selectionEntries, dirEntry as DirectoryEntry);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
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
        chrome.fileManagerPrivate.IoTaskType.ZIP, selectionEntries,
        {destinationFolder: dirEntry as DirectoryEntry});
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const dirEntry = fileManager.getCurrentDirectoryEntry();
    const selection = fileManager.getSelection();

    // Hide ZIP selection for single ZIP file selected.
    if (selection.entries.length === 1 &&
        getExtension(selection.entries[0]!) === '.zip') {
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
                (metadata, i) => isEncrypted(
                    selection.entries[i]!, metadata.contentMimeType));

    event.canExecute = !!dirEntry && !fileManager.directoryModel.isReadOnly() &&
        isOnEligibleLocation && selection && selection.totalCount > 0 &&
        !hasEncryptedFile;
  }
}

/**
 * Opens the file in Drive for the user to manage sharing permissions etc.
 */
export class ManageInDriveCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        (actionsModel: ActionsModel|void) => {
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

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    function canExecuteManageInDrive(actionsModel: ActionsModel|void) {
      if (!actionsModel) {
        return;
      }
      const action = actionsModel.getAction(InternalActionId.MANAGE_IN_DRIVE);
      if (action) {
        command.setHidden(!action);
        event.canExecute = !!action && action.canExecute();
        command.disabled = !event.canExecute;
      }
    }

    // Run synchronously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecuteManageInDrive(actionsModel);
      return;
    }

    event.canExecute = true;
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(
        canExecuteManageInDrive);
  }
}

/**
 * Opens the Manage MirrorSync dialog if the flag is enabled.
 */
export class ManageMirrorsyncCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.openManageSyncSettings();
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
   * @param vmName Name of the vm to share into.
   * @param typeForStrings VM type to identify the strings used for this VM e.g.
   *     LINUX or PLUGIN_VM.
   * @param settingsPath Path to the page in settings to manage sharing.
   * @param manageUma MenuCommandsForUma entry this command should emit metrics
   *     under when the toast to manage sharing is clicked on.
   * @param shareUma MenuCommandsForUma entry this command should emit metrics
   *     under.
   */
  constructor(
      private vmName_: string, private typeForStrings_: string,
      private settingsPath_: string, private manageUma_: MenuCommandsForUma,
      private shareUma_: MenuCommandsForUma) {
    super();
    this.validateTranslationStrings_();
  }

  /**
   * Asserts that the necessary strings have been loaded into loadTimeData.
   */
  private validateTranslationStrings_() {
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

  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
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
          this.vmName_, [unwrapEntry(entry) as Entry], true /* persist */,
          () => {
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
    if (entry.fullPath === '/') {
      fileManager.ui.confirmDialog.showHtml(
          str(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_TITLE`),
          strf(
              `SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}`,
              info.volumeInfo?.label),
          share, () => {});
    } else if (
        info.isRootEntry &&
        (info.rootType === RootType.DRIVE ||
         info.rootType === RootType.COMPUTERS_GRAND_ROOT ||
         info.rootType === RootType.SHARED_DRIVES_GRAND_ROOT)) {
      // Only show the dialog for My Drive, Shared Drives Grand Root and
      // Computers Grand Root.  Do not show for roots of a single Shared
      // Drive or Computer.
      fileManager.ui.confirmDialog.showHtml(
          str(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_TITLE`),
          str(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_DRIVE`), share,
          () => {});
    } else {
      // This is not a root, share it without confirmation dialog.
      share();
    }
    recordMenuItemSelected(this.shareUma_);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    // Must be single directory not already shared.
    const entries = getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0]!.isDirectory &&
        !isFakeEntry(entries[0]!) &&
        !fileManager.crostini.isPathShared(this.vmName_, entries[0]!) &&
        fileManager.crostini.canSharePath(
            this.vmName_, entries[0]!, true /* persist */);
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Creates a command for the gear icon to manage sharing.
 */
export class GuestOsManagingSharingGearCommand extends FilesCommand {
  /**
   * @param vmName Name of the vm to share into.
   * @param settingsPath Path to the page in settings to manage sharing.
   * @param manageUma MenuCommandsForUma entry this command should emit metrics
   *     under when the toast to manage sharing is clicked on.
   */
  constructor(
      private vmName_: string, private settingsPath_: string,
      private manageUma_: MenuCommandsForUma) {
    super();
  }
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.openSettingsSubpage(this.settingsPath_);
    recordMenuItemSelected(this.manageUma_);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    event.canExecute = fileManager.crostini.isEnabled(this.vmName_);
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Creates a command for managing sharing.
 */
export class GuestOsManagingSharingCommand extends FilesCommand {
  /**
   * @param vmName Name of the vm to share into.
   * @param settingsPath Path to the page in settings to manage sharing.
   * @param manageUma MenuCommandsForUma entry this command should emit metrics
   *     under when the toast to manage sharing is clicked on.
   */
  constructor(
      private vmName_: string, private settingsPath_: string,
      private manageUma_: MenuCommandsForUma) {
    super();
  }
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.openSettingsSubpage(this.settingsPath_);
    recordMenuItemSelected(this.manageUma_);
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0]!.isDirectory &&
        fileManager.crostini.isPathShared(this.vmName_, entries[0]!);
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Creates a shortcut of the selected folder (single only).
 */
export class PinFolderCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        (actionsModel: ActionsModel|void) => {
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

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    function canExecuteCreateShortcut(actionsModel: ActionsModel|void) {
      if (!actionsModel) {
        return;
      }
      const action =
          actionsModel.getAction(InternalActionId.CREATE_FOLDER_SHORTCUT);
      event.canExecute = !!action && action.canExecute();
      command.disabled = !event.canExecute;
      command.setHidden(!action);
    }

    // Run synchrounously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecuteCreateShortcut(actionsModel);
      return;
    }

    event.canExecute = true;
    command.setHidden(false);
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(
        canExecuteCreateShortcut);
  }
}

/**
 * Removes the folder shortcut.
 */
export class UnpinFolderCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        (actionsModel: ActionsModel|void) => {
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

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const entries = getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    function canExecuteRemoveShortcut(actionsModel: ActionsModel|void) {
      if (!actionsModel) {
        return;
      }
      const action =
          actionsModel.getAction(InternalActionId.REMOVE_FOLDER_SHORTCUT);
      command.setHidden(!action);
      event.canExecute = !!action && action.canExecute();
      command.disabled = !event.canExecute;
    }

    // Run synchrounously if possible.
    const actionsModel =
        actionsController.getInitializedActionsForEntries(entries);
    if (actionsModel) {
      canExecuteRemoveShortcut(actionsModel);
      return;
    }

    event.canExecute = true;
    command.setHidden(false);
    // Run async, otherwise.
    actionsController.getActionsForEntries(entries).then(
        canExecuteRemoveShortcut);
  }
}

/**
 * Zoom in to the Files app.
 */
export class ZoomInCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.IN);
  }
}

/**
 * Zoom out from the Files app.
 */
export class ZoomOutCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.OUT);
  }
}

/**
 * Reset the zoom factor.
 */
export class ZoomResetCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.RESET);
  }
}

/**
 * Sort the file list by name (in ascending order).
 */
export class SortByNameCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
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
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
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
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
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
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
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
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.NORMAL);
  }
}

/**
 * Open inspector for foreground page and bring focus to the console.
 */
export class InspectConsoleCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.CONSOLE);
  }
}

/**
 * Open inspector for foreground page in inspect element mode.
 */
export class InspectElementCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.ELEMENT);
  }
}

/**
 * Opens the gear menu.
 */
export class OpenGearMenuCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.ui.gearButton.showMenu(true);
  }
}

/**
 * Focus the first button visible on action bar (at the top).
 */
export class FocusActionBarCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.ui.actionbar
        .querySelector<HTMLButtonElement|CrButtonElement>(
            'button:not([hidden]), cr-button:not([hidden])')
        ?.focus();
  }
}

/**
 * Handle back button.
 */
export class BrowserBackCommand extends FilesCommand {
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    // TODO(fukino): It should be better to minimize Files app only when there
    // is no back stack, and otherwise use BrowserBack for history navigation.
    // https://crbug.com/624100.
    // TODO(crbug.com/40701086): Implement minimize for files SWA, then
    // call its minimize() function here.
  }
}

/**
 * Configures the currently selected volume.
 */
export class ConfigureCommand extends FilesCommand {
  execute(event: CommandEvent, fileManager: CommandHandlerDeps) {
    const volumeInfo = getElementVolumeInfo(event.target, fileManager);
    if (volumeInfo && volumeInfo.configurable) {
      fileManager.volumeManager.configure(volumeInfo);
    }
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const volumeInfo = getElementVolumeInfo(event.target, fileManager);
    event.canExecute = !!volumeInfo && volumeInfo.configurable;
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Refreshes the currently selected directory.
 */
export class RefreshCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.directoryModel.rescan(true /* refresh */);
    fileManager.spinnerController.blink();
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    const currentDirEntry = fileManager.directoryModel.getCurrentDirEntry();
    const volumeInfo = currentDirEntry &&
        fileManager.volumeManager.getVolumeInfo(currentDirEntry);
    event.canExecute = !!volumeInfo && !volumeInfo.watchable;
    event.command.setHidden(
        !event.canExecute ||
        fileManager.directoryModel.getFileListSelection().getCheckSelectMode());
  }
}

/**
 * Sets the system wallpaper to the selected file.
 */
export class SetWallpaperCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    if (isOnTrashRoot(fileManager)) {
      return;
    }
    const entry = fileManager.getSelection().entries[0] as FileEntry;
    new Promise<File>((resolve, reject) => {
      entry.file(resolve, reject);
    })
        .then((blob: File) => {
          const fileReader = new FileReader();
          return new Promise<ArrayBuffer|null|undefined>((resolve, reject) => {
            fileReader.onload = () => {
              resolve(fileReader.result as ArrayBuffer);
            };
            fileReader.onerror = () => {
              reject(fileReader.error);
            };
            fileReader.readAsArrayBuffer(blob);
          });
        })
        .then((arrayBuffer: ArrayBuffer|null|undefined) => {
          assert(arrayBuffer);
          return chrome.wallpaper.setWallpaper({
            data: arrayBuffer,
            layout: chrome.wallpaper.WallpaperLayout.CENTER_CROPPED,
            filename: 'wallpaper',
          });
        })
        .catch(() => {
          fileManager.ui.alertDialog.showHtml(
              '', str('ERROR_INVALID_WALLPAPER'));
        });
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
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
    const type = getType(entries[0]!);
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
  execute(_event: CommandEvent, _fileManager: CommandHandlerDeps) {
    chrome.fileManagerPrivate.openSettingsSubpage('storage');
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    event.canExecute = false;
    const currentVolumeInfo = fileManager.directoryModel.getCurrentVolumeInfo();
    if (!currentVolumeInfo) {
      return;
    }

    // Can execute only for local file systems.
    if (currentVolumeInfo.volumeType === VolumeType.MY_FILES ||
        currentVolumeInfo.volumeType === VolumeType.DOWNLOADS ||
        currentVolumeInfo.volumeType === VolumeType.CROSTINI ||
        currentVolumeInfo.volumeType === VolumeType.GUEST_OS ||
        currentVolumeInfo.volumeType === VolumeType.ANDROID_FILES ||
        currentVolumeInfo.volumeType === VolumeType.DOCUMENTS_PROVIDER) {
      event.canExecute = true;
    }
  }
}
/**
 * Opens "providers menu" to allow users to use providers/FSPs.
 */
export class ShowProvidersSubmenuCommand extends FilesCommand {
  execute(_event: CommandEvent, fileManager: CommandHandlerDeps) {
    fileManager.ui.gearButton.showSubMenu();
  }

  override canExecute(event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
    if (fileManager.dialogType !== DialogType.FULL_PAGE) {
      event.canExecute = false;
    } else {
      event.canExecute = !fileManager.guestMode;
    }
  }
}
