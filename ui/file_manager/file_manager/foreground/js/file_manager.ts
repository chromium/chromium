// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cros_components/switch/switch.js';
import '../../background/js/file_manager_base.js';
import '../../background/js/test_util.js';
import '../../widgets/xf_jellybean.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {assert, assertInstanceof} from 'chrome://resources/js/assert.js';

import type {Crostini} from '../../background/js/crostini.js';
import type {FileManagerBase} from '../../background/js/file_manager_base.js';
import type {ProgressCenter} from '../../background/js/progress_center.js';
import {getBulkPinProgress, getDialogCaller, getDlpBlockedComponents, getDriveConnectionState, getMaterializedViews, getPreferences} from '../../common/js/api.js';
import type {ArrayDataModel} from '../../common/js/array_data_model.js';
import {crInjectTypeAndInit} from '../../common/js/cr_ui.js';
import {isFolderDialogType} from '../../common/js/dialog_type.js';
import {getKeyModifiers, queryDecoratedElement, queryRequiredElement} from '../../common/js/dom_utils.js';
import type {FakeEntry, FilesAppDirEntry, FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {EntryList, FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import type {FilesAppState} from '../../common/js/files_app_state.js';
import {FilteredVolumeManager} from '../../common/js/filtered_volume_manager.js';
import {isDlpEnabled, isGuestOsEnabled, isMaterializedViewsEnabled, isSkyvaultV2Enabled} from '../../common/js/flags.js';
import {recordEnum, recordInterval, startInterval} from '../../common/js/metrics.js';
import {ProgressItemState} from '../../common/js/progress_center_common.js';
import {str} from '../../common/js/translations.js';
import {TrashRootEntry} from '../../common/js/trash.js';
import {debug, getLastVisitedURL, isInGuestMode, runningInBrowser} from '../../common/js/util.js';
import type {VolumeType} from '../../common/js/volume_manager_types.js';
import {AllowedPaths, ARCHIVE_OPENED_EVENT_TYPE, RootType} from '../../common/js/volume_manager_types.js';
import {DirectoryTreeContainer} from '../../containers/directory_tree_container.js';
import {NudgeType} from '../../containers/nudge_container.js';
import {getMyFiles} from '../../state/ducks/all_entries.js';
import {updateBulkPinProgress} from '../../state/ducks/bulk_pinning.js';
import {updateDeviceConnectionState} from '../../state/ducks/device.js';
import {updateDriveConnectionStatus} from '../../state/ducks/drive.js';
import {setLaunchParameters} from '../../state/ducks/launch_params.js';
import {updateMaterializedViews} from '../../state/ducks/materialized_views.js';
import {updatePreferences} from '../../state/ducks/preferences.js';
import {getDefaultSearchOptions, updateSearch} from '../../state/ducks/search.js';
import {addUiEntry, removeUiEntry} from '../../state/ducks/ui_entries.js';
import {driveRootEntryListKey, trashRootKey} from '../../state/ducks/volumes.js';
import {DialogType, SearchLocation} from '../../state/state.js';
import {getEmptyState, getEntry, getStore, getVolume} from '../../state/store.js';

import {ActionsController} from './actions_controller.js';
import {AndroidAppListModel} from './android_app_list_model.js';
import {AppStateController} from './app_state_controller.js';
import {BannerController} from './banner_controller.js';
import {CommandHandler} from './command_handler.js';
import {findQueryMatchedDirectoryEntry} from './crossover_search_utils.js';
import {CrostiniController} from './crostini_controller.js';
import {DialogActionController} from './dialog_action_controller.js';
import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {DirectoryTreeNamingController} from './directory_tree_naming_controller.js';
import {importElements} from './elements_importer.js';
import {EmptyFolderController} from './empty_folder_controller.js';
import {forceDefaultHandler} from './file_manager_commands_util.js';
import type {FileSelection} from './file_selection.js';
import {FileSelectionHandler} from './file_selection.js';
import {FileTasks} from './file_tasks.js';
import {FileTransferController} from './file_transfer_controller.js';
import {FileTypeFiltersController} from './file_type_filters_controller.js';
import {FolderShortcutsDataModel} from './folder_shortcuts_data_model.js';
import {GearMenuController} from './gear_menu_controller.js';
import {GuestOsController} from './guest_os_controller.js';
import {LastModifiedController} from './last_modified_controller.js';
import {LaunchParam} from './launch_param.js';
import {ListThumbnailLoader} from './list_thumbnail_loader.js';
import {MainWindowComponent} from './main_window_component.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {ThumbnailModel} from './metadata/thumbnail_model.js';
import {MetadataBoxController} from './metadata_box_controller.js';
import {MetadataUpdateController} from './metadata_update_controller.js';
import {NamingController} from './naming_controller.js';
import {NavigationUma} from './navigation_uma.js';
import {OneDriveController} from './one_drive_controller.js';
import {ProvidersModel} from './providers_model.js';
import {QuickViewController} from './quick_view_controller.js';
import {QuickViewModel} from './quick_view_model.js';
import {QuickViewUma} from './quick_view_uma.js';
import {ScanController} from './scan_controller.js';
import {SelectionMenuController} from './selection_menu_controller.js';
import {SortMenuController} from './sort_menu_controller.js';
import {SpinnerController} from './spinner_controller.js';
import {TaskController} from './task_controller.js';
import {ToolbarController} from './toolbar_controller.js';
import {CommandButton} from './ui/commandbutton.js';
import {contextMenuHandler} from './ui/context_menu_handler.js';
import {FileGrid} from './ui/file_grid.js';
import {FileManagerUI} from './ui/file_manager_ui.js';
import {FileMetadataFormatter} from './ui/file_metadata_formatter.js';
import {FileTable} from './ui/file_table.js';
import type {List} from './ui/list.js';
import {Menu} from './ui/menu.js';

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application.
 *
 */
export class FileManager {
  // ------------------------------------------------------------------------
  // Services FileManager depends on.

  /**
   * Volume manager.
   */
  private volumeManager_: FilteredVolumeManager|null = null;
  private crostini_: Crostini|null = null;
  private crostiniController_: CrostiniController|null = null;
  private guestOsController_: GuestOsController|null = null;
  private metadataModel_: MetadataModel|null = null;
  private fileMetadataFormatter_ = new FileMetadataFormatter();
  private thumbnailModel_: ThumbnailModel|null = null;
  /**
   * File filter.
   */
  private fileFilter_: null|FileFilter = null;

  /**
   * Model of current directory.
   */
  private directoryModel_: null|DirectoryModel = null;

  /**
   * Model of folder shortcuts.
   */
  private folderShortcutsModel_: null|FolderShortcutsDataModel = null;

  /**
   * Model of Android apps.
   */
  private androidAppListModel_: null|AndroidAppListModel = null;

  /**
   * Model for providers (providing extensions).
   */
  private providersModel_: null|ProvidersModel = null;

  /**
   * Model for quick view.
   */
  private quickViewModel_: null|QuickViewModel = null;

  /**
   * Controller for actions for current selection.
   */
  private actionsController_: ActionsController|null = null;

  /**
   * Handler for command events.
   */
  private commandHandler_: CommandHandler|null = null;

  /**
   * Handler for the change of file selection.
   */
  private selectionHandler_: null|FileSelectionHandler = null;

  /**
   * UI management class of file manager.
   */
  private ui_: null|FileManagerUI = null;

  // ------------------------------------------------------------------------
  // Parameters determining the type of file manager.

  /**
   * Dialog type of this window.
   */
  dialogType: DialogType = DialogType.FULL_PAGE;

  /**
   * Startup parameters for this application.
   */
  private launchParams_: null|LaunchParam = null;

  // ------------------------------------------------------------------------
  // Controllers.

  /**
   * File transfer controller.
   */
  private fileTransferController_: null|FileTransferController = null;

  /**
   * Naming controller.
   */
  private namingController_: null|NamingController = null;

  /**
   * Directory tree naming controller.
   */
  private directoryTreeNamingController_: DirectoryTreeNamingController|null =
      null;

  /**
   * Controller for directory scan.
   */
  protected scanController_: null|ScanController = null;

  /**
   * Controller for spinner.
   */
  private spinnerController_: null|SpinnerController = null;

  /**
   * Sort menu controller.
   */
  protected sortMenuController_: null|SortMenuController = null;

  /**
   * Gear menu controller.
   */
  protected gearMenuController_: null|GearMenuController = null;

  /**
   * Controller for the context menu opened by the action bar button in the
   * check-select mode.
   */
  protected selectionMenuController_: null|SelectionMenuController = null;

  /**
   * Toolbar controller.
   */
  private toolbarController_: null|ToolbarController = null;

  /**
   * App state controller.
   */
  private appStateController_: null|AppStateController = null;

  /**
   * Dialog action controller.
   */
  protected dialogActionController_: null|DialogActionController = null;

  /**
   * List update controller.
   */
  private metadataUpdateController_: null|MetadataUpdateController = null;

  /**
   * Last modified controller.
   */
  protected lastModifiedController_: LastModifiedController|null = null;

  /**
   * OneDrive controller.
   */
  protected oneDriveController_: OneDriveController|null = null;

  /**
   * Component for main window and its misc UI parts.
   */
  protected mainWindowComponent_: null|MainWindowComponent = null;
  private taskController_: TaskController|null = null;
  private quickViewUma_: QuickViewUma|null = null;
  protected quickViewController_: QuickViewController|null = null;
  protected fileTypeFiltersController_: FileTypeFiltersController|null = null;

  /**
   * Empty folder controller.
   */
  protected emptyFolderController_: null|EmptyFolderController = null;

  /**
   * Records histograms of directory-changed event.
   */
  private navigationUma_: null|NavigationUma = null;

  // ------------------------------------------------------------------------
  // DOM elements.

  /**
   */
  private fileBrowserBackground_: null|FileManagerBase = null;

  /**
   * The root DOM element of this app.
   */
  private dialogDom_: null|HTMLElement = null;

  /**
   * The document object of this app.
   */
  private document_: null|Document = null;

  // ------------------------------------------------------------------------
  // Miscellaneous FileManager's states.

  /**
   * Promise object which is fulfilled when initialization for app state
   * controller is done.
   */
  private initSettingsPromise_: null|Promise<void> = null;

  /**
   * Promise object which is fulfilled when initialization related to the
   * background page is done.
   */
  private initBackgroundPagePromise_: null|Promise<void> = null;

  /**
   * Whether Drive is enabled. Retrieved from user preferences.
   */
  private driveEnabled_: boolean = false;

  /**
   * Whether Drive bulk-pinning is available on this device. Retrieved from
   * user preferences.
   */
  private bulkPinningAvailable_: boolean = false;

  /**
   * Whether Drive bulk-pinning has been initialized in Files App.
   */
  private bulkPinningInitialized_: boolean = false;

  /**
   * Whether Trash is enabled or not, retrieved from user preferences.
   */
  trashEnabled: boolean = false;

  /**
   * A fake entry for Recents.
   */
  private recentEntry_: null|FakeEntry = null;

  /**
   * Whether or not we are running in guest mode.
   */
  private guestMode_: boolean = false;

  private store_ = getStore();

  /**
   * Whether local user files (e.g. My Files, Downloads, Play files...) are
   * enabled or not, retrieved from user preferences.
   */
  localUserFilesAllowed: boolean = true;

  constructor() {
    (function() {
      ColorChangeUpdater.forDocument().start();
    })();
  }

  get progressCenter(): ProgressCenter {
    assert(this.fileBrowserBackground_);
    assert(this.fileBrowserBackground_.progressCenter);
    return this.fileBrowserBackground_.progressCenter;
  }

  get directoryModel(): DirectoryModel {
    return this.directoryModel_!;
  }

  get directoryTreeNamingController(): DirectoryTreeNamingController {
    assert(this.directoryTreeNamingController_);
    return this.directoryTreeNamingController_;
  }

  get fileFilter(): FileFilter {
    assert(this.fileFilter_);
    return this.fileFilter_;
  }

  get folderShortcutsModel(): FolderShortcutsDataModel {
    assert(this.folderShortcutsModel_);
    return this.folderShortcutsModel_;
  }

  get actionsController(): ActionsController {
    assert(this.actionsController_);
    return this.actionsController_;
  }

  get commandHandler(): CommandHandler {
    assert(this.commandHandler_);
    return this.commandHandler_;
  }

  get providersModel(): ProvidersModel {
    assert(this.providersModel_);
    return this.providersModel_;
  }

  get metadataModel(): MetadataModel {
    assert(this.metadataModel_);
    return this.metadataModel_;
  }

  get selectionHandler(): FileSelectionHandler {
    assert(this.selectionHandler_);
    return this.selectionHandler_;
  }

  get document(): Document {
    assert(this.document_);
    return this.document_;
  }

  get fileTransferController(): FileTransferController|null {
    return this.fileTransferController_;
  }

  get namingController(): NamingController {
    assert(this.namingController_);
    return this.namingController_;
  }

  get taskController(): TaskController {
    assert(this.taskController_);
    return this.taskController_;
  }

  get spinnerController(): SpinnerController {
    assert(this.spinnerController_);
    return this.spinnerController_;
  }

  get volumeManager(): FilteredVolumeManager {
    assert(this.volumeManager_);
    return this.volumeManager_;
  }

  get crostini(): Crostini {
    assert(this.crostini_);
    return this.crostini_;
  }

  get ui(): FileManagerUI {
    assert(this.ui_);
    return this.ui_;
  }

  /**
   * @return If the app is running in the guest mode.
   */
  get guestMode(): boolean {
    return this.guestMode_;
  }

  /**
   * Launch a new File Manager app.
   * @param appState App state.
   */
  launchFileManager(appState?: FilesAppState) {
    assert(this.fileBrowserBackground_);
    this.fileBrowserBackground_.launchFileManager(appState);
  }

  /**
   * Returns the last URL visited with visitURL() (e.g. for "Manage in Drive").
   * Used by the integration tests.
   */
  getLastVisitedUrl(): string {
    return getLastVisitedURL();
  }

  /**
   * Returns a string translation from its translation ID.
   * @param id The id of the translated string.
   */
  getTranslatedString(id: string): string {
    return str(id);
  }

  /**
   * One time initialization for app state controller to load view option from
   * local storage.
   */
  private async startInitSettings_(): Promise<void> {
    startInterval('Load.InitSettings');
    this.appStateController_ = new AppStateController(this.dialogType);
    await this.appStateController_.loadInitialViewOptions();
    recordInterval('Load.InitSettings');
  }

  /**
   * Updates guestMode_ field based on what the result of the isInGuestMode
   * helper function. It errs on the side of not-in-guestmode, if the util
   * function fails. The worse this causes are extra notifications.
   */
  private async setGuestMode_() {
    try {
      const guest = await isInGuestMode();
      if (guest !== null) {
        this.guestMode_ = guest;
      }
    } catch (error) {
      console.warn(error);
      // Leave this.guestMode_ as its initial value.
    }
  }

  /**
   * One time initialization for the file system and related things.
   */
  private async initFileSystemUi_(): Promise<void> {
    this.ui.listContainer.startBatchUpdates();

    const fileListPromise = this.initFileList_();
    const currentDirectoryPromise = this.setupCurrentDirectory_();

    let listBeingUpdated: List|null = null;
    this.directoryModel.addEventListener('begin-update-files', () => {
      this.ui.listContainer.currentList.startBatchUpdates();
      // Remember the list which was used when updating files started, so
      // endBatchUpdates() is called on the same list.
      listBeingUpdated = this.ui.listContainer.currentList;
    });
    this.directoryModel.addEventListener('end-update-files', () => {
      this.namingController.restoreItemBeingRenamed();
      listBeingUpdated?.endBatchUpdates();
      listBeingUpdated = null;
    });
    this.volumeManager.addEventListener(ARCHIVE_OPENED_EVENT_TYPE, event => {
      assert(event.detail.mountPoint);
      if (window.isFocused?.()) {
        this.directoryModel.changeDirectoryEntry(event.detail.mountPoint);
      }
    });

    this.directoryModel.addEventListener('directory-changed', event => {
      this.navigationUma_!.onDirectoryChanged(event.detail.newDirEntry);
    });

    this.initCommands_();

    assert(this.directoryModel_);
    assert(this.spinnerController_);
    assert(this.commandHandler_);
    assert(this.selectionHandler_);
    assert(this.launchParams_);
    assert(this.volumeManager_);
    assert(this.dialogDom_);
    assert(this.fileBrowserBackground_);
    assert(this.metadataModel_);
    assert(this.providersModel_);
    assert(this.folderShortcutsModel_);
    assert(this.ui_);
    assert(this.taskController_);

    this.fileBrowserBackground_.driveSyncHandler.metadataModel =
        this.metadataModel_;
    this.scanController_ = new ScanController(
        this.directoryModel_, this.ui.listContainer, this.spinnerController_,
        this.selectionHandler_);
    this.sortMenuController_ = new SortMenuController(
        this.ui.sortButton, this.directoryModel_.getFileList());
    this.gearMenuController_ = new GearMenuController(
        this.ui.gearButton, this.ui.gearMenu, this.ui.providersMenu,
        this.directoryModel_, this.providersModel_);
    this.selectionMenuController_ = new SelectionMenuController(
        this.ui.selectionMenuButton,
        queryDecoratedElement('#file-context-menu', Menu));
    this.toolbarController_ = new ToolbarController(
        this.ui.toolbar, this.ui.dialogNavigationList, this.ui.listContainer,
        this.selectionHandler_, this.directoryModel_, this.volumeManager_,
        this.ui_);
    this.actionsController_ = new ActionsController(
        this.volumeManager_, this.metadataModel_, this.folderShortcutsModel_,
        this.selectionHandler_, this.ui_);
    this.lastModifiedController_ = new LastModifiedController(
        this.ui_.listContainer.table, this.directoryModel_);

    this.quickViewModel_ = new QuickViewModel();
    const fileListSelectionModel = this.directoryModel_.getFileListSelection();
    this.quickViewUma_ = new QuickViewUma(this.volumeManager_, this.dialogType);
    const metadataBoxController = new MetadataBoxController(
        this.metadataModel_, this.quickViewModel_, this.fileMetadataFormatter_,
        this.volumeManager_);
    this.quickViewController_ = new QuickViewController(
        this, this.metadataModel_, this.selectionHandler_,
        this.ui_.listContainer, this.ui_.selectionMenuButton,
        this.quickViewModel_, this.taskController_, fileListSelectionModel,
        this.quickViewUma_, metadataBoxController, this.dialogType,
        this.volumeManager_, this.dialogDom_);

    assert(this.fileFilter_);
    assert(this.namingController_);
    assert(this.appStateController_);
    assert(this.taskController_);
    this.mainWindowComponent_ = new MainWindowComponent(
        this.dialogType, this.ui_, this.volumeManager_, this.directoryModel_,
        this.selectionHandler_, this.namingController_,
        this.appStateController_, this.taskController_);

    this.initDataTransferOperations_();
    fileListPromise.then(() => {
      this.taskController_!.setFileTransferController(
          this.fileTransferController_!);
    });

    this.selectionHandler_.onFileSelectionChanged();
    this.ui_.listContainer.endBatchUpdates();

    const bannerController = new BannerController(
        this.directoryModel_, this.volumeManager_, this.crostini,
        this.dialogType);
    this.ui_.initBanners(bannerController);
    bannerController.initialize();

    this.ui_.attachFilesTooltip();
    this.ui_.decorateFilesMenuItems();
    this.ui_.selectionMenuButton.hidden = false;

    await Promise.all([
      fileListPromise,
      currentDirectoryPromise,
      this.setGuestMode_(),
    ]);
  }

  /**
   * Subscribes to bulk-pinning events to ensure the store is kept up to date.
   * Also tries to retrieve a first bulk pinning progress to populate the store.
   */
  private async initBulkPinning_() {
    try {
      const promise = getBulkPinProgress();

      if (!this.bulkPinningInitialized_) {
        chrome.fileManagerPrivate.onBulkPinProgress.addListener(
            (progress: chrome.fileManagerPrivate.BulkPinProgress) => {
              debug('Got bulk-pinning event:', progress);
              this.store_.dispatch(updateBulkPinProgress(progress));
            });

        this.bulkPinningInitialized_ = true;
      }

      const progress = await promise;
      if (progress) {
        debug('Got initial bulk-pinning state:', progress);
        this.store_.dispatch(updateBulkPinProgress(progress));
      }
    } catch (e) {
      console.warn('Cannot get initial bulk-pinning state:', e);
    }
  }

  private initDataTransferOperations_() {
    // CopyManager are required for 'Delete' operation in
    // Open and Save dialogs. But drag-n-drop and copy-paste are not needed.
    if (this.dialogType !== DialogType.FULL_PAGE) {
      return;
    }

    this.fileTransferController_ = new FileTransferController(
        this.document, this.ui.listContainer, this.ui.directoryTree!,
        this.ui.showConfirmationDialog.bind(this.ui), this.progressCenter,
        this.metadataModel, this.directoryModel, this.volumeManager,
        this.selectionHandler, this.ui.toast);
  }

  /**
   * One-time initialization of commands.
   */
  private initCommands_() {
    assert(this.ui_);
    assert(this.ui_.textContextMenu);
    assert(this.dialogDom_);
    assert(this.directoryTreeNamingController_);

    this.commandHandler_ = new CommandHandler(this);

    // TODO(hirono): Move the following block to the UI part.
    // Hook up the cr-button commands.
    for (const crButton of this.dialogDom_.querySelectorAll<CrButtonElement>(
             'cr-button[command]')) {
      crInjectTypeAndInit(crButton, CommandButton);
    }

    for (const input of this.getDomInputs_()) {
      this.setContextMenuForInput_(input);
    }

    this.setContextMenuForInput_(this.ui_.listContainer.renameInput);
    this.setContextMenuForInput_(
        this.directoryTreeNamingController_.getInputElement());
  }

  /**
   * Get input elements from root DOM element of this app.
   */
  private getDomInputs_() {
    return this.dialogDom_!.querySelectorAll<HTMLInputElement>(
        'input[type=text], input[type=search], textarea, cr-input');
  }

  /**
   * Set context menu and handlers for an input element.
   */
  private setContextMenuForInput_(input: HTMLInputElement) {
    let touchInduced = false;

    // stop contextmenu propagation for touch-induced events.
    input.addEventListener('touchstart', (_e) => {
      touchInduced = true;
    });
    input.addEventListener('contextmenu', (e) => {
      if (touchInduced) {
        e.stopImmediatePropagation();
      }
      touchInduced = false;
    });
    input.addEventListener('click', (_e) => {
      touchInduced = false;
    });

    contextMenuHandler.setContextMenu(input, this.ui.textContextMenu);
    this.registerInputCommands_(input);
  }

  /**
   * Registers cut, copy, paste and delete commands on input element.
   *
   * @param node Text input element to register on.
   */
  private registerInputCommands_(node: HTMLElement) {
    forceDefaultHandler(node, 'cut');
    forceDefaultHandler(node, 'copy');
    forceDefaultHandler(node, 'paste');
    forceDefaultHandler(node, 'delete');
    node.addEventListener('keydown', (e: KeyboardEvent) => {
      const key = getKeyModifiers(e) + e.keyCode;
      if (key === '190' /* '/' */ || key === '191' /* '.' */) {
        // If this key event is propagated, this is handled search command,
        // which calls 'preventDefault' method.
        e.stopPropagation();
      }
    });
  }

  /**
   * Entry point of the initialization.
   * This method is called from main.js.
   */
  initializeCore() {
    this.initGeneral_();
    this.initSettingsPromise_ = this.startInitSettings_();
    this.initBackgroundPagePromise_ =
        this.startInitBackgroundPage_().then(() => this.initVolumeManager_());

    window.addEventListener('pagehide', this.onUnload_.bind(this));
  }

  async initializeUi(dialogDom: HTMLElement): Promise<void> {
    console.warn(`Files app starting up: ${this.dialogType}`);
    this.dialogDom_ = dialogDom;
    this.document_ = this.dialogDom_.ownerDocument;

    startInterval('Load.InitDocuments');
    // importElements depend on loadTimeData which is initialized in the
    // initBackgroundPagePromise_.
    await this.initBackgroundPagePromise_;
    await importElements();
    recordInterval('Load.InitDocuments');

    startInterval('Load.InitUI');
    this.document_.documentElement.classList.add('files-ng');
    this.dialogDom_.classList.add('files-ng');

    chrome.fileManagerPrivate.isTabletModeEnabled(
        this.onTabletModeChanged_.bind(this));
    chrome.fileManagerPrivate.onTabletModeChanged.addListener(
        this.onTabletModeChanged_.bind(this));

    this.initEssentialUi_();
    // Initialize the Store for the whole app.
    this.store_.init(getEmptyState());
    this.initAdditionalUi_();
    await this.initPrefs_();
    await this.initSettingsPromise_;
    const fileSystemUIPromise = this.initFileSystemUi_();
    this.initUiFocus_();
    recordInterval('Load.InitUI');

    chrome.fileManagerPrivate.onDeviceConnectionStatusChanged.addListener(
        this.updateDeviceConnectionState_.bind(this));
    chrome.fileManagerPrivate.getDeviceConnectionState(
        this.updateDeviceConnectionState_.bind(this));

    return fileSystemUIPromise;
  }

  /**
   * Initializes general purpose basic things, which are used by other
   * initializing methods.
   */
  private initGeneral_() {
    // Initialize the application state, from the GET params.
    let json = {};
    if (location.search) {
      const query = location.search.substr(1);
      try {
        json = JSON.parse(decodeURIComponent(query));
      } catch (e) {
        debug(`Error parsing location.search "${query}" due to ${e}`);
      }
    }
    this.launchParams_ = new LaunchParam(json);
    this.store_.dispatch(
        setLaunchParameters({dialogType: this.launchParams_.type}));

    // Initialize the member variables that depend this.launchParams_.
    this.dialogType = this.launchParams_.type;
  }

  /**
   * Initializes the background page.
   */
  private async startInitBackgroundPage_(): Promise<void> {
    startInterval('Load.InitBackgroundPage');

    this.fileBrowserBackground_ = window.background;

    await this.fileBrowserBackground_.ready();

    // For the SWA, we load background and foreground in the same Window, avoid
    // loading the `data` twice.
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = this.fileBrowserBackground_.stringData;
    }
    if (runningInBrowser()) {
      this.fileBrowserBackground_.registerDialog(window);
    }
    this.crostini_ = this.fileBrowserBackground_.crostini;

    recordInterval('Load.InitBackgroundPage');
  }

  /**
   * Initializes the VolumeManager instance.
   */
  private async initVolumeManager_() {
    const allowedPaths = this.getAllowedPaths_();
    assert(this.launchParams_);
    assert(this.fileBrowserBackground_);
    const writableOnly =
        this.launchParams_.type === DialogType.SELECT_SAVEAS_FILE;
    const disabledVolumes = await this.getDisabledVolumes_();

    // FilteredVolumeManager hides virtual file system related event and data
    // even depends on the value of |supportVirtualPath|. If it is
    // VirtualPathSupport.NO_VIRTUAL_PATH, it hides Drive even if Drive is
    // enabled on preference.
    // In other words, even if Drive is disabled on preference but the Files app
    // should show Drive when it is re-enabled, then the value should be set to
    // true.
    // Note that the Drive enabling preference change is listened by
    // DriveIntegrationService, so here we don't need to take care about it.
    this.volumeManager_ = new FilteredVolumeManager(
        allowedPaths, writableOnly,
        this.fileBrowserBackground_.getVolumeManager(),
        this.launchParams_.volumeFilter, disabledVolumes);

    await this.fileBrowserBackground_.getVolumeManager();
  }

  /**
   * One time initialization of the essential UI elements in the Files app.
   * These elements will be shown to the user. Only visible elements should be
   * initialized here. Any heavy operation should be avoided. The Files app's
   * window is shown at the end of this routine.
   */
  private initEssentialUi_() {
    // Record stats of dialog types. New values must NOT be inserted into the
    // array enumerating the types. It must be in sync with
    // FileDialogType enum in tools/metrics/histograms/histogram.xml.
    const metricName = 'SWA.Create';
    recordEnum(metricName, this.dialogType, [
      DialogType.SELECT_FOLDER,
      DialogType.SELECT_UPLOAD_FOLDER,
      DialogType.SELECT_SAVEAS_FILE,
      DialogType.SELECT_OPEN_FILE,
      DialogType.SELECT_OPEN_MULTI_FILE,
      DialogType.FULL_PAGE,
    ]);

    // Create the metadata cache.
    assert(this.volumeManager_);
    this.metadataModel_ = MetadataModel.create(this.volumeManager_);
    this.thumbnailModel_ = new ThumbnailModel(this.metadataModel_);
    this.providersModel_ = new ProvidersModel(this.volumeManager_);
    this.fileFilter_ = new FileFilter(this.volumeManager_);

    // Set the files-ng class for dialog header styling.
    const dialogHeader = queryRequiredElement('.dialog-header');
    dialogHeader.classList.add('files-ng');

    // Create the root view of FileManager.
    assert(this.dialogDom_);
    assert(this.launchParams_);
    assert(this.providersModel_);
    this.ui_ = new FileManagerUI(
        this.providersModel_, this.dialogDom_, this.launchParams_);
  }

  /**
   * One-time initialization of various DOM nodes. Loads the additional DOM
   * elements visible to the user. Initialize here elements, which are expensive
   * or hidden in the beginning.
   */
  private initAdditionalUi_() {
    assert(this.metadataModel_);
    assert(this.volumeManager_);
    assert(this.dialogDom_);
    assert(this.ui_);

    // Cache nodes we'll be manipulating.
    const dom = this.dialogDom_;
    assert(dom);

    const table = queryRequiredElement('.detail-table', dom);
    FileTable.decorate(
        table, this.metadataModel_, this.volumeManager_, this.ui,
        this.dialogType === DialogType.FULL_PAGE);
    const grid = queryRequiredElement('.thumbnail-grid', dom);
    FileGrid.decorate(grid, this.metadataModel_, this.volumeManager_, this.ui);

    assertInstanceof(table, FileTable);
    assertInstanceof(grid, FileGrid);
    this.ui_.initAdditionalUI(table, grid, this.volumeManager_);

    // Handle UI events.
    this.progressCenter.addPanel(this.ui_.progressCenterPanel);

    // Arrange the file list.
    this.ui_.listContainer.table.normalizeColumns();
    this.ui_.listContainer.table.redraw();
  }

  /**
   * Initializes the prefs in the store.
   */
  private async initPrefs_():
      Promise<chrome.fileManagerPrivate.Preferences|null> {
    let prefs = null;
    try {
      prefs = await getPreferences();
    } catch (e) {
      console.error('Cannot get preferences:', e);
      return null;
    }

    this.store_.dispatch(updatePreferences(prefs));
    return prefs;
  }

  /**
   * One-time initialization of focus. This should run at the last of UI
   * initialization.
   */
  private initUiFocus_() {
    this.ui_?.initUIFocus();
  }

  /**
   * Constructs table and grid (heavy operation).
   */
  private async initFileList_(): Promise<void> {
    const singleSelection = this.dialogType === DialogType.SELECT_OPEN_FILE ||
        this.dialogType === DialogType.SELECT_FOLDER ||
        this.dialogType === DialogType.SELECT_UPLOAD_FOLDER ||
        this.dialogType === DialogType.SELECT_SAVEAS_FILE;

    assert(this.volumeManager_);
    assert(this.metadataModel_);
    assert(this.fileFilter_);
    assert(this.launchParams_);
    assert(this.ui_);
    assert(this.thumbnailModel_);
    assert(this.appStateController_);
    assert(this.crostini_);
    assert(this.providersModel_);

    this.directoryModel_ = new DirectoryModel(
        singleSelection, this.fileFilter_, this.metadataModel_,
        this.volumeManager_);
    assert(this.directoryModel_);

    this.folderShortcutsModel_ =
        new FolderShortcutsDataModel(this.volumeManager_);

    this.androidAppListModel_ = new AndroidAppListModel(
        this.launchParams_.showAndroidPickerApps,
        this.launchParams_.includeAllFiles, this.launchParams_.typeList);

    this.recentEntry_ = new FakeEntryImpl(
        str('RECENT_ROOT_LABEL'), RootType.RECENT, this.getSourceRestriction_(),
        chrome.fileManagerPrivate.FileCategory.ALL);
    this.store_.dispatch(addUiEntry(this.recentEntry_));
    assert(this.launchParams_);
    this.selectionHandler_ = new FileSelectionHandler(
        this.directoryModel_, this.ui_.listContainer, this.metadataModel_,
        this.volumeManager_, this.launchParams_.allowedPaths);

    // TODO(mtomasz, yoshiki): Create navigation list earlier, and here just
    // attach the directory model.
    const directoryTreePromise = this.initDirectoryTree_();

    this.ui_.listContainer.listThumbnailLoader = new ListThumbnailLoader(
        this.directoryModel_, this.thumbnailModel_, this.volumeManager_);
    this.ui_.listContainer.dataModel = this.directoryModel_.getFileList();
    this.ui_.listContainer.emptyDataModel =
        this.directoryModel_.getEmptyFileList();
    this.ui_.listContainer.selectionModel =
        this.directoryModel_.getFileListSelection();

    this.appStateController_.initialize(this.ui_, this.directoryModel_);

    // Create metadata update controller.
    this.metadataUpdateController_ = new MetadataUpdateController(
        this.ui_.listContainer, this.directoryModel_, this.metadataModel_,
        this.fileMetadataFormatter_);

    // Create naming controller.
    this.namingController_ = new NamingController(
        this.ui_.listContainer, this.ui_.alertDialog, this.ui_.confirmDialog,
        this.directoryModel_, this.fileFilter_, this.selectionHandler_);

    // Create task controller.
    this.taskController_ = new TaskController(
        this.volumeManager_, this.ui_, this.metadataModel_,
        this.directoryModel_, this.selectionHandler_,
        this.metadataUpdateController_, this.crostini_, this.progressCenter);

    // Create directory tree naming controller.
    this.directoryTreeNamingController_ = new DirectoryTreeNamingController(
        this.directoryModel_, this.ui_.directoryTree,
        this.ui_.directoryTreeContainer, this.ui_.alertDialog);

    // Create spinner controller.
    this.spinnerController_ =
        new SpinnerController(this.ui_.listContainer.spinner);
    this.spinnerController_.blink();

    // Create dialog action controller.
    this.dialogActionController_ = new DialogActionController(
        this.dialogType, this.ui_.dialogFooter, this.directoryModel_,
        this.volumeManager_, this.fileFilter_, this.namingController_,
        this.selectionHandler_, this.launchParams_);

    // Create file-type filter controller.
    this.fileTypeFiltersController_ = new FileTypeFiltersController(
        this.ui_.fileTypeFilterContainer, this.directoryModel_,
        this.recentEntry_, this.ui);
    this.emptyFolderController_ = new EmptyFolderController(
        this.ui_.emptyFolder, this.directoryModel_, this.providersModel_,
        this.recentEntry_);


    return directoryTreePromise;
  }

  /**
   * Based on the dialog type and dialog caller, sets the list of volumes
   * that should be disabled according to Data Leak Prevention rules.
   */
  private async getDisabledVolumes_(): Promise<VolumeType[]> {
    if (this.dialogType !== DialogType.SELECT_SAVEAS_FILE || !isDlpEnabled()) {
      return [];
    }
    const caller = await getDialogCaller();
    if (!caller.url) {
      return [];
    }
    const dlpBlockedComponents = await getDlpBlockedComponents(caller.url);
    const disabledVolumes = [];
    for (const c of dlpBlockedComponents) {
      disabledVolumes.push(c);
    }
    return disabledVolumes as VolumeType[];
  }

  private async initDirectoryTree_(): Promise<void> {
    this.navigationUma_ = new NavigationUma(this.volumeManager);

    assert(this.dialogDom_);
    assert(this.directoryModel_);
    assert(this.ui_);
    assert(this.volumeManager_);
    assert(this.directoryModel_);
    assert(this.metadataModel_);
    assert(this.folderShortcutsModel_);
    assert(this.launchParams_);
    assert(this.recentEntry_);
    assert(this.androidAppListModel_);
    assert(this.crostini_);

    const treeContainerDiv = this.dialogDom_.querySelector<HTMLDivElement>(
        '.dialog-navigation-list-contents');
    assert(treeContainerDiv);

    const directoryTreeContainer =
        new DirectoryTreeContainer(treeContainerDiv, this.directoryModel_);
    this.ui_.initDirectoryTree(directoryTreeContainer);

    // If 'media-store-files-only' volume filter is enabled, then Android ARC
    // SelectFile opened files app to pick files from volumes that are indexed
    // by the Android MediaStore. Never add Drive, Crostini, GuestOS, to the
    // directory tree in that case: their volume content is not indexed by the
    // Android MediaStore, and being indexed there is needed for this Android
    // ARC SelectFile MediaStore filter mode to work: crbug.com/1333385
    if (this.volumeManager_.getMediaStoreFilesOnlyFilterEnabled()) {
      return;
    }

    // Drive add/removes itself from directory tree in onPreferencesChanged_.
    // Setup a prefs change listener then call onPreferencesChanged_() to add
    // Drive to the directory tree if Drive is enabled by prefs.
    chrome.fileManagerPrivate.onPreferencesChanged.addListener(() => {
      this.onPreferencesChanged_();
    });
    this.onPreferencesChanged_();

    chrome.fileManagerPrivate.onDriveConnectionStatusChanged.addListener(() => {
      this.onDriveConnectionStatusChanged_();
    });
    this.onDriveConnectionStatusChanged_();

    // The fmp.onCrostiniChanged receives enabled/disabled events via a pref
    // watcher and share/unshare events.  The enabled/disabled prefs are
    // handled in fmp.onCrostiniChanged rather than fmp.onPreferencesChanged
    // to keep crostini logic colocated, and to have an API that best supports
    // multiple VMs.
    chrome.fileManagerPrivate.onCrostiniChanged.addListener(
        this.onCrostiniChanged_.bind(this));
    this.crostiniController_ = new CrostiniController(this.crostini_);
    await this.crostiniController_.redraw();
    // Never show toast in an open-file dialog.
    const maybeShowToast = this.dialogType === DialogType.FULL_PAGE;
    await this.crostiniController_.loadSharedPaths(
        maybeShowToast, this.ui_.toast);

    if (isGuestOsEnabled()) {
      this.guestOsController_ = new GuestOsController();
      await this.guestOsController_.refresh();
    }
  }

  /**
   * Listens for the enable and disable events in order to add or remove the
   * directory tree 'Linux files' root item.
   *
   */
  private async onCrostiniChanged_(
      event: chrome.fileManagerPrivate.CrostiniEvent): Promise<void> {
    assert(this.crostini_);
    assert(this.crostiniController_);
    assert(this.ui_);
    assert(this.volumeManager_);
    assert(this.metadataModel_);
    assert(this.directoryModel_);
    // The background |this.crostini_| object also listens to all crostini
    // events including enable/disable, and share/unshare.
    // But to ensure we don't have any race conditions between bg and fg, we
    // set enabled status on it before calling |setupCrostini_| which reads
    // enabled status from it to determine whether 'Linux files' is shown.
    switch (event.eventType) {
      case chrome.fileManagerPrivate.CrostiniEventType.ENABLE:
        this.crostini_.setEnabled(event.vmName, event.containerName, true);
        return this.crostiniController_.redraw();

      case chrome.fileManagerPrivate.CrostiniEventType.DISABLE:
        this.crostini_.setEnabled(event.vmName, event.containerName, false);
        return this.crostiniController_.redraw();

      // Event is sent when a user drops an unshared file on Plugin VM.
      // We show the move dialog so the user can move the file or share the
      // directory.
      case chrome.fileManagerPrivate.CrostiniEventType
          .DROP_FAILED_PLUGIN_VM_DIRECTORY_NOT_SHARED:
        if (this.ui_.dragInProcess) {
          const moveMessage =
              str('UNABLE_TO_DROP_IN_PLUGIN_VM_DIRECTORY_NOT_SHARED_MESSAGE');
          const copyMessage =
              str('UNABLE_TO_DROP_IN_PLUGIN_VM_EXTERNAL_DRIVE_MESSAGE');
          FileTasks.showPluginVmNotSharedDialog(
              this.selectionHandler.selection.entries, this.volumeManager_,
              this.metadataModel_, this.ui_, moveMessage, copyMessage,
              this.fileTransferController_, this.directoryModel_);
        }
        break;
    }
  }

  private async initMaterializedViews_() {
    const views = await getMaterializedViews();
    this.store_.dispatch(updateMaterializedViews({materializedViews: views}));
  }

  /**
   * Sets up the current directory during initialization.
   */
  private async setupCurrentDirectory_(): Promise<void> {
    assert(this.launchParams_);
    assert(this.recentEntry_);
    assert(this.ui_);
    assert(this.volumeManager_);
    assert(this.metadataModel_);
    assert(this.directoryModel_);

    if (isSkyvaultV2Enabled()) {
      this.oneDriveController_ = new OneDriveController();
    }
    const initMaterializedViewsPromise = isMaterializedViewsEnabled() ?
        this.initMaterializedViews_() :
        Promise.resolve();
    const tracker = this.directoryModel_.createDirectoryChangeTracker();
    tracker.start();

    // Wait until the volume manager is initialized.
    await new Promise<void>(
        resolve => this.volumeManager.ensureInitialized(resolve));

    let nextCurrentDirEntry: Entry|FilesAppEntry|null = null;
    let selectionEntry: Entry|null = null;

    // Resolve the selectionURL to selectionEntry or to currentDirectoryEntry in
    // case of being a display root or a default directory to open files.
    if (this.launchParams_.selectionURL) {
      if (this.launchParams_.selectionURL === this.recentEntry_.toURL()) {
        nextCurrentDirEntry = this.recentEntry_;
      } else {
        try {
          const inEntry = await new Promise<Entry>((resolve, reject) => {
            window.webkitResolveLocalFileSystemURL(
                this.launchParams_!.selectionURL, resolve, reject);
          });
          const locationInfo = this.volumeManager_.getLocationInfo(inEntry);
          // If location information is not available, then the volume is no
          // longer (or never) available.
          if (locationInfo) {
            // If the selection is root, use it as a current directory instead.
            // This is because selecting a root is the same as opening it.
            if (locationInfo.isRootEntry) {
              nextCurrentDirEntry = inEntry;
            }

            // If the |selectionURL| is a directory make it the current
            // directory.
            if (inEntry.isDirectory) {
              nextCurrentDirEntry = inEntry;
            }

            // By default, the selection should be selected entry and the parent
            // directory of it should be the current directory.
            if (!nextCurrentDirEntry) {
              selectionEntry = inEntry;
            }
          }
        } catch (error: any) {
          // If `selectionURL` doesn't exist we just don't select it, thus we
          // don't need to log the failure.
          if (error.name !== 'NotFoundError') {
            console.warn(error.stack || error);
          }
        }
      }
    }

    // If searchQuery param is set, find the first directory that matches the
    // query, and select it if exists.
    const searchQuery = this.launchParams_.searchQuery;
    if (searchQuery) {
      startInterval('Load.ProcessInitialSearchQuery');
      assert(this.spinnerController_);
      // Show a spinner, as the crossover search function call could be slow.
      const hideSpinnerCallback = this.spinnerController_.show();
      const queryMatchedDirEntry = await findQueryMatchedDirectoryEntry(
          this.volumeManager_, this.directoryModel_, searchQuery);
      if (queryMatchedDirEntry) {
        nextCurrentDirEntry = queryMatchedDirEntry;
      }
      hideSpinnerCallback();
      recordInterval('Load.ProcessInitialSearchQuery');
    }

    // Resolve the currentDirectoryURL to currentDirectoryEntry (if not done by
    // the previous step).
    if (!nextCurrentDirEntry && this.launchParams_.currentDirectoryURL) {
      try {
        const inEntry = await new Promise<Entry>((resolve, reject) => {
          window.webkitResolveLocalFileSystemURL(
              this.launchParams_!.currentDirectoryURL, resolve, reject);
        });
        const locationInfo = this.volumeManager_.getLocationInfo(inEntry);
        if (locationInfo) {
          nextCurrentDirEntry = inEntry;
        }
      } catch (error: any) {
        console.warn(error.stack || error);
      }
    }

    // If the directory to be changed to is not available, then first fallback
    // to the parent of the selection entry.
    if (!nextCurrentDirEntry && selectionEntry) {
      nextCurrentDirEntry = await new Promise(resolve => {
        selectionEntry!.getParent(resolve);
      });
    }

    // Check if the next current directory is not a virtual directory which is
    // not available in UI. This may happen to shared on Drive.
    if (nextCurrentDirEntry) {
      const locationInfo =
          this.volumeManager_.getLocationInfo(nextCurrentDirEntry);
      // If we can't check, assume that the directory is illegal.
      if (!locationInfo) {
        nextCurrentDirEntry = null;
      } else {
        // Having root directory of DRIVE_SHARED_WITH_ME here should be only for
        // shared with me files. Fallback to Drive root in such case.
        if (locationInfo.isRootEntry &&
            locationInfo.rootType === RootType.DRIVE_SHARED_WITH_ME) {
          const volumeInfo =
              this.volumeManager_.getVolumeInfo(nextCurrentDirEntry);
          if (!volumeInfo) {
            nextCurrentDirEntry = null;
          } else {
            try {
              nextCurrentDirEntry = await volumeInfo.resolveDisplayRoot();
            } catch (error: any) {
              console.warn(error.stack || error);
              nextCurrentDirEntry = null;
            }
          }
        }
      }
    }

    // If the resolved directory to be changed is blocked by DLP, we should
    // fallback to the default display root.
    if (nextCurrentDirEntry && isDlpEnabled()) {
      const volumeInfo = this.volumeManager_.getVolumeInfo(nextCurrentDirEntry);
      if (volumeInfo && this.volumeManager_.isDisabled(volumeInfo.volumeType)) {
        console.warn('Target directory is DLP blocked, redirecting to MyFiles');
        nextCurrentDirEntry = null;
      }
    }

    // If the directory to be changed to is still not resolved, then fallback to
    // the default display root.
    if (!nextCurrentDirEntry) {
      nextCurrentDirEntry = await this.volumeManager_.getDefaultDisplayRoot();
    }

    // If selection failed to be resolved (eg. didn't exist, in case of saving a
    // file, or in case of a fallback of the current directory, then try to
    // resolve again using the target name.
    if (!selectionEntry && nextCurrentDirEntry &&
        this.launchParams_.targetName) {
      // Try to resolve as a file first. If it fails, then as a directory.
      try {
        selectionEntry = await new Promise((resolve, reject) => {
          (nextCurrentDirEntry as DirectoryEntry)
              .getFile(this.launchParams_!.targetName, {}, resolve, reject);
        });
      } catch (error1: any) {
        // Failed to resolve as a file. Try to resolve as a directory.
        try {
          selectionEntry = await new Promise((resolve, reject) => {
            (nextCurrentDirEntry as DirectoryEntry)
                .getDirectory(
                    this.launchParams_!.targetName, {}, resolve, reject);
          });
        } catch (error2: any) {
          // If `targetName` doesn't exist we just don't select it, thus we
          // don't need to log the failure.
          if (error1.name !== 'NotFoundError') {
            console.warn(error1.stack || error1);
            console.info(error1);
          }
          if (error2.name !== 'NotFoundError') {
            console.warn(error2.stack || error2);
          }
        }
      }
    }

    // If there is no target select MyFiles by default, but only if local files
    // are enabled.
    if (!nextCurrentDirEntry && this.localUserFilesAllowed) {
      assert(this.ui.directoryTree);
      const myFiles = getMyFiles(this.store_.getState());
      // When MyFiles volume is mounted, we rely on the current directory
      // change to make it as selected (controlled by DirectoryModel),
      // that's why we can't set MyFiles entry list as the current directory
      // here.
      // TODO(b/308504417): MyFiles entry list should be selected before
      // MyFiles volume mounts.
      if (myFiles && myFiles.myFilesVolume) {
        nextCurrentDirEntry = myFiles.myFilesEntry;
      }
    }

    // The next directory might be a materialized view.
    await initMaterializedViewsPromise;

    // TODO(b/328031885): Handle !nextCurrentDirEntry case here - it means some
    // error occurred and we should show the appropriate UI.

    // Check directory change.
    if (!tracker.hasChanged) {
      // Finish setup current directory.
      await this.finishSetupCurrentDirectory_(
          (nextCurrentDirEntry as DirectoryEntry), selectionEntry,
          this.launchParams_.targetName);
    }
    // Only stop the tracker after finishing the directory change.
    tracker.stop();
  }

  /**
   * @param directoryEntry Directory to be opened.
   * @param selectionEntry Entry to be selected.
   * @param suggestedName Suggested name for a non-existing selection.
   */
  private async finishSetupCurrentDirectory_(
      directoryEntry: null|DirectoryEntry, selectionEntry?: Entry|null,
      suggestedName?: string): Promise<void> {
    // Open the directory, and select the selection (if passed).
    const promise = (async () => {
      console.warn('Files app has started');
      if (directoryEntry) {
        await new Promise(resolve => {
          this.directoryModel.changeDirectoryEntry(directoryEntry, resolve);
        });
        if (selectionEntry) {
          this.directoryModel.selectEntry(selectionEntry);
        }
        if (this.launchParams_?.searchQuery) {
          const searchState = this.store_.getState().search;
          this.store_.dispatch(updateSearch({
            query: this.launchParams_.searchQuery,
            status: undefined,
            // Make sure the current directory can be highlighted in the
            // directory tree.
            options: {
              ...getDefaultSearchOptions(),
              ...searchState?.options,
              location: SearchLocation.THIS_FOLDER,
            },
          }));
        }
      } else {
        console.warn('No entry for finishSetupCurrentDirectory_');
      }
      this.ui.addLoadedAttribute();
    })();

    if (this.dialogType === DialogType.SELECT_SAVEAS_FILE) {
      this.ui.dialogFooter.filenameInput.value = suggestedName || '';
      this.ui.dialogFooter.selectTargetNameInFilenameInput();
    }

    return promise;
  }

  /**
   * Returns DirectoryEntry of the current directory.  Returns null if the
   * directory model is not ready or the current directory is not set.
   */
  getCurrentDirectoryEntry(): DirectoryEntry|FakeEntry|FilesAppDirEntry|null
      |undefined {
    return this.directoryModel_ && this.directoryModel_.getCurrentDirEntry();
  }

  /** Expose the unload method for integration tests. */
  onUnloadForTest() {
    this.onUnload_();
  }

  /**
   * Unload handler for the page.
   */
  private onUnload_() {
    if (this.directoryModel_) {
      this.directoryModel_.dispose();
    }

    if (this.volumeManager_) {
      this.volumeManager_.dispose();
    }

    if (this.fileTransferController_) {
      for (const taskId of this.fileTransferController_.pendingTaskIds) {
        const item = this.progressCenter.getItemById(taskId)!;
        item.message = '';
        item.state = ProgressItemState.CANCELED;
        this.progressCenter.updateItem(item);
      }
    }

    if (this.ui_ && this.ui_.progressCenterPanel) {
      this.progressCenter.removePanel(this.ui_.progressCenterPanel);
    }
  }

  /**
   * Returns allowed path for the dialog by considering:
   * 1) The launch parameter which specifies generic category of valid files
   * paths.
   * 2) Files app's unique capabilities and restrictions.
   */
  private getAllowedPaths_(): AllowedPaths {
    assert(this.launchParams_);
    let allowedPaths = this.launchParams_.allowedPaths;
    // The native implementation of the Files app creates snapshot files for
    // non-native files. But it does not work for folders (e.g., dialog for
    // loading unpacked extensions).
    if (allowedPaths === AllowedPaths.NATIVE_PATH &&
        !isFolderDialogType(this.launchParams_.type)) {
      if (this.launchParams_.type === DialogType.SELECT_SAVEAS_FILE) {
        allowedPaths = AllowedPaths.NATIVE_PATH;
      } else {
        allowedPaths = AllowedPaths.ANY_PATH;
      }
    }

    return allowedPaths;
  }

  /**
   * Returns SourceRestriction which is used to communicate restrictions about
   * sources to chrome.fileManagerPrivate.getRecentFiles API.
   */
  private getSourceRestriction_(): chrome.fileManagerPrivate.SourceRestriction {
    const allowedPaths = this.getAllowedPaths_();
    if (allowedPaths === AllowedPaths.NATIVE_PATH) {
      return chrome.fileManagerPrivate.SourceRestriction.NATIVE_SOURCE;
    }
    return chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE;
  }

  /**
   * @return Selection object.
   */
  getSelection(): FileSelection {
    return this.selectionHandler.selection;
  }

  /**
   * @return File list.
   */
  getFileList(): ArrayDataModel {
    return this.directoryModel.getFileList();
  }

  /**
   * @return Current list object.
   */
  getCurrentList(): List {
    return this.ui.listContainer.currentList;
  }

  /**
   * Add or remove the fake Drive and Trash item from the directory tree when
   * the prefs change. If Drive or Trash has been enabled by prefs, add the item
   * otherwise remove it. This supports dynamic refresh when the pref changes.
   */
  private async onPreferencesChanged_() {
    const prefs = await this.initPrefs_();
    if (!prefs) {
      return;
    }

    if (this.driveEnabled_ !== prefs.driveEnabled) {
      this.driveEnabled_ = prefs.driveEnabled;
      this.toggleDriveRootOnPreferencesUpdate_();
    }

    if (this.bulkPinningAvailable_ !== prefs.driveFsBulkPinningAvailable) {
      this.bulkPinningAvailable_ = prefs.driveFsBulkPinningAvailable;
      debug(`Bulk-pinning is now ${
          this.bulkPinningAvailable_ ? 'available' : 'unavailable'}`);
      if (this.bulkPinningAvailable_) {
        await this.initBulkPinning_();
      }
    }

    assert(this.toolbarController_);
    if (this.trashEnabled !== prefs.trashEnabled) {
      this.trashEnabled = prefs.trashEnabled;
      this.toggleTrashRootOnPreferencesUpdate_();
      this.toolbarController_.moveToTrashCommand.disabled = !this.trashEnabled;
      this.toolbarController_.moveToTrashCommand.canExecuteChange(
          this.ui.listContainer.currentList);
    }

    if (this.localUserFilesAllowed !== prefs.localUserFilesAllowed) {
      this.localUserFilesAllowed = prefs.localUserFilesAllowed;
      // Trigger the change after prefs are updated, so that if needed, the
      // default root can be resolved correctly.
      await this.maybeChangeRootOnPreferencesUpdate_();
    }

    await this.updateOfficePrefs_(prefs);

    assert(this.ui.directoryTree);
  }

  private async onDriveConnectionStatusChanged_() {
    let connectionState = null;
    try {
      connectionState = await getDriveConnectionState();
    } catch (e) {
      console.error('Failed to retrieve drive connection state:', e);
      return;
    }
    this.store_.dispatch(updateDriveConnectionStatus(connectionState));
  }

  private async updateOfficePrefs_(prefs:
                                       chrome.fileManagerPrivate.Preferences) {
    assert(this.ui_);
    // These prefs starts with value 0. We only want to display when they're
    // non-zero and show the most recent (larger value).
    if (prefs.officeFileMovedOneDrive > prefs.officeFileMovedGoogleDrive) {
      this.ui_.nudgeContainer.showNudge(
          NudgeType['ONE_DRIVE_MOVED_FILE_NUDGE']);
    } else if (
        prefs.officeFileMovedOneDrive < prefs.officeFileMovedGoogleDrive) {
      this.ui_.nudgeContainer.showNudge(NudgeType['DRIVE_MOVED_FILE_NUDGE']);
    }
    // Reset the seen state for office nudge. For normal users these 2 prefs
    // will never reset to 0, however for manual tests it can be reset in
    // chrome://files-internals.
    if (prefs.officeFileMovedOneDrive === 0 &&
        await this.ui_.nudgeContainer.checkSeen(
            NudgeType['ONE_DRIVE_MOVED_FILE_NUDGE'])) {
      this.ui_.nudgeContainer.clearSeen(
          NudgeType['ONE_DRIVE_MOVED_FILE_NUDGE']);
      debug('Reset OneDrive move to cloud nudge');
    }
    if (prefs.officeFileMovedGoogleDrive === 0 &&
        await this.ui_.nudgeContainer.checkSeen(
            NudgeType['DRIVE_MOVED_FILE_NUDGE'])) {
      this.ui_.nudgeContainer.clearSeen(NudgeType['DRIVE_MOVED_FILE_NUDGE']);
      debug('Reset Google Drive move to cloud nudge');
    }
  }

  /**
   * Invoked when the device connection status changes.
   */
  private updateDeviceConnectionState_(
      state: chrome.fileManagerPrivate.DeviceConnectionState) {
    this.store_.dispatch(updateDeviceConnectionState({connection: state}));
  }

  /**
   * Toggles the trash root visibility when the `trashEnabled` preference is
   * updated.
   */
  private toggleTrashRootOnPreferencesUpdate_() {
    assert(this.ui_);
    let trashRoot: TrashRootEntry|null =
        getEntry(this.store_.getState(), trashRootKey) as TrashRootEntry | null;
    if (this.trashEnabled) {
      if (!trashRoot) {
        trashRoot = new TrashRootEntry();
      }
      this.store_.dispatch(addUiEntry(trashRoot));
      assert(this.ui_.directoryTree);
      return;
    }

    this.store_.dispatch(removeUiEntry(trashRootKey));
    assert(this.ui_.directoryTree);
    this.navigateAwayFromDisabledRoot_(trashRoot || null);
  }

  /**
   * Toggles the drive root visibility when the `driveEnabled` preference is
   * updated.
   */
  private toggleDriveRootOnPreferencesUpdate_() {
    let driveFakeRoot: EntryList|FakeEntry|null =
        getEntry(this.store_.getState(), driveRootEntryListKey) as EntryList |
        null;
    if (this.driveEnabled_) {
      if (!driveFakeRoot) {
        driveFakeRoot = new EntryList(
            str('DRIVE_DIRECTORY_LABEL'), RootType.DRIVE_FAKE_ROOT);
        this.store_.dispatch(addUiEntry(driveFakeRoot));
      }
      assert(this.ui.directoryTree);
      return;
    }
    this.store_.dispatch(removeUiEntry(driveRootEntryListKey));
    assert(this.ui.directoryTree);
    this.navigateAwayFromDisabledRoot_(driveFakeRoot);
  }

  /**
   * Navigates to default display root if currently in a local folder and
   * `localUserFilesAllowed` preference is updated to False.
   */
  private async maybeChangeRootOnPreferencesUpdate_() {
    if (this.localUserFilesAllowed) {
      return;
    }
    assert(this.directoryModel_);
    assert(this.volumeManager_);

    const fileData = this.directoryModel_.getCurrentFileData();
    if (!fileData) {
      return;
    }

    const tracker = this.directoryModel_.createDirectoryChangeTracker();
    tracker.start();

    const state = this.store_.getState();
    const volume = getVolume(state, fileData);
    // The current directory is pointing to an entry that has a volume,
    // but the volume isn't mounted anymore.
    if (fileData.volumeId && !volume) {
      const displayRoot = await this.volumeManager_.getDefaultDisplayRoot();
      if (displayRoot && !tracker.hasChanged) {
        this.directoryModel_!.changeDirectoryEntry(displayRoot);
      }
    }
    tracker.stop();
  }

  /**
   * If the root item has been disabled but it is the current visible entry,
   * navigate away from it to the default display root.
   * @param entry The entry to navigate away from.
   */
  private navigateAwayFromDisabledRoot_(entry: null|Entry|FilesAppEntry) {
    if (!entry) {
      return;
    }

    assert(this.directoryModel_);
    assert(this.volumeManager_);
    // The fake root item is being hidden so navigate away if it's the
    // current directory.
    if (this.directoryModel_.getCurrentDirEntry() === entry) {
      this.volumeManager_.getDefaultDisplayRoot().then((displayRoot) => {
        if (this.directoryModel_!.getCurrentDirEntry() === entry &&
            displayRoot) {
          this.directoryModel_!.changeDirectoryEntry(displayRoot);
        }
      });
    }
  }

  /**
   * Updates the DOM to reflect the specified tablet mode `enabled` state.
   */
  private onTabletModeChanged_(enabled: boolean) {
    this.dialogDom_!.classList.toggle('tablet-mode-enabled', enabled);
  }
}
