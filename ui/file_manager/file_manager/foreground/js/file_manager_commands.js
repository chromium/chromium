// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_input/cr_input.js';

import {assert} from 'chrome://resources/ash/common/assert.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';

import {getDlpRestrictionDetails, getHoldingSpaceState, startIOTask} from '../../common/js/api.js';
import {DialogType, isModal} from '../../common/js/dialog_type.js';
import {FileType} from '../../common/js/file_type.js';
import {EntryList} from '../../common/js/files_app_entry_types.js';
import {metrics} from '../../common/js/metrics.js';
import {deleteIsForever, RestoreFailedType, RestoreFailedTypesUMA, RestoreFailedUMA, shouldMoveToTrash, TrashEntry} from '../../common/js/trash.js';
import {str, strf, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {NudgeType} from '../../containers/nudge_container.js';
import {CommandHandlerDeps} from '../../externs/command_handler_deps.js';
import {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../externs/files_app_entry_interfaces.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {XfDlpRestrictionDetailsDialog} from '../../widgets/xf_dlp_restriction_details_dialog.js';

import {ActionsModel} from './actions_model.js';
import {constants} from './constants.js';
import {DirectoryModel} from './directory_model.js';
import {FileSelection, FileSelectionHandler} from './file_selection.js';
import {TaskPickerType} from './file_tasks.js';
import {HoldingSpaceUtil} from './holding_space_util.js';
import {PathComponent} from './path_component.js';
import {Command} from './ui/command.js';
import {contextMenuHandler} from './ui/context_menu_handler.js';
import {DirectoryItem, DirectoryTree} from './ui/directory_tree.js';
import {FilesConfirmDialog} from './ui/files_confirm_dialog.js';
import {List} from './ui/list.js';


/**
 * A command.
 * @abstract
 */
export class FilesCommand {
  /**
   * Handles the execute event.
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
   * @abstract
   */
  execute(event, fileManager) {}

  /**
   * Handles the can execute event.
   * By default, sets the command as always enabled.
   * @param {!Event} event Can execute event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
   */
  canExecute(event, fileManager) {
    event.canExecute = true;
  }
}

/**
 * Utility for commands.
 */
const CommandUtil = {};

/**
 * The IDs of elements that can trigger share action.
 * @enum {string}
 * @const
 */
CommandUtil.SharingActionElementId = {
  CONTEXT_MENU: 'file-list',
  SHARE_SHEET: 'sharesheet-button',
};

/**
 * Helper function that for the given event returns the launch source of the
 * sharesheet. If the source cannot be determined, this function returns
 * chrome.fileManagerPrivate.SharesheetLaunchSource.UNKNOWN.
 * @param {!Event} event The event that triggered the sharesheet.
 * @return {!chrome.fileManagerPrivate.SharesheetLaunchSource}
 */
CommandUtil.getSharesheetLaunchSource = event => {
  const id = event.target.id;
  switch (id) {
    case CommandUtil.SharingActionElementId.CONTEXT_MENU:
      return chrome.fileManagerPrivate.SharesheetLaunchSource.CONTEXT_MENU;
    case CommandUtil.SharingActionElementId.SHARE_SHEET:
      return chrome.fileManagerPrivate.SharesheetLaunchSource.SHARESHEET_BUTTON;
    default: {
      console.error('Unrecognized event.target.id for sharesheet launch', id);
      return chrome.fileManagerPrivate.SharesheetLaunchSource.UNKNOWN;
    }
  }
};

/**
 * Extracts entry on which command event was dispatched.
 *
 * @param {!CommandHandlerDeps} fileManager
 * @param {EventTarget} element Element which is the command event's target.
 * @return {Entry|FakeEntry} Entry of the found node.
 */
CommandUtil.getCommandEntry = (fileManager, element) => {
  const entries = CommandUtil.getCommandEntries(fileManager, element);
  return entries.length === 0 ? null : entries[0];
};

/**
 * Extracts entries on which command event was dispatched.
 *
 * @param {!CommandHandlerDeps} fileManager
 * @param {EventTarget} element Element which is the command event's target.
 * @return {!Array<!Entry>} Entries of the found node.
 */
CommandUtil.getCommandEntries = (fileManager, element) => {
  // DirectoryItem has "entry" attribute.
  if (element && element.entry) {
    return [element.entry];
  }

  // DirectoryTree has the selected item.
  if (element.selectedItem && element.selectedItem.entry) {
    return [element.selectedItem.entry];
  }

  // The event target could still be a descendant of a DirectoryItem element
  // (e.g. the eject button).
  if (fileManager.ui.directoryTree.contains(/** @type {Node} */ (element))) {
    const treeItem = element.closest('.tree-item');
    if (treeItem && treeItem.entry) {
      return [treeItem.entry];
    }
  }

  // File list (List).
  if (element.selectedItems && element.selectedItems.length) {
    const entries = element.selectedItems;
    // Check if it is Entry or not by checking for toURL().
    return entries.filter(entry => ('toURL' in entry));
  }

  // Commands in the action bar can only act in the currently selected files.
  if (fileManager.ui.actionbar.contains(/** @type {Node} */ (element))) {
    return fileManager.getSelection().entries;
  }

  // Context Menu: redirect to the element the context menu is displayed for.
  if (element.contextElement) {
    return CommandUtil.getCommandEntries(fileManager, element.contextElement);
  }

  // Context Menu Item: redirect to the element the context menu is displayed
  // for.
  if (element.parentElement.contextElement) {
    return CommandUtil.getCommandEntries(
        fileManager, element.parentElement.contextElement);
  }

  return [];
};

/**
 * Extracts a directory which contains entries on which command event was
 * dispatched.
 *
 * @param {EventTarget} element Element which is the command event's target.
 * @param {DirectoryModel} directoryModel
 * @return {DirectoryEntry|FilesAppEntry} The extracted parent entry.
 */
CommandUtil.getParentEntry = (element, directoryModel) => {
  if (element && element.selectedItem && element.selectedItem.parentItem &&
      element.selectedItem.parentItem.entry) {
    // DirectoryTree has the selected item.
    return element.selectedItem.parentItem.entry;
  } else if (element.parentItem && element.parentItem.entry) {
    // DirectoryItem has parentItem.
    return element.parentItem.entry;
  } else if (element instanceof List) {
    return directoryModel ? directoryModel.getCurrentDirEntry() : null;
  } else {
    return null;
  }
};

/**
 * Returns VolumeInfo from the current target for commands, based on |element|.
 * It can be from directory tree (clicked item or selected item), or from file
 * list selected items; or null if can determine it.
 *
 * @param {EventTarget} element
 * @param {!CommandHandlerDeps} fileManager
 * @return {VolumeInfo}
 */
CommandUtil.getElementVolumeInfo = (element, fileManager) => {
  if (element.volumeInfo) {
    return element.volumeInfo;
  }
  const entry = CommandUtil.getCommandEntry(fileManager, element);
  return entry && fileManager.volumeManager.getVolumeInfo(entry);
};

/**
 * Sets the command as visible only when the current volume is drive and it's
 * running as a normal app, not as a modal dialog.
 * NOTE: This doesn't work for directory tree menu, because user can right-click
 * on any visible volume.
 * @param {!Event} event Command event to mark.
 * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
 */
CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly =
    (event, fileManager) => {
      const enabled = fileManager.directoryModel.isOnDrive() &&
          !isModal(fileManager.dialogType);
      event.canExecute = enabled;
      event.command.setHidden(!enabled);
    };

/**
 * Sets the default handler for the commandId and prevents handling
 * the keydown events for this command. Not doing that breaks relationship
 * of original keyboard event and the command. WebKit would handle it
 * differently in some cases.
 * @param {Node} node to register command handler on.
 * @param {string} commandId Command id to respond to.
 */
CommandUtil.forceDefaultHandler = (node, commandId) => {
  const doc = node.ownerDocument;
  const command = /** @type {!Command} */ (
      doc.body.querySelector('command[id="' + commandId + '"]'));
  node.addEventListener('keydown', e => {
    if (command.matchesEvent(e)) {
      // Prevent CommandManager of handling it and leave it
      // for the default handler.
      e.stopPropagation();
    }
  });
  node.addEventListener('command', event => {
    if (event.command.id !== commandId) {
      return;
    }
    document.execCommand(event.command.id);
    event.cancelBubble = true;
  });
  node.addEventListener('canExecute', event => {
    if (event.command.id !== commandId || event.target !== node) {
      return;
    }
    event.canExecute = document.queryCommandEnabled(event.command.id);
    event.command.setHidden(false);
  });
};

/**
 * Creates the volume switch command with index.
 * @param {number} index Volume index from 1 to 9.
 * @return {FilesCommand} Volume switch command.
 */
CommandUtil.createVolumeSwitchCommand = index =>
    new (class extends FilesCommand {
      execute(event, fileManager) {
        fileManager.directoryTree.activateByIndex(index - 1);
      }

      /** @override */
      canExecute(event, fileManager) {
        event.canExecute =
            index > 0 && index <= fileManager.directoryTree.items.length;
      }
    })();

/**
 * Returns a directory entry when only one entry is selected and it is
 * directory. Otherwise, returns null.
 * @param {FileSelection} selection Instance of FileSelection.
 * @return {?DirectoryEntry} Directory entry which is selected alone.
 */
CommandUtil.getOnlyOneSelectedDirectory = selection => {
  if (!selection) {
    return null;
  }
  if (selection.totalCount !== 1) {
    return null;
  }
  if (!selection.entries[0].isDirectory) {
    return null;
  }
  return /** @type {!DirectoryEntry} */ (selection.entries[0]);
};

/**
 * Returns true if the given entry is the root entry of the volume.
 * @param {!VolumeManager} volumeManager
 * @param {(!Entry|!FakeEntry)} entry Entry or a fake entry.
 * @return {boolean} True if the entry is a root entry.
 */
CommandUtil.isRootEntry = (volumeManager, entry) => {
  if (!volumeManager || !entry) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  return !!volumeInfo && util.isSameEntry(volumeInfo.displayRoot, entry);
};

/**
 * Returns true if the given event was triggered by the selection menu button.
 * @event {!Event} event Command event.
 * @return {boolean} Ture if the event was triggered by the selection menu
 * button.
 */
CommandUtil.isFromSelectionMenu = event => {
  return event.target.id == 'selection-menu-button';
};

/**
 * If entry is fake/invalid/root, we don't show menu items intended for regular
 * entries.
 * @param {!VolumeManager} volumeManager
 * @param {(!Entry|!FakeEntry)} entry Entry or a fake entry.
 * @return {boolean} True if we should show the menu items for regular entries.
 */
CommandUtil.shouldShowMenuItemsForEntry = (volumeManager, entry) => {
  // If the entry is fake entry, hide context menu entries.
  if (util.isFakeEntry(entry)) {
    return false;
  }

  // If the entry is not a valid entry, hide context menu entries.
  if (!volumeManager) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  if (!volumeInfo) {
    return false;
  }

  // If the entry is root entry of its volume (but not a team drive root),
  // hide context menu entries.
  if (CommandUtil.isRootEntry(volumeManager, entry) &&
      !util.isTeamDriveRoot(entry)) {
    return false;
  }

  if (util.isTeamDrivesGrandRoot(entry)) {
    return false;
  }

  return true;
};

/**
 * Returns whether all of the given entries have the given capability.
 *
 * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
 * @param {!Array<Entry>} entries List of entries to check capabilities for.
 * @param {string} capability Name of the capability to check for.
 */
CommandUtil.hasCapability = (fileManager, entries, capability) => {
  if (entries.length == 0) {
    return false;
  }

  // Check if the capability is true or undefined, but not false. A capability
  // can be undefined if the metadata is not fetched from the server yet (e.g.
  // if we create a new file in offline mode), or if there is a problem with the
  // cache and we don't have data yet. For this reason, we need to allow the
  // functionality even if it's not set.
  // TODO(crbug.com/849999): Store restrictions instead of capabilities.
  const metadata = fileManager.metadataModel.getCache(entries, [capability]);
  return metadata.length === entries.length &&
      metadata.every(item => item[capability] !== false);
};

/**
 * Checks if the handler should ignore the current event, eg. since there is
 * a popup dialog currently opened.
 *
 * @param {!Document} doc
 * @return {boolean} True if the event should be ignored, false otherwise.
 */
CommandUtil.shouldIgnoreEvents = function(doc) {
  // Do not handle commands, when a dialog is shown. Do not use querySelector
  // as it's much slower, and this method is executed often.
  const dialogs = doc.getElementsByClassName('cr-dialog-container');
  if (dialogs.length !== 0 && dialogs[0].classList.contains('shown')) {
    return true;
  }

  return false;  // Do not ignore.
};

/**
 * Returns true if all entries is inside Drive volume, which includes all Drive
 * parts (Shared Drives, My Drive, Shared with me, etc).
 *
 * @param {!Array<!Entry|!FilesAppEntry>} entries
 * @param {!VolumeManager} volumeManager
 * @return {boolean}
 */
CommandUtil.isDriveEntries = (entries, volumeManager) => {
  if (!entries.length) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entries[0]);
  if (!volumeInfo) {
    return false;
  }

  if (volumeInfo.volumeType === VolumeManagerCommon.VolumeType.DRIVE &&
      util.isSameVolume(entries, volumeManager)) {
    return true;
  }

  return false;
};


/**
 * Extracts entry on which command event was dispatched.
 *
 * @param {!Event} event Command event to mark.
 * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps to use.
 * @return {Entry|FilesAppDirEntry} Entry of the event node.
 */
CommandUtil.getEventEntry = (event, fileManager) => {
  let entry;
  if (fileManager.ui.directoryTree.contains(
          /** @type {Node} */ (event.target))) {
    // The command is executed from the directory tree context menu.
    entry = CommandUtil.getCommandEntry(fileManager, event.target);
  } else {
    // The command is executed from the gear menu.
    entry = fileManager.directoryModel.getCurrentDirEntry();
  }
  return entry;
};


/**
 * Handle of the command events.
 */
export class CommandHandler {
  /**
   * @param {!CommandHandlerDeps} fileManager Classes |CommandHalder| depends.
   * @param {!FileSelectionHandler} selectionHandler
   */
  constructor(fileManager, selectionHandler) {
    /**
     * CommandHandlerDeps.
     * @private @const {!CommandHandlerDeps}
     */
    this.fileManager_ = fileManager;

    /**
     * Command elements.
     * @private @const {Object<Command>}
     */
    this.commands_ = {};

    /** @private {?Element} */
    this.lastFocusedElement_ = null;

    // Decorate command tags in the document.
    const commands = fileManager.document.querySelectorAll('command');

    for (let i = 0; i < commands.length; i++) {
      if (Command.decorate) {
        Command.decorate(commands[i]);
      }
      this.commands_[commands[i].id] = commands[i];
    }

    // Register events.
    fileManager.document.addEventListener(
        'command', this.onCommand_.bind(this));
    fileManager.document.addEventListener(
        'canExecute', this.onCanExecute_.bind(this));

    contextMenuHandler.addEventListener(
        'show', this.onContextMenuShow_.bind(this));
    contextMenuHandler.addEventListener(
        'hide', this.onContextMenuHide_.bind(this));
  }

  /** @param {!Event} event */
  onContextMenuShow_(event) {
    this.lastFocusedElement_ = document.activeElement;
    const menu = event.menu;
    // Set focus asynchronously to give time for menu "show" event to finish and
    // have all items set up before focusing.
    setTimeout(() => {
      if (!menu.hidden) {
        menu.focusSelectedItem();
      }
    }, 0);
  }

  /** @param {!Event} event */
  onContextMenuHide_(event) {
    if (this.lastFocusedElement_) {
      const activeElement = document.activeElement;
      if (activeElement && activeElement.tagName === 'BODY') {
        this.lastFocusedElement_.focus();
      }
      this.lastFocusedElement_ = null;
    }
  }

  /**
   * Handles command events.
   * @param {!Event} event Command event.
   * @private
   */
  onCommand_(event) {
    if (CommandUtil.shouldIgnoreEvents(assert(this.fileManager_.document))) {
      return;
    }
    const handler = CommandHandler.COMMANDS_[event.command.id];
    handler.execute.call(
        /** @type {FilesCommand} */ (handler), event, this.fileManager_);
  }

  /**
   * Handles canExecute events.
   * @param {!Event} event Can execute event.
   * @private
   */
  onCanExecute_(event) {
    if (CommandUtil.shouldIgnoreEvents(assert(this.fileManager_.document))) {
      return;
    }
    const handler = CommandHandler.COMMANDS_[event.command.id];
    handler.canExecute.call(
        /** @type {FilesCommand} */ (handler), event, this.fileManager_);
  }

  /**
   * Returns command handler by name.
   * @param {string} name The command name.
   * @public
   */
  static getCommand(name) {
    return CommandHandler.COMMANDS_[name];
  }
}

/**
 * Supported disk file system types for renaming.
 * @private @const {!Array<!VolumeManagerCommon.FileSystemType>}
 */
CommandHandler.RENAME_DISK_FILE_SYSTEM_SUPPORT_ = [
  VolumeManagerCommon.FileSystemType.EXFAT,
  VolumeManagerCommon.FileSystemType.VFAT,
  VolumeManagerCommon.FileSystemType.NTFS,
];

/**
 * Name of a command (for UMA).
 * @enum {string}
 * @const
 */
CommandHandler.MenuCommandsForUMA = {
  HELP: 'volume-help',
  DRIVE_HELP: 'volume-help-drive',
  DRIVE_BUY_MORE_SPACE: 'drive-buy-more-space',
  DRIVE_GO_TO_DRIVE: 'drive-go-to-drive',
  HIDDEN_FILES_SHOW: 'toggle-hidden-files-on',
  HIDDEN_FILES_HIDE: 'toggle-hidden-files-off',
  MOBILE_DATA_ON: 'drive-sync-settings-enabled',
  MOBILE_DATA_OFF: 'drive-sync-settings-disabled',
  DEPRECATED_SHOW_GOOGLE_DOCS_FILES_OFF: 'drive-hosted-settings-disabled',
  DEPRECATED_SHOW_GOOGLE_DOCS_FILES_ON: 'drive-hosted-settings-enabled',
  HIDDEN_ANDROID_FOLDERS_SHOW: 'toggle-hidden-android-folders-on',
  HIDDEN_ANDROID_FOLDERS_HIDE: 'toggle-hidden-android-folders-off',
  SHARE_WITH_LINUX: 'share-with-linux',
  MANAGE_LINUX_SHARING: 'manage-linux-sharing',
  MANAGE_LINUX_SHARING_TOAST: 'manage-linux-sharing-toast',
  MANAGE_LINUX_SHARING_TOAST_STARTUP: 'manage-linux-sharing-toast-startup',
  SHARE_WITH_PLUGIN_VM: 'share-with-plugin-vm',
  MANAGE_PLUGIN_VM_SHARING: 'manage-plugin-vm-sharing',
  MANAGE_PLUGIN_VM_SHARING_TOAST: 'manage-plugin-vm-sharing-toast',
  MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP:
      'manage-plugin-vm-sharing-toast-startup',
  PIN_TO_HOLDING_SPACE: 'pin-to-holding-space',
  UNPIN_FROM_HOLDING_SPACE: 'unpin-from-holding-space',
  SHARE_WITH_BRUSCHETTA: 'share-with-bruschetta',
  MANAGE_BRUSCHETTA_SHARING: 'manage-bruschetta-sharing',
  MANAGE_BRUSCHETTA_SHARING_TOAST: 'manage-bruschetta-sharing-toast',
  MANAGE_BRUSCHETTA_SHARING_TOAST_STARTUP:
      'manage-bruschetta-sharing-toast-startup',
};

/**
 * Keep the order of this in sync with FileManagerMenuCommands in
 * tools/metrics/histograms/enums.xml.
 * The array indices will be recorded in UMA as enum values. The index for each
 * root type should never be renumbered nor reused in this array.
 *
 * @const {!Array<CommandHandler.MenuCommandsForUMA>}
 */
CommandHandler.ValidMenuCommandsForUMA = [
  CommandHandler.MenuCommandsForUMA.HELP,
  CommandHandler.MenuCommandsForUMA.DRIVE_HELP,
  CommandHandler.MenuCommandsForUMA.DRIVE_BUY_MORE_SPACE,
  CommandHandler.MenuCommandsForUMA.DRIVE_GO_TO_DRIVE,
  CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_SHOW,
  CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_HIDE,
  CommandHandler.MenuCommandsForUMA.MOBILE_DATA_ON,
  CommandHandler.MenuCommandsForUMA.MOBILE_DATA_OFF,
  CommandHandler.MenuCommandsForUMA.DEPRECATED_SHOW_GOOGLE_DOCS_FILES_OFF,
  CommandHandler.MenuCommandsForUMA.DEPRECATED_SHOW_GOOGLE_DOCS_FILES_ON,
  CommandHandler.MenuCommandsForUMA.HIDDEN_ANDROID_FOLDERS_SHOW,
  CommandHandler.MenuCommandsForUMA.HIDDEN_ANDROID_FOLDERS_HIDE,
  CommandHandler.MenuCommandsForUMA.SHARE_WITH_LINUX,
  CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING,
  CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST,
  CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST_STARTUP,
  CommandHandler.MenuCommandsForUMA.SHARE_WITH_PLUGIN_VM,
  CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING,
  CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING_TOAST,
  CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP,
  CommandHandler.MenuCommandsForUMA.PIN_TO_HOLDING_SPACE,
  CommandHandler.MenuCommandsForUMA.UNPIN_FROM_HOLDING_SPACE,
  CommandHandler.MenuCommandsForUMA.SHARE_WITH_BRUSCHETTA,
  CommandHandler.MenuCommandsForUMA.MANAGE_BRUSCHETTA_SHARING,
  CommandHandler.MenuCommandsForUMA.MANAGE_BRUSCHETTA_SHARING_TOAST,
  CommandHandler.MenuCommandsForUMA.MANAGE_BRUSCHETTA_SHARING_TOAST_STARTUP,
];
console.assert(
    Object.keys(CommandHandler.MenuCommandsForUMA).length ===
        CommandHandler.ValidMenuCommandsForUMA.length,
    'Members in ValidMenuCommandsForUMA do not match those in ' +
        'MenuCommandsForUMA.');

/**
 * Records the menu item as selected in UMA.
 * @param {CommandHandler.MenuCommandsForUMA} menuItem The selected menu item.
 */
CommandHandler.recordMenuItemSelected = menuItem => {
  metrics.recordEnum(
      'MenuItemSelected', menuItem, CommandHandler.ValidMenuCommandsForUMA);
};

/**
 * Commands.
 * @private @const {Object<FilesCommand>}
 */
CommandHandler.COMMANDS_ = {};

/**
 * Unmounts external drive.
 */
CommandHandler.COMMANDS_['unmount'] = new (class extends FilesCommand {
  /**
   * @param {!Event} event Command event.
   * @param {!CommandHandlerDeps} fileManager CommandHandlerDeps.
   * @private
   */
  async executeImpl_(event, fileManager) {
    /** @param {VolumeManagerCommon.VolumeType=} opt_volumeType */
    const errorCallback = opt_volumeType => {
      if (opt_volumeType === VolumeManagerCommon.VolumeType.REMOVABLE) {
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
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
    if (entry instanceof EntryList) {
      // The element is a group of removable partitions.
      if (!entry) {
        errorCallback();
        return;
      }
      // Add child partitions to the list of volumes to be unmounted.
      volumes = entry.getUIChildren().map(child => child.volumeInfo);
      label = entry.label || '';
    } else {
      // The element is a removable volume with no partitions.
      const volumeInfo =
          CommandUtil.getElementVolumeInfo(event.target, fileManager);
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
        if (error != VolumeManagerCommon.VolumeError.PATH_NOT_MOUNTED) {
          errorCallback(volume.volumeType);
        }
      }
    });

    await Promise.all(promises);
    fileManager.ui.speakA11yMessage(strf('A11Y_VOLUME_EJECT', label));
  }

  execute(event, fileManager) {
    this.executeImpl_(event, fileManager);
  }

  /** @override */
  canExecute(event, fileManager) {
    const volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager);
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);

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
        (volumeType === VolumeManagerCommon.VolumeType.ARCHIVE ||
         volumeType === VolumeManagerCommon.VolumeType.REMOVABLE ||
         volumeType === VolumeManagerCommon.VolumeType.PROVIDED ||
         volumeType === VolumeManagerCommon.VolumeType.SMB);
    event.command.setHidden(!event.canExecute);

    switch (volumeType) {
      case VolumeManagerCommon.VolumeType.ARCHIVE:
      case VolumeManagerCommon.VolumeType.PROVIDED:
      case VolumeManagerCommon.VolumeType.SMB:
        event.command.label = str('CLOSE_VOLUME_BUTTON_LABEL');
        break;
      case VolumeManagerCommon.VolumeType.REMOVABLE:
        event.command.label = str('UNMOUNT_DEVICE_BUTTON_LABEL');
        break;
    }
  }
})();

/**
 * Formats external drive.
 */
CommandHandler.COMMANDS_['format'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const directoryModel = fileManager.directoryModel;
    let root;
    if (fileManager.ui.directoryTree.contains(
            /** @type {Node} */ (event.target))) {
      // The command is executed from the directory tree context menu.
      root = CommandUtil.getCommandEntry(fileManager, event.target);
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
  canExecute(event, fileManager) {
    const directoryModel = fileManager.directoryModel;
    let root;
    if (fileManager.ui.directoryTree.contains(
            /** @type {Node} */ (event.target))) {
      // The command is executed from the directory tree context menu.
      root = CommandUtil.getCommandEntry(fileManager, event.target);
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
    const removableRoot = location && isRoot &&
        location.rootType === VolumeManagerCommon.RootType.REMOVABLE;
    event.canExecute = removableRoot && (isUnrecognizedVolume || writable);

    if (util.isSinglePartitionFormatEnabled()) {
      let isDevice = false;
      if (root && root instanceof EntryList) {
        // root entry is device node if it has child (partition).
        isDevice = !!removableRoot && root.getUIChildren().length > 0;
      }
      // Disable format command on device when SinglePartitionFormat on,
      // erase command will be available.
      event.command.setHidden(!removableRoot || isDevice);
    } else {
      event.command.setHidden(!removableRoot);
    }
  }
})();

/**
 * Deletes removable device partition, creates single partition and formats it.
 */
CommandHandler.COMMANDS_['erase-device'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const root = CommandUtil.getEventEntry(event, fileManager);

    if (root && root instanceof EntryList) {
      /** @type {FilesFormatDialogElement} */ (fileManager.ui.formatDialog)
          .showEraseModal(root);
    }
  }

  /** @override */
  canExecute(event, fileManager) {
    if (!util.isSinglePartitionFormatEnabled()) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const root = CommandUtil.getEventEntry(event, fileManager);
    const location = root && fileManager.volumeManager.getLocationInfo(root);
    const writable = location && !location.isReadOnly;
    const isRoot = location && location.isRootEntry;

    const removableRoot = location && isRoot &&
        location.rootType === VolumeManagerCommon.RootType.REMOVABLE;

    let isDevice = false;
    if (root && root instanceof EntryList) {
      // root entry is device node if it has child (partition).
      isDevice = !!removableRoot && root.getUIChildren().length > 0;
    }

    event.canExecute = removableRoot && !writable;
    // Enable the command if this is a removable and device node.
    event.command.setHidden(!removableRoot || !isDevice);
  }
})();

/**
 * Initiates new folder creation.
 */
CommandHandler.COMMANDS_['new-folder'] = new (class extends FilesCommand {
  constructor() {
    super();

    /**
     * Whether a new-folder is in progress.
     * @private {boolean}
     */
    this.busy_ = false;
  }

  execute(event, fileManager) {
    let targetDirectory;
    let executedFromDirectoryTree;

    if (event.target instanceof DirectoryTree) {
      targetDirectory = event.target.selectedItem.entry;
      executedFromDirectoryTree = true;
    } else if (event.target instanceof DirectoryItem) {
      targetDirectory = event.target.entry;
      executedFromDirectoryTree = true;
    } else {
      targetDirectory = fileManager.directoryModel.getCurrentDirEntry();
      executedFromDirectoryTree = false;
    }

    const directoryModel = fileManager.directoryModel;
    const directoryTree = fileManager.ui.directoryTree;
    const listContainer = fileManager.ui.listContainer;
    this.busy_ = true;

    this.generateNewDirectoryName_(targetDirectory).then((newName) => {
      if (!executedFromDirectoryTree) {
        listContainer.startBatchUpdates();
      }

      return new Promise(
                 targetDirectory.getDirectory.bind(
                     targetDirectory, newName, {create: true, exclusive: true}))
          .then(
              (newDirectory) => {
                metrics.recordUserAction('CreateNewFolder');

                // Select new directory and start rename operation.
                if (executedFromDirectoryTree) {
                  directoryTree.updateAndSelectNewDirectory(
                      targetDirectory, newDirectory);
                  fileManager.directoryTreeNamingController.attachAndStart(
                      assert(fileManager.ui.directoryTree.selectedItem), false,
                      null);
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

                fileManager.ui.alertDialog.show(
                    strf(
                        'ERROR_CREATING_FOLDER', newName,
                        util.getFileErrorString(error.name)),
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
  generateNewDirectoryName_(parentDirectory, opt_index) {
    const index = opt_index || 0;

    const defaultName = str('DEFAULT_NEW_FOLDER_NAME');
    const newName =
        index === 0 ? defaultName : defaultName + ' (' + index + ')';

    return new Promise(parentDirectory.getDirectory.bind(
                           parentDirectory, newName, {create: false}))
        .then(newEntry => {
          return this.generateNewDirectoryName_(parentDirectory, index + 1);
        })
        .catch(() => {
          return newName;
        });
  }

  /** @override */
  canExecute(event, fileManager) {
    if (event.target instanceof DirectoryItem ||
        event.target instanceof DirectoryTree) {
      const entry = CommandUtil.getCommandEntry(fileManager, event.target);
      if (!entry || util.isFakeEntry(entry) ||
          util.isTeamDrivesGrandRoot(entry)) {
        event.canExecute = false;
        event.command.setHidden(true);
        return;
      }

      const locationInfo = fileManager.volumeManager.getLocationInfo(entry);
      event.canExecute = locationInfo && !locationInfo.isReadOnly &&
          CommandUtil.hasCapability(fileManager, [entry], 'canAddChildren');
      event.command.setHidden(false);
    } else {
      const directoryModel = fileManager.directoryModel;
      const directoryEntry = fileManager.getCurrentDirectoryEntry();
      event.canExecute = !fileManager.directoryModel.isReadOnly() &&
          !fileManager.namingController.isRenamingInProgress() &&
          !directoryModel.isSearching() &&
          CommandUtil.hasCapability(
              fileManager, [directoryEntry], 'canAddChildren');
      event.command.setHidden(false);
    }
    if (this.busy_) {
      event.canExecute = false;
    }
  }
})();

/**
 * Initiates new window creation.
 */
CommandHandler.COMMANDS_['new-window'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    fileManager.launchFileManager({
      currentDirectoryURL: fileManager.getCurrentDirectoryEntry() &&
          fileManager.getCurrentDirectoryEntry().toURL(),
    });
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute = fileManager.getCurrentDirectoryEntry() &&
        (fileManager.dialogType === DialogType.FULL_PAGE);
  }
})();

CommandHandler.COMMANDS_['select-all'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    fileManager.directoryModel.getFileListSelection().setCheckSelectMode(true);
    fileManager.directoryModel.getFileListSelection().selectAll();
  }

  /** @override */
  canExecute(event, fileManager) {
    // Check we can select multiple items.
    const multipleSelect =
        fileManager.directoryModel.getFileListSelection().multiple;
    // Check we are not inside an input element (e.g. the search box).
    const inputElementActive =
        document.activeElement instanceof HTMLInputElement ||
        document.activeElement instanceof HTMLTextAreaElement ||
        document.activeElement.tagName.toLowerCase() === 'cr-input';
    event.canExecute = multipleSelect && !inputElementActive &&
        fileManager.directoryModel.getFileList().length > 0;
  }
})();

CommandHandler.COMMANDS_['toggle-hidden-files'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        const visible = !fileManager.fileFilter.isHiddenFilesVisible();
        fileManager.fileFilter.setHiddenFilesVisible(visible);
        event.command.checked = visible;  // Checkmark for "Show hidden files".
        CommandHandler.recordMenuItemSelected(
            visible ? CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_SHOW :
                      CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_HIDE);
      }
    })();

/**
 * Toggles visibility of top-level Android folders which are not visible by
 * default.
 */
CommandHandler.COMMANDS_['toggle-hidden-android-folders'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        const visible = !fileManager.fileFilter.isAllAndroidFoldersVisible();
        fileManager.fileFilter.setAllAndroidFoldersVisible(visible);
        event.command.checked = visible;
        CommandHandler.recordMenuItemSelected(
            visible ?
                CommandHandler.MenuCommandsForUMA.HIDDEN_ANDROID_FOLDERS_SHOW :
                CommandHandler.MenuCommandsForUMA.HIDDEN_ANDROID_FOLDERS_HIDE);
      }

      /** @override */
      canExecute(event, fileManager) {
        const hasAndroidFilesVolumeInfo =
            !!fileManager.volumeManager.getCurrentProfileVolumeInfo(
                VolumeManagerCommon.VolumeType.ANDROID_FILES);
        const currentRootType = fileManager.directoryModel.getCurrentRootType();
        const isInMyFiles =
            currentRootType == VolumeManagerCommon.RootType.MY_FILES ||
            currentRootType == VolumeManagerCommon.RootType.DOWNLOADS ||
            currentRootType == VolumeManagerCommon.RootType.CROSTINI ||
            currentRootType == VolumeManagerCommon.RootType.ANDROID_FILES;
        event.canExecute = hasAndroidFilesVolumeInfo && isInMyFiles;
        event.command.setHidden(!event.canExecute);
        event.command.checked =
            fileManager.fileFilter.isAllAndroidFoldersVisible();
      }
    })();

/**
 * Toggles drive sync settings.
 */
CommandHandler.COMMANDS_['drive-sync-settings'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        // If checked, the sync is disabled.
        const nowCellularDisabled =
            fileManager.ui.gearMenu.syncButton.hasAttribute('checked');
        const changeInfo = {cellularDisabled: !nowCellularDisabled};
        chrome.fileManagerPrivate.setPreferences(changeInfo);
        CommandHandler.recordMenuItemSelected(
            nowCellularDisabled ?
                CommandHandler.MenuCommandsForUMA.MOBILE_DATA_OFF :
                CommandHandler.MenuCommandsForUMA.MOBILE_DATA_ON);
      }

      /** @override */
      canExecute(event, fileManager) {
        event.canExecute = fileManager.directoryModel.isOnDrive() &&
            fileManager.volumeManager.getDriveConnectionState()
                .hasCellularNetworkAccess;
        event.command.setHidden(!event.canExecute);
      }
    })();

/**
 * Delete / Move to Trash command.
 * @private @const {FilesCommand}
 */
CommandHandler.deleteCommand_ = new (class extends FilesCommand {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const permanentlyDelete = event.command.id === 'delete';

    // Execute might be called without a call of canExecute method, e.g.,
    // called directly from code, crbug.com/509483. See toolbar controller
    // delete button handling, for an example.
    this.deleteEntries(entries, fileManager, permanentlyDelete);
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);

    // If entries contain fake or root entry, remove delete option.
    if (!entries.every(CommandUtil.shouldShowMenuItemsForEntry.bind(
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
   * @param {!Array<!Entry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @param {boolean} permanentlyDelete if true, entries are permanently deleted
   *     rather than moved to trash.
   * @param {?FilesConfirmDialog} dialog An optional delete confirm dialog.
   *    The default delete confirm dialog will be used if |dialog| is null.
   * @public
   */
  deleteEntries(entries, fileManager, permanentlyDelete, dialog = null) {
    // Verify that the entries are not fake or root entries, and that they
    // can be deleted.
    if (!entries.every(CommandUtil.shouldShowMenuItemsForEntry.bind(
            null, fileManager.volumeManager)) ||
        !this.canDeleteEntries_(entries, fileManager)) {
      return;
    }

    // Trashing an item shows an "Undo" visual signal instead of a confirmation
    // dialog.
    if (!permanentlyDelete &&
        shouldMoveToTrash(entries, fileManager.volumeManager) &&
        fileManager.trashEnabled) {
      fileManager.ui.nudgeContainer.showNudge(NudgeType['TRASH_NUDGE']);

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
      dialog.doneCallback && dialog.doneCallback();
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
          strf('CONFIRM_PERMANENTLY_DELETE_ONE_TITLE') :
          strf('CONFIRM_PERMANENTLY_DELETE_SOME_TITLE');

      const message = entries.length === 1 ?
          strf('CONFIRM_PERMANENTLY_DELETE_ONE_DESC', entries[0].name) :
          strf('CONFIRM_PERMANENTLY_DELETE_SOME_DESC', entries.length);

      dialog.setOkLabel(str('PERMANENTLY_DELETE_FOREVER'));
      dialog.showWithTitle(title, message, deleteAction, cancelAction, null);
      return;
    }

    const deleteMessage = entries.length === 1 ?
        strf('CONFIRM_DELETE_ONE', entries[0].name) :
        strf('CONFIRM_DELETE_SOME', entries.length);
    dialog.setOkLabel(str('DELETE_BUTTON_LABEL'));
    dialog.show(deleteMessage, deleteAction, cancelAction, null);
  }

  /**
   * Returns true if all entries can be deleted. Note: This does not check for
   * root or fake entries.
   * @param {!Array<!Entry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @return {boolean}
   * @private
   */
  canDeleteEntries_(entries, fileManager) {
    return entries.length > 0 &&
        !this.containsReadOnlyEntry_(entries, fileManager) &&
        fileManager.directoryModel.canDeleteEntries() &&
        CommandUtil.hasCapability(fileManager, entries, 'canDelete');
  }

  /**
   * Returns True if entries can be deleted.
   * @param {!Array<!Entry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @return {boolean}
   * @public
   */
  canDeleteEntries(entries, fileManager) {
    // Verify that the entries are not fake or root entries, and that they
    // can be deleted.
    if (!entries.every(CommandUtil.shouldShowMenuItemsForEntry.bind(
            null, fileManager.volumeManager)) ||
        !this.canDeleteEntries_(entries, fileManager)) {
      return false;
    }

    return true;
  }

  /**
   * Returns true if any entry belongs to a read-only volume or is
   * forced to be read-only like MyFiles>Downloads.
   * @param {!Array<!Entry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @return {boolean}
   * @private
   */
  containsReadOnlyEntry_(entries, fileManager) {
    return entries.some(entry => {
      const locationInfo = fileManager.volumeManager.getLocationInfo(entry);
      return (locationInfo && locationInfo.isReadOnly) ||
          util.isNonModifiable(fileManager.volumeManager, entry);
    });
  }
})();

CommandHandler.COMMANDS_['delete'] = CommandHandler.deleteCommand_;
CommandHandler.COMMANDS_['move-to-trash'] = CommandHandler.deleteCommand_;

/**
 * Restores selected files from trash.
 *
 * @suppress {invalidCasts} See FilesAppEntry in files_app_entry_interfaces.js
 * for explanation of why FilesAppEntry cannot extend Entry.
 */
CommandHandler
    .COMMANDS_['restore-from-trash'] = new (class extends FilesCommand {
  /** @private */
  async execute_(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);

    const infoEntries = [];
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
        metrics.recordEnum(
            RestoreFailedUMA, RestoreFailedType.SINGLE_ITEM,
            RestoreFailedTypesUMA);
        fileManager.ui.alertDialog.show(
            strf('CANT_RESTORE_SINGLE_ITEM', failedParents[0].parentName));
        return;
      }
      // More than one item has been trashed but all the items have their
      // parent removed.
      if (failedParents.length > 1 && infoEntries.length === 0) {
        const isParentFolderSame = failedParents.every(
            p => p.parentName === failedParents[0].parentName);
        // All the items were from the same parent folder.
        if (isParentFolderSame) {
          metrics.recordEnum(
              RestoreFailedUMA, RestoreFailedType.MULTIPLE_ITEMS_SAME_PARENTS,
              RestoreFailedTypesUMA);
          fileManager.ui.alertDialog.show(strf(
              'CANT_RESTORE_MULTIPLE_ITEMS_SAME_PARENTS',
              failedParents[0].parentName));
          return;
        }
        // All the items are from different parent folders.
        metrics.recordEnum(
            RestoreFailedUMA,
            RestoreFailedType.MULTIPLE_ITEMS_DIFFERENT_PARENTS,
            RestoreFailedTypesUMA);
        fileManager.ui.alertDialog.show(
            str('CANT_RESTORE_MULTIPLE_ITEMS_DIFFERENT_PARENTS'));
        return;
      }
      // A mix of items with parents and without parents are attempting to be
      // restored.
      metrics.recordEnum(
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
  execute(event, fileManager) {
    this.execute_(event, fileManager);
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);

    const enabled = entries.length > 0 &&
        entries.every(e => util.isTrashEntry(e)) && fileManager.trashEnabled;
    event.canExecute = enabled;
    event.command.setHidden(!enabled);
  }

  /**
   * Check whether the parent exists from a supplied entry and return the folder
   * name (if it exists or doesn't).
   * @param {!Entry} entry The entry to identify the parent from.
   * @param {!VolumeManager} volumeManager
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
                parentName: components[components.length - 2].name,
              });
              return;
            }
            reject(err);
          });
    });
  }
})();

/**
 * Empties (permanently deletes all) files from trash.
 */
CommandHandler.COMMANDS_['empty-trash'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const numEntries = fileManager.directoryModel.getFileList().length;
    if (numEntries === 0) {
      return;
    }

    fileManager.ui.emptyTrashConfirmDialog.showWithTitle(
        str('CONFIRM_EMPTY_TRASH_TITLE'), str('CONFIRM_EMPTY_TRASH_DESC'),
        () => {
          startIOTask(
              chrome.fileManagerPrivate.IOTaskType.EMPTY_TRASH, /*entries=*/[],
              /*params=*/ {});
        });
  }

  /** @override */
  canExecute(event, fileManager) {
    // Always allow execute regardless of which files are selected to allow the
    // trash toolbar action to run even if no files are selected.
    event.canExecute = true;

    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const visible = entries.length === 1 && util.isTrashRoot(entries[0]) &&
        fileManager.trashEnabled;
    event.command.setHidden(!visible);
  }
})();

/**
 * Pastes files from clipboard.
 */
CommandHandler.COMMANDS_['paste'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    fileManager.document.execCommand(event.command.id);
  }

  /** @override */
  canExecute(event, fileManager) {
    const fileTransferController = fileManager.fileTransferController;

    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(
            fileManager.directoryModel.getCurrentDirEntry());

    // Hide this command if only one folder is selected.
    event.command.setHidden(
        !!CommandUtil.getOnlyOneSelectedDirectory(fileManager.getSelection()));
  }
})();

/**
 * Pastes files from clipboard. This is basically same as 'paste'.
 * This command is used for always showing the Paste command to gear menu.
 */
CommandHandler.COMMANDS_['paste-into-current-folder'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        fileManager.document.execCommand('paste');
      }

      /** @override */
      canExecute(event, fileManager) {
        const fileTransferController = fileManager.fileTransferController;

        event.canExecute = !!fileTransferController &&
            fileTransferController.queryPasteCommandEnabled(
                fileManager.directoryModel.getCurrentDirEntry());
      }
    })();

/**
 * Pastes files from clipboard into the selected folder.
 */
CommandHandler.COMMANDS_['paste-into-folder'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        const entries =
            CommandUtil.getCommandEntries(fileManager, event.target);
        if (entries.length !== 1 || !entries[0].isDirectory ||
            !CommandUtil.shouldShowMenuItemsForEntry(
                fileManager.volumeManager, entries[0])) {
          return;
        }

        // This handler tweaks the Event object for 'paste' event so that
        // the FileTransferController can distinguish this 'paste-into-folder'
        // command and know the destination directory.
        const handler = inEvent => {
          inEvent.destDirectory = entries[0];
        };
        fileManager.document.addEventListener('paste', handler, true);
        fileManager.document.execCommand('paste');
        fileManager.document.removeEventListener('paste', handler, true);
      }

      /** @override */
      canExecute(event, fileManager) {
        const entries =
            CommandUtil.getCommandEntries(fileManager, event.target);

        // Show this item only when one directory is selected.
        if (entries.length !== 1 || !entries[0].isDirectory ||
            !CommandUtil.shouldShowMenuItemsForEntry(
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
    })();

/**
 * Cut/Copy command.
 * @private @const {FilesCommand}
 */
CommandHandler.cutCopyCommand_ = new (class extends FilesCommand {
  execute(event, fileManager) {
    // Cancel check-select-mode on cut/copy.  Any further selection of a dir
    // should start a new selection rather than add to the existing selection.
    fileManager.directoryModel.getFileListSelection().setCheckSelectMode(false);
    fileManager.document.execCommand(event.command.id);
  }

  /** @override */
  canExecute(event, fileManager) {
    const fileTransferController = fileManager.fileTransferController;

    if (!fileTransferController) {
      // File Open and SaveAs dialogs do not have a fileTransferController.
      event.command.setHidden(true);
      event.canExecute = false;
      return;
    }

    const command = event.command;
    const target = event.target;
    const isMove = command.id === 'cut';
    const volumeManager = fileManager.volumeManager;
    command.setHidden(false);

    /** @returns {boolean} If the operation is allowed in the Directory Tree. */
    function canDoDirectoryTree() {
      let entry;
      if (target.entry) {
        entry = target.entry;
      } else if (target.selectedItem && target.selectedItem.entry) {
        entry = target.selectedItem.entry;
      } else {
        return false;
      }

      if (!CommandUtil.shouldShowMenuItemsForEntry(volumeManager, entry)) {
        command.setHidden(true);
        return false;
      }

      // For MyFiles/Downloads and MyFiles/PluginVm we only allow copy.
      if (isMove && util.isNonModifiable(volumeManager, entry)) {
        return false;
      }

      // Cut is unavailable on Shared Drive roots.
      if (util.isTeamDriveRoot(entry)) {
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
      if (CommandUtil.shouldIgnoreEvents(assert(fileManager.document))) {
        return false;
      }

      if (!fileManager.getSelection().entries.every(
              CommandUtil.shouldShowMenuItemsForEntry.bind(
                  null, volumeManager))) {
        command.setHidden(true);
        return false;
      }

      // For MyFiles/Downloads we only allow copy.
      if (isMove &&
          fileManager.getSelection().entries.some(
              util.isNonModifiable.bind(null, volumeManager))) {
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
})();

CommandHandler.COMMANDS_['cut'] = CommandHandler.cutCopyCommand_;
CommandHandler.COMMANDS_['copy'] = CommandHandler.cutCopyCommand_;

/**
 * Initiates file renaming.
 */
CommandHandler.COMMANDS_['rename'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
    if (util.isNonModifiable(fileManager.volumeManager, entry)) {
      return;
    }
    const currentRootType = fileManager.directoryModel.getCurrentRootType();
    if (currentRootType === VolumeManagerCommon.RootType.TRASH) {
      return;
    }
    let isRemovableRoot = false;
    let volumeInfo = null;
    if (entry) {
      volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
      // Checks whether the target is an external drive.
      if (volumeInfo &&
          CommandUtil.isRootEntry(fileManager.volumeManager, entry)) {
        isRemovableRoot = true;
      }
    }
    if (event.target instanceof DirectoryTree ||
        event.target instanceof DirectoryItem) {
      if (event.target instanceof DirectoryTree) {
        const directoryTree = event.target;
        assert(fileManager.directoryTreeNamingController)
            .attachAndStart(
                assert(directoryTree.selectedItem), isRemovableRoot,
                volumeInfo);
      } else if (event.target instanceof DirectoryItem) {
        const directoryItem = event.target;
        assert(fileManager.directoryTreeNamingController)
            .attachAndStart(directoryItem, isRemovableRoot, volumeInfo);
      }
    } else {
      fileManager.namingController.initiateRename(isRemovableRoot, volumeInfo);
    }
  }

  /** @override */
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

    // Items in Trash are a fake representation of a file + it's metadata. These
    // items can't be renamed whilst in Trash and should be restored to enable
    // renaming.
    const currentRootType = fileManager.directoryModel.getCurrentRootType();
    if (currentRootType === VolumeManagerCommon.RootType.TRASH) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    // Check if it is removable drive
    if ((() => {
          const root = CommandUtil.getCommandEntry(fileManager, event.target);
          // |root| is null for unrecognized volumes. Do not enable rename
          // command for such volumes because they need to be formatted prior to
          // rename.
          if (!root ||
              !CommandUtil.isRootEntry(fileManager.volumeManager, root)) {
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
          const removable =
              location.rootType === VolumeManagerCommon.RootType.REMOVABLE;
          event.canExecute = removable && writable &&
              volumeInfo.diskFileSystemType &&
              CommandHandler.RENAME_DISK_FILE_SYSTEM_SUPPORT_.indexOf(
                  volumeInfo.diskFileSystemType) > -1;
          event.command.setHidden(!removable);
          return removable;
        })()) {
      return;
    }

    // Check if it is file or folder
    const renameTarget = CommandUtil.isFromSelectionMenu(event) ?
        fileManager.ui.listContainer.currentList :
        event.target;
    const entries = CommandUtil.getCommandEntries(fileManager, renameTarget);
    if (entries.length === 0 ||
        !CommandUtil.shouldShowMenuItemsForEntry(
            fileManager.volumeManager, entries[0]) ||
        entries.some(
            util.isNonModifiable.bind(null, fileManager.volumeManager))) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const parentEntry =
        CommandUtil.getParentEntry(renameTarget, fileManager.directoryModel);
    const locationInfo = parentEntry ?
        fileManager.volumeManager.getLocationInfo(parentEntry) :
        null;
    const volumeIsNotReadOnly = !!locationInfo && !locationInfo.isReadOnly;
    // ARC doesn't support rename for now. http://b/232152680
    const isRecentArcEntry = VolumeManagerCommon.isRecentArcEntry(entries[0]);
    event.canExecute = entries.length === 1 && volumeIsNotReadOnly &&
        !isRecentArcEntry &&
        CommandUtil.hasCapability(fileManager, entries, 'canRename');
    event.command.setHidden(false);
  }
})();

/**
 * Opens drive help.
 */
CommandHandler.COMMANDS_['volume-help'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    if (fileManager.directoryModel.isOnDrive()) {
      util.visitURL(str('GOOGLE_DRIVE_HELP_URL'));
      CommandHandler.recordMenuItemSelected(
          CommandHandler.MenuCommandsForUMA.DRIVE_HELP);
    } else {
      util.visitURL(str('FILES_APP_HELP_URL'));
      CommandHandler.recordMenuItemSelected(
          CommandHandler.MenuCommandsForUMA.HELP);
    }
  }

  /** @override */
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
})();

/**
 * Opens the send feedback window.
 */
CommandHandler.COMMANDS_['send-feedback'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.sendFeedback();
  }
})();

/**
 * Opens drive buy-more-space url.
 */
CommandHandler.COMMANDS_['drive-buy-more-space'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        util.visitURL(str('GOOGLE_DRIVE_BUY_STORAGE_URL'));
        CommandHandler.recordMenuItemSelected(
            CommandHandler.MenuCommandsForUMA.DRIVE_BUY_MORE_SPACE);
      }

      /** @override */
      canExecute(event, fileManager) {
        CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly(
            event, fileManager);
      }
    })();

/**
 * Opens drive.google.com.
 */
CommandHandler.COMMANDS_['drive-go-to-drive'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        util.visitURL(str('GOOGLE_DRIVE_ROOT_URL'));
        CommandHandler.recordMenuItemSelected(
            CommandHandler.MenuCommandsForUMA.DRIVE_GO_TO_DRIVE);
      }

      /** @override */
      canExecute(event, fileManager) {
        CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly(
            event, fileManager);
      }
    })();

/**
 * Opens a file with default task.
 */
CommandHandler.COMMANDS_['default-task'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    fileManager.taskController.executeDefaultTask();
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute = fileManager.taskController.canExecuteDefaultTask();
    event.command.setHidden(fileManager.taskController.shouldHideDefaultTask());
  }
})();

/**
 * Displays "open with" dialog for current selection.
 */
CommandHandler.COMMANDS_['open-with'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    console.assert(
        `open-with command doesn't execute, ` +
        `instead it only opens the sub-menu`);
  }

  /** @override */
  canExecute(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteOpenActions();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
})();

/**
 * Invoke Sharesheet.
 */
CommandHandler.COMMANDS_['invoke-sharesheet'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        const entries = fileManager.selectionHandler.selection.entries;
        const launchSource = CommandUtil.getSharesheetLaunchSource(event);
        const dlpSourceUrls =
            fileManager.metadataModel.getCache(entries, ['sourceUrl'])
                .map(m => m.sourceUrl || '');
        chrome.fileManagerPrivate.invokeSharesheet(
            entries.map(e => util.unwrapEntry(e)), launchSource, dlpSourceUrls,
            () => {
              if (chrome.runtime.lastError) {
                console.warn(chrome.runtime.lastError.message);
                return;
              }
            });
      }

      /** @override */
      canExecute(event, fileManager) {
        const entries = fileManager.selectionHandler.selection.entries;

        if (!entries || entries.length === 0 ||
            (entries.some(entry => entry.isDirectory) &&
             (!CommandUtil.isDriveEntries(entries, fileManager.volumeManager) ||
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
            entries.map(e => util.unwrapEntry(e)), hasTargets => {
              if (chrome.runtime.lastError) {
                console.warn(chrome.runtime.lastError.message);
                return;
              }
              event.command.setHidden(!hasTargets);
              event.canExecute = hasTargets;
              event.command.disabled = !hasTargets;
            });
      }
    })();

CommandHandler.COMMANDS_['toggle-holding-space'] =
    new (class extends FilesCommand {
      constructor() {
        super();
        /**
         * Whether the command adds or removed items from holding space. The
         * value is set in <code>canExecute()</code>. It will be true unless all
         * selected items are already in the holding space.
         * @private {boolean|undefined}
         */
        this.addsItems_;
      }

      /** @override */
      execute(event, fileManager) {
        if (this.addsItems_ === undefined) {
          return;
        }

        // Filter out entries from unsupported volumes.
        const allowedVolumeTypes = HoldingSpaceUtil.getAllowedVolumeTypes();
        const entries =
            fileManager.selectionHandler.selection.entries.filter(entry => {
              const volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
              return volumeInfo &&
                  allowedVolumeTypes.includes(volumeInfo.volumeType);
            });

        chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
            entries, this.addsItems_);

        if (this.addsItems_) {
          HoldingSpaceUtil.maybeStoreTimeOfFirstPin();
        }

        CommandHandler.recordMenuItemSelected(
            this.addsItems_ ?
                CommandHandler.MenuCommandsForUMA.PIN_TO_HOLDING_SPACE :
                CommandHandler.MenuCommandsForUMA.UNPIN_FROM_HOLDING_SPACE);
      }

      /** @override */
      canExecute(event, fileManager) {
        const command = event.command;

        const allowedVolumeTypes = HoldingSpaceUtil.getAllowedVolumeTypes();
        const currentRootType = fileManager.directoryModel.getCurrentRootType();
        if (!util.isRecentRootType(currentRootType)) {
          const volumeInfo = fileManager.directoryModel.getCurrentVolumeInfo();
          if (!volumeInfo ||
              !allowedVolumeTypes.includes(volumeInfo.volumeType)) {
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
          command.setHidden(true);
          return;
        }

        const itemsSet = {};
        state.itemUrls.forEach((item) => itemsSet[item] = true);

        const selectedUrls = util.entriesToURLs(entries);
        this.addsItems_ = selectedUrls.some(url => !itemsSet[url]);

        command.label = this.addsItems_ ?
            str('HOLDING_SPACE_PIN_COMMAND_LABEL') :
            str('HOLDING_SPACE_UNPIN_COMMAND_LABEL');
      }
    })();

/**
 * Opens containing folder of the focused file.
 */
CommandHandler.COMMANDS_['go-to-file-location'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        const entries =
            CommandUtil.getCommandEntries(fileManager, event.target);
        if (entries.length !== 1) {
          return;
        }

        const components = PathComponent.computeComponentsFromEntry(
            entries[0], fileManager.volumeManager);
        // Entries in file list table should always have its containing folder.
        // (i.e. Its path have at least two components: its parent and itself.)
        assert(components.length >= 2);
        const parentComponent = components[components.length - 2];
        parentComponent.resolveEntry().then(entry => {
          if (entry && entry.isDirectory) {
            fileManager.directoryModel.changeDirectoryEntry(
                /** @type {!(DirectoryEntry|FilesAppDirEntry)} */ (entry));
          }
        });
      }

      /** @override */
      canExecute(event, fileManager) {
        // Available in Recents, Audio, Images, and Videos.
        if (!util.isRecentRootType(
                fileManager.directoryModel.getCurrentRootType())) {
          event.canExecute = false;
          event.command.setHidden(true);
          return;
        }

        // Available for a single entry.
        const entries =
            CommandUtil.getCommandEntries(fileManager, event.target);
        event.canExecute = entries.length === 1;
        event.command.setHidden(!event.canExecute);
      }
    })();

/**
 * Displays QuickView for current selection.
 */
CommandHandler.COMMANDS_['get-info'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    // 'get-info' command is executed by 'command' event handler in
    // QuickViewController.
  }

  /** @override */
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
})();

/**
 * Displays the Data Leak Prevention (DLP) Restriction details.
 */
CommandHandler.COMMANDS_['dlp-restriction-details'] =
    new (class extends FilesCommand {
      async executeImpl_(event, fileManager) {
        const entries = fileManager.getSelection().entries;

        const metadata =
            fileManager.metadataModel.getCache(entries, ['sourceUrl']);
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

      execute(event, fileManager) {
        this.executeImpl_(event, fileManager);
      }

      /** @override */
      canExecute(event, fileManager) {
        if (!util.isDlpEnabled()) {
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
    })();

/**
 * Focuses search input box.
 */
CommandHandler.COMMANDS_['search'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    // Cancel item selection.
    fileManager.directoryModel.clearSelection();
    // Open the query input via the search container.
    fileManager.ui.searchContainer.openSearch();
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute = !fileManager.namingController.isRenamingInProgress();
  }
})();

/**
 * Activates the n-th volume.
 */
CommandHandler.COMMANDS_['volume-switch-1'] =
    CommandUtil.createVolumeSwitchCommand(1);
CommandHandler.COMMANDS_['volume-switch-2'] =
    CommandUtil.createVolumeSwitchCommand(2);
CommandHandler.COMMANDS_['volume-switch-3'] =
    CommandUtil.createVolumeSwitchCommand(3);
CommandHandler.COMMANDS_['volume-switch-4'] =
    CommandUtil.createVolumeSwitchCommand(4);
CommandHandler.COMMANDS_['volume-switch-5'] =
    CommandUtil.createVolumeSwitchCommand(5);
CommandHandler.COMMANDS_['volume-switch-6'] =
    CommandUtil.createVolumeSwitchCommand(6);
CommandHandler.COMMANDS_['volume-switch-7'] =
    CommandUtil.createVolumeSwitchCommand(7);
CommandHandler.COMMANDS_['volume-switch-8'] =
    CommandUtil.createVolumeSwitchCommand(8);
CommandHandler.COMMANDS_['volume-switch-9'] =
    CommandUtil.createVolumeSwitchCommand(9);

/**
 * Flips 'available offline' flag on the file.
 */
CommandHandler.COMMANDS_['toggle-pinned'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const entries = fileManager.getSelection().entries;
    const actionsController = fileManager.actionsController;

    actionsController.getActionsForEntries(entries).then(
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const saveForOfflineAction = actionsModel.getAction(
              ActionsModel.CommonActionId.SAVE_FOR_OFFLINE);
          const offlineNotNeededAction = actionsModel.getAction(
              ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY);
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
  canExecute(event, fileManager) {
    const entries = fileManager.getSelection().entries;
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!CommandUtil.isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    function canExecutePinned_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const saveForOfflineAction =
          actionsModel.getAction(ActionsModel.CommonActionId.SAVE_FOR_OFFLINE);
      const offlineNotNeededAction = actionsModel.getAction(
          ActionsModel.CommonActionId.OFFLINE_NOT_NECESSARY);
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
})();

/**
 * Extracts content of ZIP files in the current selection.
 */
CommandHandler.COMMANDS_['extract-all'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    let dirEntry = fileManager.getCurrentDirectoryEntry();
    if (!dirEntry ||
        !fileManager.getSelection().entries.every(
            CommandUtil.shouldShowMenuItemsForEntry.bind(
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
  canExecute(event, fileManager) {
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    const selection = fileManager.getSelection();

    if (!dirEntry || !selection || selection.totalCount === 0) {
      event.command.setHidden(true);
      event.canExecute = false;
    } else {
      // Check the selected entries for a ZIP archive in the selected set.
      for (const entry of selection.entries) {
        if (FileType.getExtension(entry) === '.zip') {
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
})();

/**
 * Creates ZIP file for current selection.
 */
CommandHandler.COMMANDS_['zip-selection'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    if (!dirEntry ||
        !fileManager.getSelection().entries.every(
            CommandUtil.shouldShowMenuItemsForEntry.bind(
                null, fileManager.volumeManager))) {
      return;
    }

    const selectionEntries = fileManager.getSelection().entries;
    startIOTask(
        chrome.fileManagerPrivate.IOTaskType.ZIP, selectionEntries,
        {destinationFolder: /** @type {!DirectoryEntry} */ (dirEntry)});
  }

  /** @override */
  canExecute(event, fileManager) {
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    const selection = fileManager.getSelection();

    // Hide ZIP selection for single ZIP file selected.
    if (selection.entries.length === 1 &&
        FileType.getExtension(selection.entries[0]) === '.zip') {
      event.command.setHidden(true);
      event.canExecute = false;
      return;
    }

    if (!selection.entries.every(CommandUtil.shouldShowMenuItemsForEntry.bind(
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

    event.canExecute = dirEntry && !fileManager.directoryModel.isReadOnly() &&
        isOnEligibleLocation && selection && selection.totalCount > 0;
  }
})();

/**
 * Shows the share dialog for the current selection (single only).
 */
CommandHandler.COMMANDS_['share'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const action =
              actionsModel.getAction(ActionsModel.CommonActionId.SHARE);
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!CommandUtil.isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    function canExecuteShare_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const action = actionsModel.getAction(ActionsModel.CommonActionId.SHARE);
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
})();

/**
 * Opens the file in Drive for the user to manage sharing permissions etc.
 */
CommandHandler.COMMANDS_['manage-in-drive'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const action = actionsModel.getAction(
              ActionsModel.InternalActionId.MANAGE_IN_DRIVE);
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!CommandUtil.isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    function canExecuteManageInDrive_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const action =
          actionsModel.getAction(ActionsModel.InternalActionId.MANAGE_IN_DRIVE);
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
})();

/**
 * Opens the Manage MirrorSync dialog if the flag is enabled.
 */
CommandHandler.COMMANDS_['manage-mirrorsync'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        chrome.fileManagerPrivate.openManageSyncSettings();
      }

      /**
       * @override
       */
      canExecute(event, fileManager) {
        // MirrorSync is only available to sync local directories, only show the
        // folder when navigated to a local directory.
        const currentRootType = fileManager.directoryModel.getCurrentRootType();
        event.canExecute =
            (currentRootType === VolumeManagerCommon.RootType.MY_FILES ||
             currentRootType === VolumeManagerCommon.RootType.DOWNLOADS) &&
            util.isMirrorSyncEnabled();
        event.command.setHidden(!event.canExecute);
      }
    })();

/**
 * A command to share the target folder with the specified Guest OS.
 */
class GuestOsShareCommand extends FilesCommand {
  /**
   * @param {string} vmName Name of the vm to share into.
   * @param {string} typeForStrings VM type to identify the strings used for
   *     this VM e.g. LINUX or PLUGIN_VM.
   * @param {string} settingsPath Path to the page in settings to manage
   *     sharing.
   * @param {!CommandHandler.MenuCommandsForUMA} manageUma MenuCommandsForUMA
   *     entry this command should emit metrics under when the toast to manage
   *     sharing is clicked on.
   * @param {!CommandHandler.MenuCommandsForUMA} shareUma MenuCommandsForUMA
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

  execute(event, fileManager) {
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
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
          this.vmName_, [util.unwrapEntry(entry)], true /* persist */, () => {
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
              CommandHandler.recordMenuItemSelected(this.manageUma_);
            },
          });
    };
    // Show a confirmation dialog if we are sharing the root of a volume.
    // Non-Drive volume roots are always '/'.
    if (entry.fullPath == '/') {
      fileManager.ui.confirmDialog.showHtml(
          strf(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_TITLE`),
          strf(
              `SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}`,
              info.volumeInfo.label),
          share, () => {});
    } else if (
        info.isRootEntry &&
        (info.rootType == VolumeManagerCommon.RootType.DRIVE ||
         info.rootType == VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
         info.rootType ==
             VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT)) {
      // Only show the dialog for My Drive, Shared Drives Grand Root and
      // Computers Grand Root.  Do not show for roots of a single Shared
      // Drive or Computer.
      fileManager.ui.confirmDialog.showHtml(
          strf(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_TITLE`),
          strf(`SHARE_ROOT_FOLDER_WITH_${this.typeForStrings_}_DRIVE`), share,
          () => {});
    } else {
      // This is not a root, share it without confirmation dialog.
      share();
    }
    CommandHandler.recordMenuItemSelected(this.shareUma_);
  }

  /** @override */
  canExecute(event, fileManager) {
    // Must be single directory not already shared.
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
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
class GuestOsManagingSharingGearCommand extends FilesCommand {
  /**
   * @param {string} vmName Name of the vm to share into.
   * @param {string} settingsPath Path to the page in settings to manage
   *     sharing.
   * @param {!CommandHandler.MenuCommandsForUMA} manageUma MenuCommandsForUMA
   *     entry this command should emit metrics under when the toast to manage
   *     sharing is clicked on.
   */
  constructor(vmName, settingsPath, manageUma) {
    super();
    this.vmName_ = vmName;
    this.settingsPath_ = settingsPath;
    this.manageUma_ = manageUma;
  }
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage(this.settingsPath_);
    CommandHandler.recordMenuItemSelected(this.manageUma_);
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute = fileManager.crostini.isEnabled(this.vmName_);
    event.command.setHidden(!event.canExecute);
  }
}

/**
 * Creates a command for managing sharing.
 */
class GuestOsManagingSharingCommand extends FilesCommand {
  /**
   * @param {string} vmName Name of the vm to share into.
   * @param {string} settingsPath Path to the page in settings to manage
   *     sharing.
   * @param {!CommandHandler.MenuCommandsForUMA} manageUma MenuCommandsForUMA
   *     entry this command should emit metrics under when the toast to manage
   *     sharing is clicked on.
   */
  constructor(vmName, settingsPath, manageUma) {
    super();
    this.vmName_ = vmName;
    this.settingsPath_ = settingsPath;
    this.manageUma_ = manageUma;
  }
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage(this.settingsPath_);
    CommandHandler.recordMenuItemSelected(this.manageUma_);
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        fileManager.crostini.isPathShared(this.vmName_, entries[0]);
    event.command.setHidden(!event.canExecute);
  }
}

const crostiniSettings = 'crostini/sharedPaths';
const pluginVmSettings = 'app-management/pluginVm/sharedPaths';
const bruschettaSettings = 'bruschetta/sharedPaths';

CommandHandler.COMMANDS_['share-with-linux'] = new GuestOsShareCommand(
    constants.DEFAULT_CROSTINI_VM, 'CROSTINI', crostiniSettings,
    CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST,
    CommandHandler.MenuCommandsForUMA.SHARE_WITH_LINUX);
CommandHandler.COMMANDS_['share-with-plugin-vm'] = new GuestOsShareCommand(
    constants.PLUGIN_VM, 'PLUGIN_VM', pluginVmSettings,
    CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING_TOAST,
    CommandHandler.MenuCommandsForUMA.SHARE_WITH_PLUGIN_VM);
CommandHandler.COMMANDS_['share-with-bruschetta'] = new GuestOsShareCommand(
    constants.DEFAULT_BRUSCHETTA_VM, 'BRUSCHETTA', bruschettaSettings,
    CommandHandler.MenuCommandsForUMA.MANAGE_BRUSCHETTA_SHARING_TOAST,
    CommandHandler.MenuCommandsForUMA.SHARE_WITH_BRUSCHETTA);

CommandHandler.COMMANDS_['manage-linux-sharing-gear'] =
    new GuestOsManagingSharingGearCommand(
        constants.DEFAULT_CROSTINI_VM, crostiniSettings,
        CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING);
CommandHandler.COMMANDS_['manage-plugin-vm-sharing-gear'] =
    new GuestOsManagingSharingGearCommand(
        constants.PLUGIN_VM, pluginVmSettings,
        CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING);
CommandHandler.COMMANDS_['manage-bruschetta-sharing-gear'] =
    new GuestOsManagingSharingGearCommand(
        constants.DEFAULT_BRUSCHETTA_VM, bruschettaSettings,
        CommandHandler.MenuCommandsForUMA.MANAGE_BRUSCHETTA_SHARING);

CommandHandler.COMMANDS_['manage-linux-sharing'] =
    new GuestOsManagingSharingCommand(
        constants.DEFAULT_CROSTINI_VM, crostiniSettings,
        CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING);
CommandHandler.COMMANDS_['manage-plugin-vm-sharing'] =
    new GuestOsManagingSharingCommand(
        constants.PLUGIN_VM, pluginVmSettings,
        CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING);
CommandHandler.COMMANDS_['manage-bruschetta-sharing'] =
    new GuestOsManagingSharingCommand(
        constants.DEFAULT_BRUSCHETTA_VM, bruschettaSettings,
        CommandHandler.MenuCommandsForUMA.MANAGE_BRUSCHETTA_SHARING);

/**
 * Creates a shortcut of the selected folder (single only).
 */
CommandHandler.COMMANDS_['pin-folder'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const action = actionsModel.getAction(
              ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT);
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!CommandUtil.isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    function canExecuteCreateShortcut_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const action = actionsModel.getAction(
          ActionsModel.InternalActionId.CREATE_FOLDER_SHORTCUT);
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
})();

/**
 * Removes the folder shortcut.
 */
CommandHandler.COMMANDS_['unpin-folder'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const actionsController = fileManager.actionsController;

    fileManager.actionsController.getActionsForEntries(entries).then(
        (/** ?ActionsModel */ actionsModel) => {
          if (!actionsModel) {
            return;
          }
          const action = actionsModel.getAction(
              ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT);
          if (action) {
            actionsController.executeAction(action);
          }
        });
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    const command = event.command;
    const actionsController = fileManager.actionsController;

    // Avoid flickering menu height: synchronously define command visibility.
    if (!CommandUtil.isDriveEntries(entries, fileManager.volumeManager)) {
      command.setHidden(true);
      return;
    }

    command.setHidden(false);

    function canExecuteRemoveShortcut_(/** ?ActionsModel */ actionsModel) {
      if (!actionsModel) {
        return;
      }
      const action = actionsModel.getAction(
          ActionsModel.InternalActionId.REMOVE_FOLDER_SHORTCUT);
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
})();

/**
 * Zoom in to the Files app.
 */
CommandHandler.COMMANDS_['zoom-in'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.IN);
  }
})();

/**
 * Zoom out from the Files app.
 */
CommandHandler.COMMANDS_['zoom-out'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.OUT);
  }
})();

/**
 * Reset the zoom factor.
 */
CommandHandler.COMMANDS_['zoom-reset'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.RESET);
  }
})();

/**
 * Sort the file list by name (in ascending order).
 */
CommandHandler.COMMANDS_['sort-by-name'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('name', 'asc');
      const msg = strf('COLUMN_SORTED_ASC', str('NAME_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
})();

/**
 * Sort the file list by size (in descending order).
 */
CommandHandler.COMMANDS_['sort-by-size'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('size', 'desc');
      const msg = strf('COLUMN_SORTED_DESC', str('SIZE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
})();

/**
 * Sort the file list by type (in ascending order).
 */
CommandHandler.COMMANDS_['sort-by-type'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('type', 'asc');
      const msg = strf('COLUMN_SORTED_ASC', str('TYPE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
})();

/**
 * Sort the file list by date-modified (in descending order).
 */
CommandHandler.COMMANDS_['sort-by-date'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('modificationTime', 'desc');
      const msg = strf('COLUMN_SORTED_DESC', str('DATE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
})();

/**
 * Open inspector for foreground page.
 */
CommandHandler.COMMANDS_['inspect-normal'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.NORMAL);
  }
})();

/**
 * Open inspector for foreground page and bring focus to the console.
 */
CommandHandler.COMMANDS_['inspect-console'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.CONSOLE);
  }
})();

/**
 * Open inspector for foreground page in inspect element mode.
 */
CommandHandler.COMMANDS_['inspect-element'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.ELEMENT);
  }
})();

/**
 * Opens the gear menu.
 */
CommandHandler.COMMANDS_['open-gear-menu'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    fileManager.ui.gearButton.showMenu(true);
  }
})();

/**
 * Focus the first button visible on action bar (at the top).
 */
CommandHandler.COMMANDS_['focus-action-bar'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    fileManager.ui.actionbar
        .querySelector('button:not([hidden]), cr-button:not([hidden])')
        .focus();
  }
})();

/**
 * Handle back button.
 */
CommandHandler.COMMANDS_['browser-back'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    // TODO(fukino): It should be better to minimize Files app only when there
    // is no back stack, and otherwise use BrowserBack for history navigation.
    // https://crbug.com/624100.
    // TODO(https://crbug.com/1097066): Implement minimize for files SWA, then
    // call its minimize() function here.
  }
})();

/**
 * Configures the currently selected volume.
 */
CommandHandler.COMMANDS_['configure'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    const volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager);
    if (volumeInfo && volumeInfo.configurable) {
      fileManager.volumeManager.configure(volumeInfo);
    }
  }

  /** @override */
  canExecute(event, fileManager) {
    const volumeInfo =
        CommandUtil.getElementVolumeInfo(event.target, fileManager);
    event.canExecute = volumeInfo && volumeInfo.configurable;
    event.command.setHidden(!event.canExecute);
  }
})();

/**
 * Refreshes the currently selected directory.
 */
CommandHandler.COMMANDS_['refresh'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    fileManager.directoryModel.rescan(true /* refresh */);
    fileManager.spinnerController.blink();
  }

  /** @override */
  canExecute(event, fileManager) {
    const currentDirEntry = fileManager.directoryModel.getCurrentDirEntry();
    const volumeInfo = currentDirEntry &&
        fileManager.volumeManager.getVolumeInfo(currentDirEntry);
    event.canExecute = volumeInfo && !volumeInfo.watchable;
    event.command.setHidden(
        !event.canExecute ||
        fileManager.directoryModel.getFileListSelection().getCheckSelectMode());
  }
})();

/**
 * Sets the system wallpaper to the selected file.
 */
CommandHandler.COMMANDS_['set-wallpaper'] = new (class extends FilesCommand {
  execute(event, fileManager) {
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
            chrome.wallpaper.setWallpaper(
                {
                  data: arrayBuffer,
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
  canExecute(event, fileManager) {
    const entries = fileManager.getSelection().entries;
    if (entries.length === 0) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }
    const type = FileType.getType(entries[0]);
    if (entries.length !== 1 || type.type !== 'image') {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    event.canExecute = type.subtype === 'JPEG' || type.subtype === 'PNG';
    event.command.setHidden(false);
  }
})();

/**
 * Opens settings/storage sub page.
 */
CommandHandler.COMMANDS_['volume-storage'] = new (class extends FilesCommand {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage('storage');
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute = false;
    const currentVolumeInfo = fileManager.directoryModel.getCurrentVolumeInfo();
    if (!currentVolumeInfo) {
      return;
    }

    // Can execute only for local file systems.
    if (currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.MY_FILES ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.DOWNLOADS ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.CROSTINI ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.GUEST_OS ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.ANDROID_FILES ||
        currentVolumeInfo.volumeType ==
            VolumeManagerCommon.VolumeType.DOCUMENTS_PROVIDER) {
      event.canExecute = true;
    }
  }
})();

/**
 * Opens "providers menu" to allow users to use providers/FSPs.
 */
CommandHandler.COMMANDS_['show-providers-submenu'] =
    new (class extends FilesCommand {
      execute(event, fileManager) {
        fileManager.ui.gearButton.showSubMenu();
      }

      /** @override */
      canExecute(event, fileManager) {
        if (fileManager.dialogType !== DialogType.FULL_PAGE) {
          event.canExecute = false;
        } else {
          event.canExecute = !fileManager.guestMode;
        }
      }
    })();

export {CommandUtil};
