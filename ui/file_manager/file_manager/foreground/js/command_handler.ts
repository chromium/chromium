// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';

import type {Crostini} from '../../background/js/crostini.js';
import type {ProgressCenter} from '../../background/js/progress_center.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {crInjectTypeAndInit} from '../../common/js/cr_ui.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import type {FilesAppState} from '../../common/js/files_app_state.js';
import {recordEnum} from '../../common/js/metrics.js';
import type {DialogType} from '../../state/state.js';

import type {ActionsController} from './actions_controller.js';
import {DEFAULT_BRUSCHETTA_VM, DEFAULT_CROSTINI_VM, PLUGIN_VM} from './constants.js';
import type {FileFilter} from './directory_contents.js';
import type {DirectoryModel} from './directory_model.js';
import type {DirectoryTreeNamingController} from './directory_tree_naming_controller.js';
import {BrowserBackCommand, ConfigureCommand, CutCopyCommand, DefaultTaskCommand, DeleteCommand, DlpRestrictionDetailsCommand, DriveBuyMoreSpaceCommand, DriveGoToDriveCommand, DriveSyncSettingsCommand, EmptyTrashCommand, EraseDeviceCommand, ExtractAllCommand, FilesSettingsCommand, FocusActionBarCommand, FormatCommand, GetInfoCommand, GoToFileLocationCommand, GuestOsManagingSharingCommand, GuestOsManagingSharingGearCommand, GuestOsShareCommand, InspectConsoleCommand, InspectElementCommand, InspectNormalCommand, InvokeSharesheetCommand, ManageInDriveCommand, ManageMirrorsyncCommand, NewFolderCommand, NewWindowCommand, OpenGearMenuCommand, OpenWithCommand, PasteCommand, PasteIntoCurrentFolderCommand, PasteIntoFolderCommand, PinFolderCommand, RefreshCommand, RenameCommand, RestoreFromTrashCommand, SearchCommand, SelectAllCommand, SendFeedbackCommand, SetWallpaperCommand, ShowProvidersSubmenuCommand, SortByDateCommand, SortByNameCommand, SortBySizeCommand, SortByTypeCommand, ToggleHiddenAndroidFoldersCommand, ToggleHiddenFilesCommand, ToggleHoldingSpaceCommand, TogglePinnedCommand, UnmountCommand, UnpinFolderCommand, VolumeHelpCommand, VolumeStorageCommand, VolumeSwitchCommand, ZipSelectionCommand, ZoomInCommand, ZoomOutCommand, ZoomResetCommand} from './file_manager_commands.js';
import {shouldIgnoreEvents} from './file_manager_commands_util.js';
import type {FileSelection, FileSelectionHandler} from './file_selection.js';
import type {FileTransferController} from './file_transfer_controller.js';
import type {MetadataModel} from './metadata/metadata_model.js';
import type {NamingController} from './naming_controller.js';
import type {ProvidersModel} from './providers_model.js';
import type {SpinnerController} from './spinner_controller.js';
import type {TaskController} from './task_controller.js';
import type {CanExecuteEvent} from './ui/command.js';
import {Command, type CommandEvent} from './ui/command.js';
import {contextMenuHandler, type HideEvent, type ShowEvent} from './ui/context_menu_handler.js';
import type {FileManagerUI} from './ui/file_manager_ui.js';

/**
 * Interface on which `CommandHandler` depends.
 */
export interface CommandHandlerDeps {
  actionsController: ActionsController;
  dialogType: DialogType;
  directoryModel: DirectoryModel;
  directoryTreeNamingController: DirectoryTreeNamingController;
  document: Document;
  fileFilter: FileFilter;
  fileTransferController: FileTransferController|null;
  selectionHandler: FileSelectionHandler;
  namingController: NamingController;
  progressCenter: ProgressCenter;
  providersModel: ProvidersModel;
  spinnerController: SpinnerController;
  taskController: TaskController;
  ui: FileManagerUI;
  volumeManager: VolumeManager;
  metadataModel: MetadataModel;
  crostini: Crostini;
  guestMode: boolean;
  trashEnabled: boolean;

  getCurrentDirectoryEntry(): DirectoryEntry|FilesAppEntry|null|undefined;
  getSelection(): FileSelection;
  launchFileManager(appState?: FilesAppState): void;
}

/**
 * Name of a command (for UMA).
 */
export enum MenuCommandsForUma {
  HELP = 'volume-help',
  DRIVE_HELP = 'volume-help-drive',
  DRIVE_BUY_MORE_SPACE = 'drive-buy-more-space',
  DRIVE_GO_TO_DRIVE = 'drive-go-to-drive',
  HIDDEN_FILES_SHOW = 'toggle-hidden-files-on',
  HIDDEN_FILES_HIDE = 'toggle-hidden-files-off',
  MOBILE_DATA_ON = 'drive-sync-settings-enabled',
  MOBILE_DATA_OFF = 'drive-sync-settings-disabled',
  DEPRECATED_SHOW_GOOGLE_DOCS_FILES_OFF = 'drive-hosted-settings-disabled',
  DEPRECATED_SHOW_GOOGLE_DOCS_FILES_ON = 'drive-hosted-settings-enabled',
  HIDDEN_ANDROID_FOLDERS_SHOW = 'toggle-hidden-android-folders-on',
  HIDDEN_ANDROID_FOLDERS_HIDE = 'toggle-hidden-android-folders-off',
  SHARE_WITH_LINUX = 'share-with-linux',
  MANAGE_LINUX_SHARING = 'manage-linux-sharing',
  MANAGE_LINUX_SHARING_TOAST = 'manage-linux-sharing-toast',
  MANAGE_LINUX_SHARING_TOAST_STARTUP = 'manage-linux-sharing-toast-startup',
  SHARE_WITH_PLUGIN_VM = 'share-with-plugin-vm',
  MANAGE_PLUGIN_VM_SHARING = 'manage-plugin-vm-sharing',
  MANAGE_PLUGIN_VM_SHARING_TOAST = 'manage-plugin-vm-sharing-toast',
  MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP =
      'manage-plugin-vm-sharing-toast-startup',
  PIN_TO_HOLDING_SPACE = 'pin-to-holding-space',
  UNPIN_FROM_HOLDING_SPACE = 'unpin-from-holding-space',
  SHARE_WITH_BRUSCHETTA = 'share-with-bruschetta',
  MANAGE_BRUSCHETTA_SHARING = 'manage-bruschetta-sharing',
  MANAGE_BRUSCHETTA_SHARING_TOAST = 'manage-bruschetta-sharing-toast',
  MANAGE_BRUSCHETTA_SHARING_TOAST_STARTUP =
      'manage-bruschetta-sharing-toast-startup',
}

const cutCopyCommand = new CutCopyCommand();
const deleteCommand = new DeleteCommand();

const crostiniSettings = 'crostini/sharedPaths';
const pluginVmSettings = 'app-management/pluginVm/sharedPaths';
const bruschettaSettings = 'bruschetta/sharedPaths';

/**
 * A map of FilesCommand to the ID that is used in the DOM to reference them.
 */
const FilesCommands = {
  'focus-action-bar': new FocusActionBarCommand(),
  'open-gear-menu': new OpenGearMenuCommand(),
  'inspect-element': new InspectElementCommand(),
  'inspect-console': new InspectConsoleCommand(),
  'inspect-normal': new InspectNormalCommand(),
  'sort-by-date': new SortByDateCommand(),
  'sort-by-type': new SortByTypeCommand(),
  'sort-by-size': new SortBySizeCommand(),
  'sort-by-name': new SortByNameCommand(),
  'zoom-reset': new ZoomResetCommand(),
  'zoom-out': new ZoomOutCommand(),
  'zoom-in': new ZoomInCommand(),
  'unpin-folder': new UnpinFolderCommand(),
  'pin-folder': new PinFolderCommand(),
  'manage-mirrorsync': new ManageMirrorsyncCommand(),
  'manage-in-drive': new ManageInDriveCommand(),
  'zip-selection': new ZipSelectionCommand(),
  'extract-all': new ExtractAllCommand(),
  'toggle-pinned': new TogglePinnedCommand(),
  'search': new SearchCommand(),
  'dlp-restriction-details': new DlpRestrictionDetailsCommand(),
  'get-info': new GetInfoCommand(),
  'go-to-file-location': new GoToFileLocationCommand(),
  'toggle-holding-space': new ToggleHoldingSpaceCommand(),
  'invoke-sharesheet': new InvokeSharesheetCommand(),
  'open-with': new OpenWithCommand(),
  'default-task': new DefaultTaskCommand(),
  'drive-go-to-drive': new DriveGoToDriveCommand(),
  'drive-buy-more-space': new DriveBuyMoreSpaceCommand(),
  'send-feedback': new SendFeedbackCommand(),
  'volume-help': new VolumeHelpCommand(),
  'files-settings': new FilesSettingsCommand(),
  'rename': new RenameCommand(),
  'cut': cutCopyCommand,
  'copy': cutCopyCommand,
  'paste-into-folder': new PasteIntoFolderCommand(),
  'paste-into-current-folder': new PasteIntoCurrentFolderCommand(),
  'paste': new PasteCommand(),
  'empty-trash': new EmptyTrashCommand(),
  'restore-from-trash': new RestoreFromTrashCommand(),
  'delete': deleteCommand,
  'move-to-trash': deleteCommand,
  'drive-sync-settings': new DriveSyncSettingsCommand(),
  'toggle-hidden-android-folders': new ToggleHiddenAndroidFoldersCommand(),
  'toggle-hidden-files': new ToggleHiddenFilesCommand(),
  'select-all': new SelectAllCommand(),
  'new-window': new NewWindowCommand(),
  'new-folder': new NewFolderCommand(),
  'erase-device': new EraseDeviceCommand(),
  'format': new FormatCommand(),
  'unmount': new UnmountCommand(),
  'browser-back': new BrowserBackCommand(),
  'configure': new ConfigureCommand(),
  'refresh': new RefreshCommand(),
  'set-wallpaper': new SetWallpaperCommand(),
  'volume-storage': new VolumeStorageCommand(),
  'show-providers-submenu': new ShowProvidersSubmenuCommand(),
  'volume-switch-1': new VolumeSwitchCommand(1),
  'volume-switch-2': new VolumeSwitchCommand(2),
  'volume-switch-3': new VolumeSwitchCommand(3),
  'volume-switch-4': new VolumeSwitchCommand(4),
  'volume-switch-5': new VolumeSwitchCommand(5),
  'volume-switch-6': new VolumeSwitchCommand(6),
  'volume-switch-7': new VolumeSwitchCommand(7),
  'volume-switch-8': new VolumeSwitchCommand(8),
  'volume-switch-9': new VolumeSwitchCommand(9),
  'share-with-linux': new GuestOsShareCommand(
      DEFAULT_CROSTINI_VM, 'CROSTINI', crostiniSettings,
      MenuCommandsForUma.MANAGE_LINUX_SHARING_TOAST,
      MenuCommandsForUma.SHARE_WITH_LINUX),
  'share-with-plugin-vm': new GuestOsShareCommand(
      PLUGIN_VM, 'PLUGIN_VM', pluginVmSettings,
      MenuCommandsForUma.MANAGE_PLUGIN_VM_SHARING_TOAST,
      MenuCommandsForUma.SHARE_WITH_PLUGIN_VM),
  'share-with-bruschetta': new GuestOsShareCommand(
      DEFAULT_BRUSCHETTA_VM, 'BRUSCHETTA', bruschettaSettings,
      MenuCommandsForUma.MANAGE_BRUSCHETTA_SHARING_TOAST,
      MenuCommandsForUma.SHARE_WITH_BRUSCHETTA),
  'manage-linux-sharing-gear': new GuestOsManagingSharingGearCommand(
      DEFAULT_CROSTINI_VM, crostiniSettings,
      MenuCommandsForUma.MANAGE_LINUX_SHARING),
  'manage-plugin-vm-sharing-gear': new GuestOsManagingSharingGearCommand(
      PLUGIN_VM, pluginVmSettings, MenuCommandsForUma.MANAGE_PLUGIN_VM_SHARING),
  'manage-bruschetta-sharing-gear': new GuestOsManagingSharingGearCommand(
      DEFAULT_BRUSCHETTA_VM, bruschettaSettings,
      MenuCommandsForUma.MANAGE_BRUSCHETTA_SHARING),
  'manage-linux-sharing': new GuestOsManagingSharingCommand(
      DEFAULT_CROSTINI_VM, crostiniSettings,
      MenuCommandsForUma.MANAGE_LINUX_SHARING),
  'manage-plugin-vm-sharing': new GuestOsManagingSharingCommand(
      PLUGIN_VM, pluginVmSettings, MenuCommandsForUma.MANAGE_PLUGIN_VM_SHARING),
  'manage-bruschetta-sharing': new GuestOsManagingSharingCommand(
      DEFAULT_BRUSCHETTA_VM, bruschettaSettings,
      MenuCommandsForUma.MANAGE_BRUSCHETTA_SHARING),
};

/**
 * Strongly type any places which can ONLY provide a key that directly maps to a
 * files command above.
 */
export type FilesCommandId = keyof typeof FilesCommands;

/**
 * Handle of the command events.
 */
export class CommandHandler {
  /**
   * Command elements.
   */
  private commands_: Record<string, Command> = {};
  private lastFocusedElement_: HTMLElement|null = null;

  /**
   * @param fileManager_ Classes |CommandHandler| depends.
   */
  constructor(private fileManager_: CommandHandlerDeps) {
    // Decorate command tags in the document.
    const commands =
        this.fileManager_.document.querySelectorAll<Command>('command');

    for (const command of commands) {
      crInjectTypeAndInit(command, Command);
      this.commands_[command.id] = command;
    }

    // Register events.
    this.fileManager_.document.addEventListener(
        'command', this.onCommand_.bind(this) as EventListener);
    this.fileManager_.document.addEventListener(
        'canExecute', this.onCanExecute_.bind(this) as EventListener);

    contextMenuHandler.addEventListener(
        'show', this.onContextMenuShow_.bind(this));
    contextMenuHandler.addEventListener(
        'hide', this.onContextMenuHide_.bind(this));
  }

  private onContextMenuShow_(event: ShowEvent) {
    this.lastFocusedElement_ = document.activeElement as HTMLElement;
    const menu = event.detail.menu;
    // Set focus asynchronously to give time for menu "show" event to finish and
    // have all items set up before focusing.
    setTimeout(() => {
      if (!menu.hidden) {
        menu.focusSelectedItem();
      }
    }, 0);
  }

  private onContextMenuHide_(_event: HideEvent) {
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
   */
  private onCommand_(event: CommandEvent) {
    assert(this.fileManager_.document);
    if (shouldIgnoreEvents(this.fileManager_.document)) {
      return;
    }
    const commandId = event.detail.command.id as FilesCommandId;
    const handler = FilesCommands[commandId];
    handler.execute(event, this.fileManager_);
  }

  /**
   * Handles canExecute events.
   */
  private onCanExecute_(event: CanExecuteEvent) {
    assert(this.fileManager_.document);
    if (shouldIgnoreEvents(this.fileManager_.document)) {
      return;
    }
    const commandId = event.command.id as FilesCommandId;
    const handler = FilesCommands[commandId]!;
    handler.canExecute(event, this.fileManager_);
  }

  /**
   * Returns command handler by name.
   */
  static getCommand<K extends FilesCommandId>(name: K):
      typeof FilesCommands[K] {
    return FilesCommands[name];
  }
}

/**
 * Keep the order of this in sync with FileManagerMenuCommands in
 * tools/metrics/histograms/enums.xml.
 * The array indices will be recorded in UMA as enum values. The index for each
 * root type should never be renumbered nor reused in this array.
 */
export const ValidMenuCommandsForUma = [
  MenuCommandsForUma.HELP,
  MenuCommandsForUma.DRIVE_HELP,
  MenuCommandsForUma.DRIVE_BUY_MORE_SPACE,
  MenuCommandsForUma.DRIVE_GO_TO_DRIVE,
  MenuCommandsForUma.HIDDEN_FILES_SHOW,
  MenuCommandsForUma.HIDDEN_FILES_HIDE,
  MenuCommandsForUma.MOBILE_DATA_ON,
  MenuCommandsForUma.MOBILE_DATA_OFF,
  MenuCommandsForUma.DEPRECATED_SHOW_GOOGLE_DOCS_FILES_OFF,
  MenuCommandsForUma.DEPRECATED_SHOW_GOOGLE_DOCS_FILES_ON,
  MenuCommandsForUma.HIDDEN_ANDROID_FOLDERS_SHOW,
  MenuCommandsForUma.HIDDEN_ANDROID_FOLDERS_HIDE,
  MenuCommandsForUma.SHARE_WITH_LINUX,
  MenuCommandsForUma.MANAGE_LINUX_SHARING,
  MenuCommandsForUma.MANAGE_LINUX_SHARING_TOAST,
  MenuCommandsForUma.MANAGE_LINUX_SHARING_TOAST_STARTUP,
  MenuCommandsForUma.SHARE_WITH_PLUGIN_VM,
  MenuCommandsForUma.MANAGE_PLUGIN_VM_SHARING,
  MenuCommandsForUma.MANAGE_PLUGIN_VM_SHARING_TOAST,
  MenuCommandsForUma.MANAGE_PLUGIN_VM_SHARING_TOAST_STARTUP,
  MenuCommandsForUma.PIN_TO_HOLDING_SPACE,
  MenuCommandsForUma.UNPIN_FROM_HOLDING_SPACE,
  MenuCommandsForUma.SHARE_WITH_BRUSCHETTA,
  MenuCommandsForUma.MANAGE_BRUSCHETTA_SHARING,
  MenuCommandsForUma.MANAGE_BRUSCHETTA_SHARING_TOAST,
  MenuCommandsForUma.MANAGE_BRUSCHETTA_SHARING_TOAST_STARTUP,
];
console.assert(
    Object.keys(MenuCommandsForUma).length === ValidMenuCommandsForUma.length,
    'Members in ValidMenuCommandsForUma do not match those in ' +
        'MenuCommandsForUma.');

/**
 * Records the menu item as selected in UMA.
 */
export function recordMenuItemSelected(menuItem: MenuCommandsForUma) {
  recordEnum('MenuItemSelected', menuItem, ValidMenuCommandsForUma);
}
