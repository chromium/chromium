// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A command.
 * @abstract
 */
class Command {
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
  SHARE_BUTTON: 'share-menu-button',
};

/**
 * Helper function that for the given event returns the source of a share
 * action. If the source cannot be determined, this function returns
 * CommandUtil.SharingActionSourceForUMA.UNKNOWN.
 * @param {!Event} event The event that triggered share action.
 * @return {!FileTasks.SharingActionSourceForUMA}
 */
CommandUtil.getSharingActionSource = event => {
  const id = event.target.id;
  switch (id) {
    case CommandUtil.SharingActionElementId.CONTEXT_MENU:
      return FileTasks.SharingActionSourceForUMA.CONTEXT_MENU;
    case CommandUtil.SharingActionElementId.SHARE_BUTTON:
      return FileTasks.SharingActionSourceForUMA.SHARE_BUTTON;
    default: {
      console.error('Unrecognized event.target.id for sharing action "%s"', id);
      return FileTasks.SharingActionSourceForUMA.UNKNOWN;
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

  // File list (cr.ui.List).
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
  } else if (element instanceof cr.ui.List) {
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
          !DialogType.isModal(fileManager.dialogType);
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
  const command = /** @type {!cr.ui.Command} */ (
      doc.body.querySelector('command[id="' + commandId + '"]'));
  node.addEventListener('keydown', e => {
    if (command.matchesEvent(e)) {
      // Prevent cr.ui.CommandManager of handling it and leave it
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
 * @return {Command} Volume switch command.
 */
CommandUtil.createVolumeSwitchCommand = index => new class extends Command {
  execute(event, fileManager) {
    fileManager.directoryTree.activateByIndex(index - 1);
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute =
        index > 0 && index <= fileManager.directoryTree.items.length;
  }
};

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
 * @param {!string} capability Name of the capability to check for.
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
 * Handle of the command events.
 */
class CommandHandler {
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
     * @private @const {Object<cr.ui.Command>}
     */
    this.commands_ = {};

    /** @private {?Element} */
    this.lastFocusedElement_ = null;

    // Decorate command tags in the document.
    const commands = fileManager.document.querySelectorAll('command');
    for (let i = 0; i < commands.length; i++) {
      cr.ui.Command.decorate(commands[i]);
      this.commands_[commands[i].id] = commands[i];
    }

    // Register events.
    fileManager.document.addEventListener(
        'command', this.onCommand_.bind(this));
    fileManager.document.addEventListener(
        'canExecute', this.onCanExecute_.bind(this));

    cr.ui.contextMenuHandler.addEventListener(
        'show', this.onContextMenuShow_.bind(this));
    cr.ui.contextMenuHandler.addEventListener(
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
        /** @type {Command} */ (handler), event, this.fileManager_);
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
        /** @type {Command} */ (handler), event, this.fileManager_);
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
 * @private @const {Object<Command>}
 */
CommandHandler.COMMANDS_ = {};

/**
 * Unmounts external drive.
 */
CommandHandler.COMMANDS_['unmount'] = new class extends Command {
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
        console.error(
            `Cannot unmount '${volume.volumeId}': ${error.stack || error}`);
        errorCallback(volume.volumeType);
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
};

/**
 * Formats external drive.
 */
CommandHandler.COMMANDS_['format'] = new class extends Command {
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
    event.command.setHidden(!removableRoot);
  }
};

/**
 * Initiates new folder creation.
 */
CommandHandler.COMMANDS_['new-folder'] = new class extends Command {
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
                        console.error(error);
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
};

/**
 * Initiates new window creation.
 */
CommandHandler.COMMANDS_['new-window'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.launchFileManager({
      currentDirectoryURL: fileManager.getCurrentDirectoryEntry() &&
          fileManager.getCurrentDirectoryEntry().toURL()
    });
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute = fileManager.getCurrentDirectoryEntry() &&
        (fileManager.dialogType === DialogType.FULL_PAGE);
  }
};

CommandHandler.COMMANDS_['select-all'] = new class extends Command {
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
};

CommandHandler.COMMANDS_['toggle-hidden-files'] = new class extends Command {
  execute(event, fileManager) {
    const visible = !fileManager.fileFilter.isHiddenFilesVisible();
    fileManager.fileFilter.setHiddenFilesVisible(visible);
    event.command.checked = visible;  // Checkmark for "Show hidden files".
    CommandHandler.recordMenuItemSelected(
        visible ? CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_SHOW :
                  CommandHandler.MenuCommandsForUMA.HIDDEN_FILES_HIDE);
  }
};

/**
 * Toggles visibility of top-level Android folders which are not visible by
 * default.
 */
CommandHandler.COMMANDS_['toggle-hidden-android-folders'] =
    new class extends Command {
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
    event.command.checked = fileManager.fileFilter.isAllAndroidFoldersVisible();
  }
};

/**
 * Toggles drive sync settings.
 */
CommandHandler.COMMANDS_['drive-sync-settings'] = new class extends Command {
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
};

/**
 * Deletes selected files.
 */
CommandHandler.COMMANDS_['delete'] = new class extends Command {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);

    // Execute might be called without a call of canExecute method, e.g.,
    // called directly from code, crbug.com/509483. See toolbar controller
    // delete button handling, for an example.
    this.deleteEntries(entries, fileManager);
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

    event.canExecute = this.canDeleteEntries_(entries, fileManager);

    // Remove if nothing is selected, e.g. user clicked in an empty
    // space in the file list.
    const noEntries = entries.length === 0;
    event.command.setHidden(noEntries);
  }

  /**
   * Delete the entries (if the entries can be deleted).
   * @param {!Array<!Entry>} entries
   * @param {!CommandHandlerDeps} fileManager
   * @param {?FilesConfirmDialog} dialog An optional delete confirm dialog.
   *    The default delete confirm dialog will be used if |dialog| is null.
   * @public
   */
  deleteEntries(entries, fileManager, dialog = null) {
    // Verify that the entries are not fake or root entries, and that they
    // can be deleted.
    if (!entries.every(CommandUtil.shouldShowMenuItemsForEntry.bind(
            null, fileManager.volumeManager)) ||
        !this.canDeleteEntries_(entries, fileManager)) {
      return;
    }

    const message = entries.length === 1 ?
        strf('GALLERY_CONFIRM_DELETE_ONE', entries[0].name) :
        strf('GALLERY_CONFIRM_DELETE_SOME', entries.length);

    if (!dialog) {
      dialog = fileManager.ui.deleteConfirmDialog;
    } else if (dialog.showModalElement) {
      dialog.showModalElement();
    }

    dialog.show(message, () => {
      dialog.doneCallback && dialog.doneCallback();
      fileManager.fileOperationManager.deleteEntries(entries);
    }, dialog.doneCallback, null);
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
        !fileManager.directoryModel.isReadOnly() &&
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
};

/**
 * Pastes files from clipboard.
 */
CommandHandler.COMMANDS_['paste'] = new class extends Command {
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
};

/**
 * Pastes files from clipboard. This is basically same as 'paste'.
 * This command is used for always showing the Paste command to gear menu.
 */
CommandHandler.COMMANDS_['paste-into-current-folder'] =
    new class extends Command {
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
};

/**
 * Pastes files from clipboard into the selected folder.
 */
CommandHandler.COMMANDS_['paste-into-folder'] = new class extends Command {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
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
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);

    // Show this item only when one directory is selected.
    if (entries.length !== 1 || !entries[0].isDirectory ||
        !CommandUtil.shouldShowMenuItemsForEntry(
            fileManager.volumeManager, entries[0])) {
      event.canExecute = false;
      event.command.setHidden(true);
      return;
    }

    const fileTransferController = fileManager.fileTransferController;
    const directoryEntry = /** @type {DirectoryEntry|FakeEntry} */ (entries[0]);
    event.canExecute = !!fileTransferController &&
        fileTransferController.queryPasteCommandEnabled(directoryEntry);
    event.command.setHidden(false);
  }
};

/**
 * Cut/Copy command.
 * @private @const {Command}
 */
CommandHandler.cutCopyCommand_ = new class extends Command {
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
};

CommandHandler.COMMANDS_['cut'] = CommandHandler.cutCopyCommand_;
CommandHandler.COMMANDS_['copy'] = CommandHandler.cutCopyCommand_;

/**
 * Initiates file renaming.
 */
CommandHandler.COMMANDS_['rename'] = new class extends Command {
  execute(event, fileManager) {
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
    if (util.isNonModifiable(fileManager.volumeManager, entry)) {
      return;
    }
    if (event.target instanceof DirectoryTree ||
        event.target instanceof DirectoryItem) {
      let isRemovableRoot = false;
      let volumeInfo = null;
      if (entry) {
        volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
        // Checks whether the target is actually external drive or just a folder
        // inside the drive.
        if (volumeInfo &&
            CommandUtil.isRootEntry(fileManager.volumeManager, entry)) {
          isRemovableRoot = true;
        }
      }

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
      fileManager.namingController.initiateRename();
    }
  }

  /** @override */
  canExecute(event, fileManager) {
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
    event.canExecute = entries.length === 1 && volumeIsNotReadOnly &&
        CommandUtil.hasCapability(fileManager, entries, 'canRename');
    event.command.setHidden(false);
  }
};

/**
 * Opens drive help.
 */
CommandHandler.COMMANDS_['volume-help'] = new class extends Command {
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
    const hideHelp = DialogType.isModal(fileManager.dialogType);
    event.canExecute = !hideHelp;
    event.command.setHidden(hideHelp);
  }
};

/**
 * Opens the send feedback window with pre-populated content.
 */
CommandHandler.COMMANDS_['send-feedback'] = new class extends Command {
  execute(event, fileManager) {
    const message = {
      categoryTag: 'chromeos-files-app',
      requestFeedback: true,
      feedbackInfo: {
        description: '',
      },
    };

    const kFeedbackExtensionId = 'gfdkimpbcpahaombhbimeihdjnejgicl';
    // On ChromiumOS the feedback extension is not installed, so we just log
    // that filing feedback has failed.
    chrome.runtime.sendMessage(kFeedbackExtensionId, message, (response) => {
      if (chrome.runtime.lastError) {
        console.log(
            'Failed to send feedback: ' + chrome.runtime.lastError.message);
      }
    });
  }
};

/**
 * Opens drive buy-more-space url.
 */
CommandHandler.COMMANDS_['drive-buy-more-space'] = new class extends Command {
  execute(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_BUY_STORAGE_URL'));
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.DRIVE_BUY_MORE_SPACE);
  }

  /** @override */
  canExecute(event, fileManager) {
    CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly(event, fileManager);
  }
};

/**
 * Opens drive.google.com.
 */
CommandHandler.COMMANDS_['drive-go-to-drive'] = new class extends Command {
  execute(event, fileManager) {
    util.visitURL(str('GOOGLE_DRIVE_ROOT_URL'));
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.DRIVE_GO_TO_DRIVE);
  }

  /** @override */
  canExecute(event, fileManager) {
    CommandUtil.canExecuteVisibleOnDriveInNormalAppModeOnly(event, fileManager);
  }
};

/**
 * Opens a file with default task.
 */
CommandHandler.COMMANDS_['default-task'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.taskController.executeDefaultTask();
  }

  /** @override */
  canExecute(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteDefaultTask();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
};

/**
 * Displays "open with" dialog for current selection.
 */
CommandHandler.COMMANDS_['open-with'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.taskController.getFileTasks()
        .then(tasks => {
          tasks.showTaskPicker(
              fileManager.ui.defaultTaskPicker, str('OPEN_WITH_BUTTON_LABEL'),
              '', task => {
                tasks.execute(task);
              }, FileTasks.TaskPickerType.OpenWith);
        })
        .catch(error => {
          if (error) {
            console.error(error.stack || error);
          }
        });
  }

  /** @override */
  canExecute(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteOpenActions();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
};

/**
 * Displays "More actions" dialog for current selection.
 */
CommandHandler.COMMANDS_['more-actions'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.taskController.getFileTasks()
        .then(tasks => {
          tasks.showTaskPicker(
              fileManager.ui.defaultTaskPicker,
              str('MORE_ACTIONS_BUTTON_LABEL'), '', task => {
                tasks.execute(task);
              }, FileTasks.TaskPickerType.MoreActions);
        })
        .catch(error => {
          if (error) {
            console.error(error.stack || error);
          }
        });
  }

  /** @override */
  canExecute(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteMoreActions() &&
        !util.isSharesheetEnabled();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
};

/**
 * Displays any available (child) sub menu for current selection.
 */
CommandHandler.COMMANDS_['show-submenu'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.ui.shareMenuButton.showSubMenu();
  }

  /** @override */
  canExecute(event, fileManager) {
    const canExecute = fileManager.taskController.canExecuteShowOverflow();
    event.canExecute = canExecute;
    event.command.setHidden(!canExecute);
  }
};


/**
 * Invoke Sharesheet.
 */
CommandHandler.COMMANDS_['invoke-sharesheet'] = new class extends Command {
  execute(event, fileManager) {
    const entries = fileManager.selectionHandler.selection.entries;
    chrome.fileManagerPrivate.invokeSharesheet(entries, () => {
      if (chrome.runtime.lastError) {
        console.error(chrome.runtime.lastError.message);
        return;
      }
    });
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = fileManager.selectionHandler.selection.entries;

    if (!util.isSharesheetEnabled() || !entries || entries.length === 0 ||
        (entries.some(entry => entry.isDirectory) &&
         (!CommandUtil.isDriveEntries(entries, fileManager.volumeManager) ||
          entries.length > 1))) {
      event.canExecute = false;
      event.command.setHidden(true);
      event.command.disabled = true;
      return;
    }

    event.canExecute = true;
    event.command.disabled = true;
    chrome.fileManagerPrivate.sharesheetHasTargets(entries, hasTargets => {
      if (chrome.runtime.lastError) {
        console.error(chrome.runtime.lastError.message);
        return;
      }
      event.command.setHidden(!hasTargets);
      event.canExecute = hasTargets;
      event.command.disabled = !hasTargets;
    });
  }
};

CommandHandler.COMMANDS_['toggle-holding-space'] = new class extends Command {
  constructor() {
    super();
    /**
     * Whether the command adds or removed items from holding space. The value
     * is set in <code>canExecute()</code>. It will be true unless all selected
     * items are already in the holding space.
     * @private {boolean|undefined}
     */
    this.addsItems_;
  }

  execute(event, fileManager) {
    if (this.addsItems_ === undefined) {
      return;
    }

    const entries = fileManager.selectionHandler.selection.entries;
    chrome.fileManagerPrivate.toggleAddedToHoldingSpace(
        entries, this.addsItems_);
  }

  /** @override */
  canExecute(event, fileManager) {
    const command = event.command;

    if (!util.isHoldingSpaceEnabled()) {
      event.canExecute = false;
      command.setHidden(true);
      return;
    }

    const allowedVolumeTypes = [
      VolumeManagerCommon.VolumeType.MY_FILES,
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      VolumeManagerCommon.VolumeType.DRIVE,
      VolumeManagerCommon.VolumeType.CROSTINI,
      VolumeManagerCommon.VolumeType.ANDROID_FILES,
    ];

    const currentVolumeInfo = fileManager.directoryModel.getCurrentVolumeInfo();
    if (!currentVolumeInfo ||
        !allowedVolumeTypes.includes(currentVolumeInfo.volumeType)) {
      event.canExecute = false;
      command.setHidden(true);
      return;
    }

    const entries = fileManager.selectionHandler.selection.entries;

    if (!entries || entries.length === 0) {
      event.canExecute = false;
      command.setHidden(true);
      return;
    }

    event.canExecute = true;
    command.setHidden(false);

    // Update the command to add or remove holding space items depending on the
    // current holding space state - the command will remove items only if all
    // currently selected items are already in the holding space.
    chrome.fileManagerPrivate.getHoldingSpaceState((state) => {
      if (!state) {
        command.setHidden(true);
        return;
      }

      const itemsSet = {};
      state.itemUrls.forEach((item) => itemsSet[item] = true);

      const selectedUrls = util.entriesToURLs(entries);
      this.addsItems_ = selectedUrls.some(url => !itemsSet[url]);

      command.label = this.addsItems_ ?
          str('HOLDING_SPACE_PIN_TO_SHELF_COMMAND_LABEL') :
          str('HOLDING_SPACE_UNPIN_FROM_SHELF_COMMAND_LABEL');
    });
  }
};

/**
 * Opens containing folder of the focused file.
 */
CommandHandler.COMMANDS_['go-to-file-location'] = new class extends Command {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
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
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1;
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Displays QuickView for current selection.
 */
CommandHandler.COMMANDS_['get-info'] = new class extends Command {
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
};

/**
 * Focuses search input box.
 */
CommandHandler.COMMANDS_['search'] = new class extends Command {
  execute(event, fileManager) {
    // Cancel item selection.
    fileManager.directoryModel.clearSelection();

    // Focus and unhide the search box.
    const element = fileManager.document.querySelector('#search-box cr-input');
    element.disabled = false;
    (/** @type {!CrInputElement} */ (element)).select();
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute = !fileManager.namingController.isRenamingInProgress();
  }
};

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
CommandHandler.COMMANDS_['toggle-pinned'] = new class extends Command {
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
};

/**
 * Creates zip file for current selection.
 */
CommandHandler.COMMANDS_['zip-selection'] = new class extends Command {
  execute(event, fileManager) {
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    if (!dirEntry ||
        !fileManager.getSelection().entries.every(
            CommandUtil.shouldShowMenuItemsForEntry.bind(
                null, fileManager.volumeManager))) {
      return;
    }

    if (util.isZipPackEnabled()) {
      // TODO(crbug.com/912236) Implement and remove error notification.
      const item = new ProgressCenterItem();
      item.id = 'no_zip';
      item.message = 'Cannot zip selection: Not implemented yet';
      item.state = ProgressItemState.ERROR;
      fileManager.progressCenter.updateItem(item);
    } else {
      fileManager.taskController.getFileTasks()
          .then(tasks => {
            if (fileManager.directoryModel.isOnDrive() ||
                fileManager.directoryModel.isOnMTP()) {
              tasks.execute(/** @type {chrome.fileManagerPrivate.FileTask} */ (
                  {taskId: FileTasks.ZIP_ARCHIVER_ZIP_USING_TMP_TASK_ID}));
            } else {
              tasks.execute(/** @type {chrome.fileManagerPrivate.FileTask} */ (
                  {taskId: FileTasks.ZIP_ARCHIVER_ZIP_TASK_ID}));
            }
          })
          .catch(error => {
            if (error) {
              console.error(error.stack || error);
            }
          });
    }
  }

  /** @override */
  canExecute(event, fileManager) {
    const dirEntry = fileManager.getCurrentDirectoryEntry();
    const selection = fileManager.getSelection();

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
    event.canExecute = dirEntry && !fileManager.directoryModel.isReadOnly() &&
        selection && selection.totalCount > 0;
  }
};

/**
 * Shows the share dialog for the current selection (single only).
 */
CommandHandler.COMMANDS_['share'] = new class extends Command {
  execute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    FileTasks.recordSharingActionUMA_(
        CommandUtil.getSharingActionSource(event), entries);
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
};

/**
 * Opens the file in Drive for the user to manage sharing permissions etc.
 */
CommandHandler.COMMANDS_['manage-in-drive'] = new class extends Command {
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
};

/**
 * Shares the selected (single only) directory with the default crostini VM.
 */
CommandHandler.COMMANDS_['share-with-linux'] = new class extends Command {
  execute(event, fileManager) {
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
    if (!entry || !entry.isDirectory) {
      return;
    }
    const dir = /** @type {!DirectoryEntry} */ (entry);
    const info = fileManager.volumeManager.getLocationInfo(dir);
    if (!info) {
      return;
    }
    function share() {
      // Always persist shares via right-click > Share with Linux.
      chrome.fileManagerPrivate.sharePathsWithCrostini(
          constants.DEFAULT_CROSTINI_VM, [dir], true /* persist */, () => {
            if (chrome.runtime.lastError) {
              console.error(
                  'Error sharing with linux: ' +
                  chrome.runtime.lastError.message);
            }
          });
      // Register the share and show the 'Manage Linux sharing' toast
      // immediately, since the container may take 10s or more to start.
      fileManager.crostini.registerSharedPath(
          constants.DEFAULT_CROSTINI_VM, dir);
      fileManager.ui.toast.show(str('FOLDER_SHARED_WITH_CROSTINI'), {
        text: str('MANAGE_TOAST_BUTTON_LABEL'),
        callback: () => {
          chrome.fileManagerPrivate.openSettingsSubpage('crostini/sharedPaths');
          CommandHandler.recordMenuItemSelected(
              CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING_TOAST);
        }
      });
    }
    // Show a confirmation dialog if we are sharing the root of a volume.
    // Non-Drive volume roots are always '/'.
    if (dir.fullPath == '/') {
      fileManager.ui.confirmDialog.showHtml(
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_TITLE'),
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI', info.volumeInfo.label), share,
          () => {});
    } else if (
        info.isRootEntry &&
        (info.rootType == VolumeManagerCommon.RootType.DRIVE ||
         info.rootType == VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
         info.rootType ==
             VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT)) {
      // Only show the dialog for My Drive, Shared Drives Grand Root and
      // Computers Grand Root.  Do not show for roots of a single Shared Drive
      // or Computer.
      fileManager.ui.confirmDialog.showHtml(
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_TITLE'),
          strf('SHARE_ROOT_FOLDER_WITH_CROSTINI_DRIVE'), share, () => {});
    } else {
      // This is not a root, share it without confirmation dialog.
      share();
    }
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.SHARE_WITH_LINUX);
  }

  /** @override */
  canExecute(event, fileManager) {
    // Must be single directory not already shared.
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        !fileManager.crostini.isPathShared(
            constants.DEFAULT_CROSTINI_VM, entries[0]) &&
        fileManager.crostini.canSharePath(
            constants.DEFAULT_CROSTINI_VM, entries[0], true /* persist */);
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Shares the selected (single only) directory with the Plugin VM.
 */
CommandHandler.COMMANDS_['share-with-plugin-vm'] = new class extends Command {
  execute(event, fileManager) {
    const entry = CommandUtil.getCommandEntry(fileManager, event.target);
    if (!entry || !entry.isDirectory) {
      return;
    }
    const dir = /** @type {!DirectoryEntry} */ (entry);
    const info = fileManager.volumeManager.getLocationInfo(dir);
    if (!info) {
      return;
    }
    function share() {
      // Always persist shares via right-click > Share with PluginVM.
      chrome.fileManagerPrivate.sharePathsWithCrostini(
          constants.PLUGIN_VM, [dir], true /* persist */, () => {
            if (chrome.runtime.lastError) {
              console.error(
                  'Error sharing with Plugin VM: ' +
                  chrome.runtime.lastError.message);
            }
          });
      // Register the share and show the 'Manage PluginVM sharing' toast
      // immediately, since the container may take 10s or more to start.
      fileManager.crostini.registerSharedPath(constants.PLUGIN_VM, dir);
      fileManager.ui.toast.show(str('FOLDER_SHARED_WITH_PLUGIN_VM'), {
        text: str('MANAGE_TOAST_BUTTON_LABEL'),
        callback: () => {
          chrome.fileManagerPrivate.openSettingsSubpage(
              'app-management/pluginVm/sharedPaths');
          CommandHandler.recordMenuItemSelected(
              CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING_TOAST);
        }
      });
    }
    // Show a confirmation dialog if we are sharing the root of a volume.
    // Non-Drive volume roots are always '/'.
    if (dir.fullPath == '/') {
      fileManager.ui.confirmDialog.showHtml(
          strf('SHARE_ROOT_FOLDER_WITH_PLUGIN_VM_TITLE'),
          strf('SHARE_ROOT_FOLDER_WITH_PLUGIN_VM', info.volumeInfo.label),
          share, () => {});
    } else if (
        info.isRootEntry &&
        (info.rootType == VolumeManagerCommon.RootType.DRIVE ||
         info.rootType == VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT ||
         info.rootType ==
             VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT)) {
      // Only show the dialog for My Drive, Shared Drives Grand Root and
      // Computers Grand Root.  Do not show for roots of a single Shared Drive
      // or Computer.
      fileManager.ui.confirmDialog.showHtml(
          strf('SHARE_ROOT_FOLDER_WITH_PLUGIN_VM_TITLE'),
          strf('SHARE_ROOT_FOLDER_WITH_PLUGIN_VM_DRIVE'), share, () => {});
    } else {
      // This is not a root, share it without confirmation dialog.
      share();
    }
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.SHARE_WITH_PLUGIN_VM);
  }

  /** @override */
  canExecute(event, fileManager) {
    // Must be single directory subfolder of Downloads not already shared.
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        !fileManager.crostini.isPathShared(constants.PLUGIN_VM, entries[0]) &&
        fileManager.crostini.canSharePath(
            constants.PLUGIN_VM, entries[0], true /* persist */);
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Link to settings page from gear menu.  Allows the user to manage files and
 * folders shared with the crostini container.
 */
CommandHandler.COMMANDS_['manage-linux-sharing-gear'] =
    new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage('crostini/sharedPaths');
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING);
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute =
        fileManager.crostini.isEnabled(constants.DEFAULT_CROSTINI_VM);
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Link to settings page from file context menus (not gear menu).  Allows
 * the user to manage files and folders shared with the crostini container.
 */
CommandHandler.COMMANDS_['manage-linux-sharing'] = new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage('crostini/sharedPaths');
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.MANAGE_LINUX_SHARING);
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        fileManager.crostini.isPathShared(
            constants.DEFAULT_CROSTINI_VM, entries[0]);
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Link to settings page from gear menu.  Allows the user to manage files and
 * folders shared with the Plugin VM.
 */
CommandHandler.COMMANDS_['manage-plugin-vm-sharing-gear'] =
    new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage(
        'app-management/pluginVm/sharedPaths');
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING);
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute = fileManager.crostini.isEnabled(constants.PLUGIN_VM);
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Link to settings page from file context menus (not gear menu).  Allows
 * the user to manage files and folders shared with the Plugin VM.
 */
CommandHandler.COMMANDS_['manage-plugin-vm-sharing'] =
    new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openSettingsSubpage(
        'app-management/pluginVm/sharedPaths');
    CommandHandler.recordMenuItemSelected(
        CommandHandler.MenuCommandsForUMA.MANAGE_PLUGIN_VM_SHARING);
  }

  /** @override */
  canExecute(event, fileManager) {
    const entries = CommandUtil.getCommandEntries(fileManager, event.target);
    event.canExecute = entries.length === 1 && entries[0].isDirectory &&
        fileManager.crostini.isPathShared(constants.PLUGIN_VM, entries[0]);
    event.command.setHidden(!event.canExecute);
  }
};

/**
 * Creates a shortcut of the selected folder (single only).
 */
CommandHandler.COMMANDS_['pin-folder'] = new class extends Command {
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
};

/**
 * Removes the folder shortcut.
 */
CommandHandler.COMMANDS_['unpin-folder'] = new class extends Command {
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
};

/**
 * Zoom in to the Files app.
 */
CommandHandler.COMMANDS_['zoom-in'] = new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.IN);
  }
};

/**
 * Zoom out from the Files app.
 */
CommandHandler.COMMANDS_['zoom-out'] = new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.OUT);
  }
};

/**
 * Reset the zoom factor.
 */
CommandHandler.COMMANDS_['zoom-reset'] = new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.zoom(
        chrome.fileManagerPrivate.ZoomOperationType.RESET);
  }
};

/**
 * Sort the file list by name (in ascending order).
 */
CommandHandler.COMMANDS_['sort-by-name'] = new class extends Command {
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('name', 'asc');
      const msg = strf('COLUMN_SORTED_ASC', str('NAME_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
};

/**
 * Sort the file list by size (in descending order).
 */
CommandHandler.COMMANDS_['sort-by-size'] = new class extends Command {
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('size', 'desc');
      const msg = strf('COLUMN_SORTED_DESC', str('SIZE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
};

/**
 * Sort the file list by type (in ascending order).
 */
CommandHandler.COMMANDS_['sort-by-type'] = new class extends Command {
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('type', 'asc');
      const msg = strf('COLUMN_SORTED_ASC', str('TYPE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
};

/**
 * Sort the file list by date-modified (in descending order).
 */
CommandHandler.COMMANDS_['sort-by-date'] = new class extends Command {
  execute(event, fileManager) {
    if (fileManager.directoryModel.getFileList()) {
      fileManager.directoryModel.getFileList().sort('modificationTime', 'desc');
      const msg = strf('COLUMN_SORTED_DESC', str('DATE_COLUMN_LABEL'));
      fileManager.ui.speakA11yMessage(msg);
    }
  }
};

/**
 * Open inspector for foreground page.
 */
CommandHandler.COMMANDS_['inspect-normal'] = new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.NORMAL);
  }
};

/**
 * Open inspector for foreground page and bring focus to the console.
 */
CommandHandler.COMMANDS_['inspect-console'] = new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.CONSOLE);
  }
};

/**
 * Open inspector for foreground page in inspect element mode.
 */
CommandHandler.COMMANDS_['inspect-element'] = new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.ELEMENT);
  }
};

/**
 * Open inspector for background page.
 */
CommandHandler.COMMANDS_['inspect-background'] = new class extends Command {
  execute(event, fileManager) {
    chrome.fileManagerPrivate.openInspector(
        chrome.fileManagerPrivate.InspectionType.BACKGROUND);
  }
};

/**
 * Shows a suggest dialog with new services to be added to the left nav.
 */
CommandHandler.COMMANDS_['install-new-extension'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.ui.suggestAppsDialog.showProviders((result, itemId) => {
      // If a new provider is installed, then launch it so the configuration
      // dialog is shown (if it's available).
      if (result === SuggestAppsDialog.Result.SUCCESS) {
        fileManager.providersModel.requestMount(assert(itemId));
      }
    });
  }

  /** @override */
  canExecute(event, fileManager) {
    const isFullPage = fileManager.dialogType === DialogType.FULL_PAGE;
    event.canExecute = isFullPage && navigator.onLine;
    event.command.setHidden(!isFullPage);
  }
};

/**
 * Opens the gear menu.
 */
CommandHandler.COMMANDS_['open-gear-menu'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.ui.gearButton.showMenu(true);
  }
};

/**
 * Focus the first button visible on action bar (at the top).
 */
CommandHandler.COMMANDS_['focus-action-bar'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.ui.actionbar
        .querySelector('button:not([hidden]), cr-button:not([hidden])')
        .focus();
  }
};

/**
 * Handle back button.
 */
CommandHandler.COMMANDS_['browser-back'] = new class extends Command {
  execute(event, fileManager) {
    // TODO(fukino): It should be better to minimize Files app only when there
    // is no back stack, and otherwise use BrowserBack for history navigation.
    // https://crbug.com/624100.
    const currentWindow = chrome.app.window.current();
    if (currentWindow) {
      currentWindow.minimize();
    }
  }
};

/**
 * Configures the currently selected volume.
 */
CommandHandler.COMMANDS_['configure'] = new class extends Command {
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
};

/**
 * Refreshes the currently selected directory.
 */
CommandHandler.COMMANDS_['refresh'] = new class extends Command {
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
};

/**
 * Refreshes the currently selected directory.
 */
CommandHandler.COMMANDS_['set-wallpaper'] = new class extends Command {
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
                  filename: 'wallpaper'
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
};

/**
 * Opens settings/storage sub page.
 */
CommandHandler.COMMANDS_['volume-storage'] = new class extends Command {
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
            VolumeManagerCommon.VolumeType.ANDROID_FILES) {
      event.canExecute = true;
    }
  }
};

/**
 * Opens "providers menu" to allow users to install new providers/FSPs.
 */
CommandHandler.COMMANDS_['new-service'] = new class extends Command {
  execute(event, fileManager) {
    fileManager.ui.gearButton.showSubMenu();
  }

  /** @override */
  canExecute(event, fileManager) {
    event.canExecute =
        (fileManager.dialogType === DialogType.FULL_PAGE &&
         !chrome.extension.inIncognitoContext);
  }
};
