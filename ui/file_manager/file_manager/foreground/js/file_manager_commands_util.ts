// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {isModal} from '../../common/js/dialog_type.js';
import {getFocusedTreeItem} from '../../common/js/dom_utils.js';
import {getTreeItemEntry, isFakeEntry, isInteractiveVolume, isSameEntry, isSameVolume, isTeamDriveRoot, isTeamDrivesGrandRoot, isTrashRootType} from '../../common/js/entry_utils.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import type {State} from '../../state/state.js';
import {getFileData} from '../../state/store.js';
import {isTreeItem} from '../../widgets/xf_tree_util.js';

import type {CommandHandlerDeps} from './command_handler.js';
import type {DirectoryModel} from './directory_model.js';
import type {FileSelection} from './file_selection.js';
import type {MetadataKey} from './metadata/metadata_item.js';
import type {CanExecuteEvent, Command, CommandEvent} from './ui/command.js';
import {List} from './ui/list.js';
import type {Menu} from './ui/menu.js';
import type {MenuItem} from './ui/menu_item.js';

/**
 * The IDs of elements that can trigger share action.
 */
enum SharingActionElementId {
  CONTEXT_MENU = 'file-list',
  SHARE_SHEET = 'sharesheet-button',
}

function isList(element: EventTarget|null): element is List {
  if (element && 'selectedItems' in element) {
    return true;
  }
  return false;
}

function isMenu(element: EventTarget|null): element is Menu {
  if (element && 'contextElement' in element) {
    return true;
  }
  return false;
}

function isMenuItem(element: EventTarget|null): element is MenuItem {
  if (element && 'parentElement' in element &&
      isMenu(element.parentElement as EventTarget)) {
    return true;
  }
  return false;
}


/**
 * Helper function that for the given event returns the launch source of the
 * sharesheet. If the source cannot be determined, this function returns
 * chrome.fileManagerPrivate.SharesheetLaunchSource.UNKNOWN.
 */
export function getSharesheetLaunchSource(event: Event) {
  const id = (event.target as Command).id;
  switch (id) {
    case SharingActionElementId.CONTEXT_MENU:
      return chrome.fileManagerPrivate.SharesheetLaunchSource.CONTEXT_MENU;
    case SharingActionElementId.SHARE_SHEET:
      return chrome.fileManagerPrivate.SharesheetLaunchSource.SHARESHEET_BUTTON;
    default: {
      console.error('Unrecognized event.target.id for sharesheet launch', id);
      return chrome.fileManagerPrivate.SharesheetLaunchSource.UNKNOWN;
    }
  }
}

/**
 * Extracts entry on which command event was dispatched.
 */
export function getCommandEntry(
    fileManager: CommandHandlerDeps, element: EventTarget|null): Entry|
    FilesAppEntry|undefined {
  const entries = getCommandEntries(fileManager, element);
  return entries.length === 0 ? undefined : entries[0]!;
}

/**
 * Extracts entries on which command event was dispatched.
 */
export function getCommandEntries(
    fileManager: CommandHandlerDeps,
    element: EventTarget|null): Array<Entry|FilesAppEntry> {
  if (isTreeItem(element)) {
    const entry = getTreeItemEntry(element);
    if (entry) {
      return [entry];
    }
  }

  // DirectoryTree has the focused item.
  const focusedItem = getFocusedTreeItem(element);
  const entry = getTreeItemEntry(focusedItem);
  if (entry) {
    return [entry];
  }

  const htmlElement = element as HTMLElement;
  // The event target could still be a descendant of a legacy TreeItem element
  // (e.g. the eject button).
  // Handle eject button in the new directory tree.
  if (htmlElement.classList.contains('root-eject')) {
    const treeItem = htmlElement.closest('xf-tree-item');
    const entry = treeItem && getTreeItemEntry(treeItem);
    if (entry) {
      return [entry];
    }
  }

  // File list (List).
  if (isList(element) && element.selectedItems.length) {
    const entries = element.selectedItems as Array<Entry|FilesAppEntry>;
    // Check if it is Entry or not by checking for toURL().
    return entries.filter(entry => ('toURL' in entry));
  }

  // Commands in the action bar can only act in the currently selected files.
  if (fileManager.ui.actionbar.contains(htmlElement)) {
    return fileManager.getSelection().entries;
  }

  // Context Menu: redirect to the element the context menu is displayed for.
  if (isMenu(element) && element.contextElement) {
    return getCommandEntries(fileManager, element.contextElement);
  }

  // Context Menu Item: redirect to the element the context menu is displayed
  // for.
  if (isMenuItem(element)) {
    const menu = element.parentElement as Menu;
    if (menu.contextElement) {
      return getCommandEntries(fileManager, menu.contextElement);
    }
  }

  return [];
}

/**
 * Extracts a directory which contains entries on which command event was
 * dispatched.
 */
export function getParentEntry(
    element: EventTarget, directoryModel: DirectoryModel) {
  const focusedItem = getFocusedTreeItem(element);

  const parentItem = focusedItem?.parentItem;
  if (isTreeItem(parentItem) && getTreeItemEntry(parentItem)) {
    // DirectoryTree has the focused item.
    return getTreeItemEntry(parentItem);
  }

  if (element instanceof List) {
    return directoryModel ? directoryModel.getCurrentDirEntry() : null;
  }

  return null;
}

/**
 * Returns VolumeInfo from the current target for commands, based on |element|.
 * It can be from directory tree (clicked item or selected item), or from file
 * list selected items; or null if can determine it.
 */
export function getElementVolumeInfo(
    element: EventTarget|null, fileManager: CommandHandlerDeps): VolumeInfo|
    null|undefined {
  if (element && 'volumeInfo' in element) {
    return element.volumeInfo as VolumeInfo;
  }
  const entry = getCommandEntry(fileManager, element);
  return entry && fileManager.volumeManager.getVolumeInfo(entry);
}

/**
 * Sets the command as visible only when the current volume is drive and it's
 * running as a normal app, not as a modal dialog.
 * NOTE: This doesn't work for directory tree menu, because user can right-click
 * on any visible volume.
 */
export function canExecuteVisibleOnDriveInNormalAppModeOnly(
    event: CanExecuteEvent, fileManager: CommandHandlerDeps) {
  const enabled = fileManager.directoryModel.isOnDrive() &&
      !isModal(fileManager.dialogType);
  event.canExecute = enabled;
  event.command.setHidden(!enabled);
}

/**
 * Sets the default handler for the commandId and prevents handling
 * the keydown events for this command. Not doing that breaks relationship
 * of original keyboard event and the command. WebKit would handle it
 * differently in some cases.
 */
export function forceDefaultHandler(node: HTMLElement, commandId: string) {
  const doc = node.ownerDocument!;
  const command =
      doc.body.querySelector<Command>('command[id="' + commandId + '"]')!;
  node.addEventListener('keydown', e => {
    if (command.matchesEvent(e)) {
      e.stopPropagation();
    }
  });
  node.addEventListener('command', (event: CommandEvent) => {
    if (event.detail.command.id !== commandId) {
      return;
    }
    document.execCommand(event.detail.command.id);
    event.cancelBubble = true;
  });
  node.addEventListener(
      'canExecute',
      ((event: CanExecuteEvent) => {
        if (event.command.id !== commandId || event.target !== node) {
          return;
        }
        event.canExecute = document.queryCommandEnabled(event.command.id);
        event.command.setHidden(false);
      }) as EventListener);
}

/**
 * Returns a directory entry when only one entry is selected and it is
 * directory. Otherwise, returns null.
 * @param selection Instance of FileSelection.
 * @return Directory entry which is selected alone.
 */
export function getOnlyOneSelectedDirectory(selection: FileSelection):
    DirectoryEntry|null {
  if (!selection) {
    return null;
  }
  if (selection.totalCount !== 1) {
    return null;
  }
  if (!selection.entries[0]!.isDirectory) {
    return null;
  }
  return selection.entries[0] as DirectoryEntry;
}

/**
 * Returns true if the given entry is the root entry of the volume.
 * @param volumeManager
 * @param entry Entry or a fake entry.
 * @return True if the entry is a root entry.
 */
export function isRootEntry(
    volumeManager: VolumeManager, entry: Entry|FilesAppEntry) {
  if (!volumeManager || !entry) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  return !!volumeInfo && isSameEntry(volumeInfo.displayRoot, entry);
}

/**
 * Returns true if the given event was triggered by the selection menu button.
 * @param event Command event.
 * @return True if the event was triggered by the selection menu button.
 */
export function isFromSelectionMenu(event: Event) {
  return (event.target as HTMLElement).id === 'selection-menu-button';
}

/**
 * If entry is fake/invalid/non-interactive/root, we don't show menu items
 * intended for regular entries.
 * @param volumeManager
 * @param entry Entry or a fake entry.
 * @return True if we should show the menu items for regular entries.
 */
export function shouldShowMenuItemsForEntry(
    volumeManager: VolumeManager, entry: Entry|FilesAppEntry) {
  // If the entry is fake entry, hide context menu entries.
  if (isFakeEntry(entry)) {
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

  // If the entry belongs to a non-interactive volume, hide context menu
  // entries.
  if (!isInteractiveVolume(volumeInfo)) {
    return false;
  }

  // If the entry is root entry of its volume (but not a team drive root),
  // hide context menu entries.
  if (isRootEntry(volumeManager, entry) && !isTeamDriveRoot(entry)) {
    return false;
  }

  if (isTeamDrivesGrandRoot(entry)) {
    return false;
  }

  return true;
}

/**
 * Returns whether all of the given entries have the given capability.
 *
 * @param fileManager CommandHandlerDeps.
 * @param entries List of entries to check capabilities for.
 * @param capability Name of the capability to check for.
 */
export function hasCapability(
    fileManager: CommandHandlerDeps, entries: Array<Entry|FilesAppEntry>,
    capability: MetadataKey) {
  if (entries.length === 0) {
    return false;
  }

  // Check if the capability is true or undefined, but not false. A capability
  // can be undefined if the metadata is not fetched from the server yet (e.g.
  // if we create a new file in offline mode), or if there is a problem with the
  // cache and we don't have data yet. For this reason, we need to allow the
  // functionality even if it's not set.
  // TODO(crbug.com/41392991): Store restrictions instead of capabilities.
  const metadata = fileManager.metadataModel.getCache(entries, [capability]);
  return metadata.length === entries.length &&
      metadata.every(item => item[capability] !== false);
}

/**
 * Checks if the handler should ignore the current event, eg. since there is
 * a popup dialog currently opened.
 *
 * @return True if the event should be ignored, false otherwise.
 */
export function shouldIgnoreEvents(doc: Document) {
  // Do not handle commands, when a dialog is shown. Do not use querySelector
  // as it's much slower, and this method is executed often.
  const dialogs = doc.getElementsByClassName('cr-dialog-container');
  if (dialogs.length !== 0 && dialogs[0]!.classList.contains('shown')) {
    return true;
  }

  return false;  // Do not ignore.
}

/**
 * Returns true if all entries is inside Drive volume, which includes all Drive
 * parts (Shared Drives, My Drive, Shared with me, etc).
 */
export function isDriveEntries(
    entries: Array<Entry|FilesAppEntry>, volumeManager: VolumeManager) {
  if (!entries.length) {
    return false;
  }

  const volumeInfo = volumeManager.getVolumeInfo(entries[0]!);
  if (!volumeInfo) {
    return false;
  }

  if (volumeInfo.volumeType === VolumeType.DRIVE &&
      isSameVolume(entries, volumeManager)) {
    return true;
  }

  return false;
}

/**
 * Returns true if all entries descend from the My Drive root (e.g. not located
 * within Shared with me or Shared drives).
 */
export function isOnlyMyDriveEntries(
    entries: Array<Entry|FilesAppEntry>, state: State): boolean {
  if (!entries.length) {
    return false;
  }

  for (const entry of entries) {
    const fileData = getFileData(state, entry.toURL());
    if (!fileData) {
      return false;
    }
    if (fileData.rootType !== RootType.DRIVE) {
      return false;
    }
  }
  return true;
}

/**
 * Returns true if the current root is Trash. Items in Trash are a fake
 * representation of a file + its metadata. Some actions are infeasible and
 * items should be restored to enable these actions.
 */
export function isOnTrashRoot(fileManager: CommandHandlerDeps) {
  const currentRootType = fileManager.directoryModel.getCurrentRootType();
  if (!currentRootType) {
    return false;
  }
  return isTrashRootType(currentRootType);
}

/**
 * Extracts entry on which command event was dispatched.
 */
export function getEventEntry(event: Event, fileManager: CommandHandlerDeps):
    Entry|FilesAppEntry|undefined {
  let entry;
  const htmlElement = event.target as HTMLElement;
  if (fileManager.ui.directoryTree!.contains(htmlElement)) {
    // The command is executed from the directory tree context menu.
    entry = getCommandEntry(fileManager, htmlElement);
  } else {
    // The command is executed from the gear menu.
    entry = fileManager.directoryModel.getCurrentDirEntry();
  }
  return entry;
}

/**
 * Returns true if the current volume is interactive.
 */
export function currentVolumeIsInteractive(fileManager: CommandHandlerDeps) {
  const volumeInfo = fileManager.directoryModel.getCurrentVolumeInfo();
  if (!volumeInfo) {
    return true;
  }
  return isInteractiveVolume(volumeInfo);
}

/**
 * Returns true if any entry belongs to a non-interactive volume.
 */
export function containsNonInteractiveEntry(
    entries: Array<Entry|FilesAppEntry>, fileManager: CommandHandlerDeps) {
  return entries.some(entry => {
    const volumeInfo = fileManager.volumeManager.getVolumeInfo(entry);
    if (!volumeInfo) {
      return false;
    }
    return isInteractiveVolume(volumeInfo);
  });
}
