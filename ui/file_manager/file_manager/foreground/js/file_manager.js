// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {startColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';

import {getDialogCaller, getDlpBlockedComponents, getPreferences} from '../../common/js/api.js';
import {ArrayDataModel} from '../../common/js/array_data_model.js';
import {DialogType, isFolderDialogType} from '../../common/js/dialog_type.js';
import {getKeyModifiers, queryDecoratedElement, queryRequiredElement} from '../../common/js/dom_utils.js';
import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {FilesAppState} from '../../common/js/files_app_state.js';
import {FilteredVolumeManager} from '../../common/js/filtered_volume_manager.js';
import {metrics} from '../../common/js/metrics.js';
import {ProgressItemState} from '../../common/js/progress_center_common.js';
import {TrashRootEntry} from '../../common/js/trash.js';
import {str, util} from '../../common/js/util.js';
import {AllowedPaths, VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {NudgeType} from '../../containers/nudge_container.js';
import {Crostini} from '../../externs/background/crostini.js';
import {FileManagerBaseInterface} from '../../externs/background/file_manager_base.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {CommandHandlerDeps} from '../../externs/command_handler_deps.js';
import {FakeEntry, FilesAppDirEntry} from '../../externs/files_app_entry_interfaces.js';
import {ForegroundWindow} from '../../externs/foreground_window.js';
import {PropStatus} from '../../externs/ts/state.js';
import {Store} from '../../externs/ts/store.js';
import {updateSearch} from '../../state/actions.js';
import {addUiEntry, removeUiEntry} from '../../state/actions/ui_entries.js';
import {trashRootKey} from '../../state/reducers/volumes.js';
import {getEmptyState, getStore} from '../../state/store.js';

import {ActionsController} from './actions_controller.js';
import {AndroidAppListModel} from './android_app_list_model.js';
import {AppStateController} from './app_state_controller.js';
import {BannerController} from './banner_controller.js';
import {crossoverSearchUtils} from './crossover_search_utils.js';
import {CrostiniController} from './crostini_controller.js';
import {DialogActionController} from './dialog_action_controller.js';
import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {DirectoryTreeNamingController} from './directory_tree_naming_controller.js';
import {DriveDialogController} from './drive_dialog_controller.js';
import {importElements} from './elements_importer.js';
import {EmptyFolderController} from './empty_folder_controller.js';
import {CommandHandler, CommandUtil} from './file_manager_commands.js';
import {FileSelection, FileSelectionHandler} from './file_selection.js';
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
import {NavigationListModel, NavigationModelFakeItem, NavigationModelItemType} from './navigation_list_model.js';
import {NavigationUma} from './navigation_uma.js';
import {ProvidersModel} from './providers_model.js';
import {QuickViewController} from './quick_view_controller.js';
import {QuickViewModel} from './quick_view_model.js';
import {QuickViewUma} from './quick_view_uma.js';
import {ScanController} from './scan_controller.js';
import {SearchController} from './search_controller.js';
import {SelectionMenuController} from './selection_menu_controller.js';
import {SortMenuController} from './sort_menu_controller.js';
import {SpinnerController} from './spinner_controller.js';
import {TaskController} from './task_controller.js';
import {ToolbarController} from './toolbar_controller.js';
import {A11yAnnounce} from './ui/a11y_announce.js';
import {CommandButton} from './ui/commandbutton.js';
import {contextMenuHandler} from './ui/context_menu_handler.js';
import {DirectoryTree} from './ui/directory_tree.js';
import {FileGrid} from './ui/file_grid.js';
import {FileListSelectionModel} from './ui/file_list_selection_model.js';
import {FileManagerUI} from './ui/file_manager_ui.js';
import {FileMetadataFormatter} from './ui/file_metadata_formatter.js';
import {FileTable} from './ui/file_table.js';
import {List} from './ui/list.js';
import {Menu} from './ui/menu.js';

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application.
 *
 * @implements {CommandHandlerDeps}
 */
export class FileManager extends EventTarget {
  constructor() {
    super();

    // ------------------------------------------------------------------------
    // Services FileManager depends on.

    /**
     * Volume manager.
     * @private {!FilteredVolumeManager}
     */
    this.volumeManager_;

    /** @private {?Crostini} */
    this.crostini_ = null;

    /** @private {?CrostiniController} */
    this.crostiniController_ = null;

    /** @private {?GuestOsController} */
    this.guestOsController_ = null;

    /** @private {?MetadataModel} */
    this.metadataModel_ = null;

    /** @private @const {!FileMetadataFormatter} */
    this.fileMetadataFormatter_ = new FileMetadataFormatter();

    /** @private {?ThumbnailModel} */
    this.thumbnailModel_ = null;

    /**
     * File operation manager.
     * @private {?FileOperationManager}
     */
    this.fileOperationManager_ = null;

    /**
     * File filter.
     * @private {?FileFilter}
     */
    this.fileFilter_ = null;

    /**
     * Model of current directory.
     * @private {?DirectoryModel}
     */
    this.directoryModel_ = null;

    /**
     * Model of folder shortcuts.
     * @private {?FolderShortcutsDataModel}
     */
    this.folderShortcutsModel_ = null;

    /**
     * Model of Android apps.
     * @private {?AndroidAppListModel}
     */
    this.androidAppListModel_ = null;

    /**
     * Model for providers (providing extensions).
     * @private {?ProvidersModel}
     */
    this.providersModel_ = null;

    /**
     * Model for quick view.
     * @private {?QuickViewModel}
     */
    this.quickViewModel_ = null;

    /**
     * Controller for actions for current selection.
     * @private {ActionsController}
     */
    this.actionsController_ = null;

    /**
     * Controller for showing dialogs from Drive.
     * @private {?DriveDialogController}
     */
    this.driveDialogController_ = null;

    /**
     * Handler for command events.
     * @private {CommandHandler}
     */
    this.commandHandler_ = null;

    /**
     * Handler for the change of file selection.
     * @private {?FileSelectionHandler}
     */
    this.selectionHandler_ = null;

    /**
     * UI management class of file manager.
     * @private {?FileManagerUI}
     */
    this.ui_ = null;

    // ------------------------------------------------------------------------
    // Parameters determining the type of file manager.

    /**
     * Dialog type of this window.
     * @public {DialogType}
     */
    this.dialogType = DialogType.FULL_PAGE;

    /**
     * Startup parameters for this application.
     * @private {?LaunchParam}
     */
    this.launchParams_ = null;

    // ------------------------------------------------------------------------
    // Controllers.

    /**
     * File transfer controller.
     * @private {?FileTransferController}
     */
    this.fileTransferController_ = null;

    /**
     * Naming controller.
     * @private {?NamingController}
     */
    this.namingController_ = null;

    /**
     * Directory tree naming controller.
     * @private {DirectoryTreeNamingController}
     */
    this.directoryTreeNamingController_ = null;

    /**
     * Controller for search UI.
     * @private {?SearchController}
     */
    this.searchController_ = null;

    /**
     * Controller for directory scan.
     * @private {?ScanController}
     */
    this.scanController_ = null;

    /**
     * Controller for spinner.
     * @private {?SpinnerController}
     */
    this.spinnerController_ = null;

    /**
     * Sort menu controller.
     * @private {?SortMenuController}
     */
    this.sortMenuController_ = null;

    /**
     * Gear menu controller.
     * @private {?GearMenuController}
     */
    this.gearMenuController_ = null;

    /**
     * Controller for the context menu opened by the action bar button in the
     * check-select mode.
     * @private {?SelectionMenuController}
     */
    this.selectionMenuController_ = null;

    /**
     * Toolbar controller.
     * @private {?ToolbarController}
     */
    this.toolbarController_ = null;

    /**
     * App state controller.
     * @private {?AppStateController}
     */
    this.appStateController_ = null;

    /**
     * Dialog action controller.
     * @private {?DialogActionController}
     */
    this.dialogActionController_ = null;

    /**
     * List update controller.
     * @private {?MetadataUpdateController}
     */
    this.metadataUpdateController_ = null;

    /**
     * Last modified controller.
     * @private {LastModifiedController}
     */
    this.lastModifiedController_ = null;

    /**
     * Component for main window and its misc UI parts.
     * @private {?MainWindowComponent}
     */
    this.mainWindowComponent_ = null;

    /** @private {?TaskController} */
    this.taskController_ = null;

    /** @private {?QuickViewUma} */
    this.quickViewUma_ = null;

    /** @private {?QuickViewController} */
    this.quickViewController_ = null;

    /** @private {?FileTypeFiltersController} */
    this.fileTypeFiltersController_ = null;

    /**
     * Empty folder controller.
     * @private {?EmptyFolderController}
     */
    this.emptyFolderController_ = null;

    /**
     * Records histograms of directory-changed event.
     * @private {?NavigationUma}
     */
    this.navigationUma_ = null;

    // ------------------------------------------------------------------------
    // DOM elements.

    /**
     * @private {?FileManagerBaseInterface}
     */
    this.fileBrowserBackground_ = null;

    /**
     * The root DOM element of this app.
     * @private {?HTMLBodyElement}
     */
    this.dialogDom_ = null;

    /**
     * The document object of this app.
     * @private {?Document}
     */
    this.document_ = null;

    // ------------------------------------------------------------------------
    // Miscellaneous FileManager's states.

    /**
     * Promise object which is fulfilled when initialization for app state
     * controller is done.
     * @private {?Promise<void>}
     */
    this.initSettingsPromise_ = null;

    /**
     * Promise object which is fulfilled when initialization related to the
     * background page is done.
     * @private {?Promise<void>}
     */
    this.initBackgroundPagePromise_ = null;

    /**
     * Whether Drive is enabled. Retrieved from user preferences.
     * @private {?boolean}
     */
    this.driveEnabled_ = false;

    /**
     * A fake Drive placeholder item.
     * @private {?NavigationModelFakeItem}
     */
    this.fakeDriveItem_ = null;

    /**
     * Whether Trash is enabled or not, retrieved from user preferences.
     * @type {boolean}
     */
    this.trashEnabled = false;

    /**
     * A fake Trash placeholder item.
     * @private {?NavigationModelFakeItem}
     */
    this.fakeTrashItem_ = null;

    /**
     * A fake entry for Recents.
     * @private {?FakeEntry}
     */
    this.recentEntry_ = null;

    /**
     * Whether or not we are running in guest mode.
     * @private {boolean}
     */
    this.guestMode_ = false;

    /** @private {!Store} */
    this.store_ = getStore();

    startColorChangeUpdater();
  }

  /**
   * @return {!ProgressCenter}
   */
  get progressCenter() {
    return assert(this.fileBrowserBackground_.progressCenter);
  }

  /**
   * @return {DirectoryModel}
   */
  get directoryModel() {
    return this.directoryModel_;
  }

  /**
   * @return {DirectoryTreeNamingController}
   */
  get directoryTreeNamingController() {
    return this.directoryTreeNamingController_;
  }

  /**
   * @return {FileFilter}
   */
  get fileFilter() {
    return this.fileFilter_;
  }

  /**
   * @return {FolderShortcutsDataModel}
   */
  get folderShortcutsModel() {
    return this.folderShortcutsModel_;
  }

  /**
   * @return {ActionsController}
   */
  get actionsController() {
    return this.actionsController_;
  }

  /**
   * @return {CommandHandler}
   */
  get commandHandler() {
    return this.commandHandler_;
  }

  /**
   * @return {ProvidersModel}
   */
  get providersModel() {
    return this.providersModel_;
  }

  /**
   * @return {MetadataModel}
   */
  get metadataModel() {
    return this.metadataModel_;
  }

  /**
   * @return {FileSelectionHandler}
   */
  get selectionHandler() {
    return this.selectionHandler_;
  }

  /**
   * @return {DirectoryTree}
   */
  get directoryTree() {
    return this.ui_.directoryTree;
  }
  /**
   * @return {Document}
   */
  get document() {
    return this.document_;
  }

  /**
   * @return {FileTransferController}
   */
  get fileTransferController() {
    return this.fileTransferController_;
  }

  /**
   * @return {NamingController}
   */
  get namingController() {
    return this.namingController_;
  }

  /**
   * @return {TaskController}
   */
  get taskController() {
    return this.taskController_;
  }

  /**
   * @return {SpinnerController}
   */
  get spinnerController() {
    return this.spinnerController_;
  }

  /**
   * @return {FileOperationManager}
   */
  get fileOperationManager() {
    return this.fileOperationManager_;
  }

  /**
   * @return {!FilteredVolumeManager}
   */
  get volumeManager() {
    return this.volumeManager_;
  }

  /**
   * @return {Crostini}
   */
  get crostini() {
    return this.crostini_;
  }

  /**
   * @return {FileManagerUI}
   */
  get ui() {
    return this.ui_;
  }

  /**
   * @return {boolean} If the app is running in the guest mode.
   */
  get guestMode() {
    return this.guestMode_;
  }

  /**
   * Launch a new File Manager app.
   * @param {!FilesAppState=} appState App state.
   */
  launchFileManager(appState) {
    this.fileBrowserBackground_.launchFileManager(appState);
  }

  /**
   * Returns the last URL visited with visitURL() (e.g. for "Manage in Drive").
   * Used by the integration tests.
   * @return {string}
   */
  getLastVisitedURL() {
    return util.getLastVisitedURL();
  }

  /**
   * Returns a string translation from its translation ID.
   * @param {string} id The id of the translated string.
   * @return {string}
   */
  getTranslatedString(id) {
    return str(id);
  }

  /**
   * One time initialization for app state controller to load view option from
   * local storage.
   * @return {!Promise<void>}
   * @private
   */
  async startInitSettings_() {
    metrics.startInterval('Load.InitSettings');
    this.appStateController_ = new AppStateController(this.dialogType);
    await this.appStateController_.loadInitialViewOptions();
    metrics.recordInterval('Load.InitSettings');
  }

  /**
   * Updates guestMode_ field based on what the result of the util.isInGuestMode
   * helper function. It errs on the side of not-in-guestmode, if the util
   * function fails. The worse this causes are extra notifications.
   */
  async setGuestMode_() {
    try {
      const guest = await util.isInGuestMode();
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
   * @return {!Promise<void>}
   * @private
   */
  async initFileSystemUI_() {
    this.ui_.listContainer.startBatchUpdates();

    const fileListPromise = this.initFileList_();
    const currentDirectoryPromise = this.setupCurrentDirectory_();

    const self = this;

    let listBeingUpdated = null;
    this.directoryModel_.addEventListener('begin-update-files', () => {
      self.ui_.listContainer.currentList.startBatchUpdates();
      // Remember the list which was used when updating files started, so
      // endBatchUpdates() is called on the same list.
      listBeingUpdated = self.ui_.listContainer.currentList;
    });
    this.directoryModel_.addEventListener('end-update-files', () => {
      self.namingController_.restoreItemBeingRenamed();
      listBeingUpdated.endBatchUpdates();
      listBeingUpdated = null;
    });
    this.volumeManager_.addEventListener(
        VolumeManagerCommon.ARCHIVE_OPENED_EVENT_TYPE, event => {
          assert(event.detail.mountPoint);
          if (window.isFocused()) {
            this.directoryModel_.changeDirectoryEntry(event.detail.mountPoint);
          }
        });

    this.directoryModel_.addEventListener(
        'directory-changed',
        /** @param {!Event} event */
        event => {
          this.navigationUma_.onDirectoryChanged(event.newDirEntry);
        });

    this.initCommands_();

    assert(this.directoryModel_);
    assert(this.spinnerController_);
    assert(this.commandHandler_);
    assert(this.selectionHandler_);
    assert(this.launchParams_);
    assert(this.volumeManager_);
    assert(this.fileOperationManager_);
    assert(this.dialogDom_);

    if (util.isInlineSyncStatusEnabled()) {
      this.fileBrowserBackground_.driveSyncHandler.metadataModel =
          assert(this.metadataModel_);
    }
    this.scanController_ = new ScanController(
        this.directoryModel_, this.ui_.listContainer, this.spinnerController_,
        this.selectionHandler_);
    this.sortMenuController_ = new SortMenuController(
        this.ui_.sortButton, assert(this.directoryModel_.getFileList()));
    this.gearMenuController_ = new GearMenuController(
        this.ui_.gearButton, this.ui_.gearMenu, this.ui_.providersMenu,
        this.directoryModel_, this.commandHandler_,
        assert(this.providersModel_));
    this.selectionMenuController_ = new SelectionMenuController(
        this.ui_.selectionMenuButton,
        queryDecoratedElement('#file-context-menu', Menu));
    this.toolbarController_ = new ToolbarController(
        this.ui_.toolbar, this.ui_.dialogNavigationList, this.ui_.listContainer,
        this.selectionHandler_, this.directoryModel_, this.volumeManager_,
        this.fileOperationManager_,
        /** @type {!A11yAnnounce} */ (this.ui_));
    this.actionsController_ = new ActionsController(
        this.volumeManager_, assert(this.metadataModel_), this.directoryModel_,
        assert(this.folderShortcutsModel_),
        this.fileBrowserBackground_.driveSyncHandler, this.selectionHandler_,
        assert(this.ui_));
    if (this.dialogType === DialogType.FULL_PAGE) {
      this.driveDialogController_ = new DriveDialogController(this.ui_);
      this.fileBrowserBackground_.driveSyncHandler.addDialog(
          window.appID, this.driveDialogController_);
    }
    this.lastModifiedController_ = new LastModifiedController(
        this.ui_.listContainer.table, this.directoryModel_);

    this.quickViewModel_ = new QuickViewModel();
    const fileListSelectionModel = /** @type {!FileListSelectionModel} */ (
        this.directoryModel_.getFileListSelection());
    this.quickViewUma_ =
        new QuickViewUma(assert(this.volumeManager_), assert(this.dialogType));
    const metadataBoxController = new MetadataBoxController(
        this.metadataModel_, this.quickViewModel_, this.fileMetadataFormatter_,
        assert(this.volumeManager_));
    this.quickViewController_ = new QuickViewController(
        this, assert(this.metadataModel_), assert(this.selectionHandler_),
        assert(this.ui_.listContainer), assert(this.ui_.selectionMenuButton),
        assert(this.quickViewModel_), assert(this.taskController_),
        fileListSelectionModel, assert(this.quickViewUma_),
        metadataBoxController, this.dialogType, assert(this.volumeManager_),
        this.dialogDom_);

    assert(this.fileFilter_);
    assert(this.namingController_);
    assert(this.appStateController_);
    assert(this.taskController_);
    this.mainWindowComponent_ = new MainWindowComponent(
        this.dialogType, this.ui_, this.volumeManager_, this.directoryModel_,
        this.fileFilter_, this.selectionHandler_, this.namingController_,
        this.appStateController_, this.taskController_);

    this.initDataTransferOperations_();
    fileListPromise.then(() => {
      this.taskController_.setFileTransferController(
          this.fileTransferController_);
    });

    this.selectionHandler_.onFileSelectionChanged();
    this.ui_.listContainer.endBatchUpdates();

    const bannerController = new BannerController(
        this.directoryModel_, this.volumeManager_, assert(this.crostini_),
        this.dialogType);
    this.ui_.initBanners(bannerController);
    bannerController.initialize();

    this.ui_.attachFilesTooltip();
    this.ui_.decorateFilesMenuItems();
    this.ui_.selectionMenuButton.hidden = false;

    await Promise.all(
        [fileListPromise, currentDirectoryPromise, this.setGuestMode_()]);
  }

  /**
   * @private
   */
  initDataTransferOperations_() {
    // CopyManager are required for 'Delete' operation in
    // Open and Save dialogs. But drag-n-drop and copy-paste are not needed.
    if (this.dialogType !== DialogType.FULL_PAGE) {
      return;
    }

    this.fileTransferController_ = new FileTransferController(
        assert(this.document_), assert(this.ui_.listContainer),
        assert(this.ui_.directoryTree),
        this.ui_.showConfirmationDialog.bind(this.ui_), this.progressCenter,
        assert(this.fileOperationManager_), assert(this.metadataModel_),
        assert(this.directoryModel_), assert(this.volumeManager_),
        assert(this.selectionHandler_), this.ui_.toast);
  }

  /**
   * One-time initialization of commands.
   * @private
   */
  initCommands_() {
    assert(this.ui_.textContextMenu);

    this.commandHandler_ =
        new CommandHandler(this, assert(this.selectionHandler_));

    // TODO(hirono): Move the following block to the UI part.
    for (const button of this.dialogDom_.querySelectorAll('button[command]')) {
      CommandButton.decorate(button);
    }
    // Hook up the cr-button commands.
    for (const crButton of this.dialogDom_.querySelectorAll(
             'cr-button[command]')) {
      CommandButton.decorate(crButton);
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
   * @private
   */
  getDomInputs_() {
    return this.dialogDom_.querySelectorAll(
        'input[type=text], input[type=search], textarea, cr-input');
  }

  /**
   * Set context menu and handlers for an input element.
   * @private
   */
  setContextMenuForInput_(input) {
    let touchInduced = false;

    // stop contextmenu propagation for touch-induced events.
    input.addEventListener('touchstart', (e) => {
      touchInduced = true;
    });
    input.addEventListener('contextmenu', (e) => {
      if (touchInduced) {
        e.stopImmediatePropagation();
      }
      touchInduced = false;
    });
    input.addEventListener('click', (e) => {
      touchInduced = false;
    });

    contextMenuHandler.setContextMenu(input, this.ui_.textContextMenu);
    this.registerInputCommands_(input);
  }

  /**
   * Registers cut, copy, paste and delete commands on input element.
   *
   * @param {Node} node Text input element to register on.
   * @private
   */
  registerInputCommands_(node) {
    CommandUtil.forceDefaultHandler(node, 'cut');
    CommandUtil.forceDefaultHandler(node, 'copy');
    CommandUtil.forceDefaultHandler(node, 'paste');
    CommandUtil.forceDefaultHandler(node, 'delete');
    node.addEventListener('keydown', e => {
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

  /**
   * @return {!Promise<void>}
   */
  async initializeUI(dialogDom) {
    console.warn(`Files app starting up: ${this.dialogType}`);
    this.dialogDom_ = dialogDom;
    this.document_ = this.dialogDom_.ownerDocument;

    metrics.startInterval('Load.InitDocuments');
    // importElements depend on loadTimeData which is initialized in the
    // initBackgroundPagePromise_.
    await this.initBackgroundPagePromise_;
    await importElements();
    metrics.recordInterval('Load.InitDocuments');

    metrics.startInterval('Load.InitUI');
    this.document_.documentElement.classList.add('files-ng');
    this.dialogDom_.classList.add('files-ng');

    // Add theme attribute so widgets can render different styles based on
    // this attribute:
    // [theme="legacy"] -> Legacy style, [theme="refresh23"] -> Refresh23 style
    const theme = util.isJellyEnabled() ? 'refresh23' : 'legacy';
    this.document_.documentElement.setAttribute('theme', theme);
    this.dialogDom_.setAttribute('theme', theme);

    chrome.fileManagerPrivate.isTabletModeEnabled(
        this.onTabletModeChanged_.bind(this));
    chrome.fileManagerPrivate.onTabletModeChanged.addListener(
        this.onTabletModeChanged_.bind(this));

    this.initEssentialUI_();
    this.initAdditionalUI_();
    await this.initSettingsPromise_;
    const fileSystemUIPromise = this.initFileSystemUI_();
    // Initialize the Store for the whole app.
    const store = getStore();
    store.init(getEmptyState());
    this.initUIFocus_();
    metrics.recordInterval('Load.InitUI');
    return fileSystemUIPromise;
  }

  /**
   * Initializes general purpose basic things, which are used by other
   * initializing methods.
   * @private
   */
  initGeneral_() {
    // Initialize the application state.
    // TODO(mtomasz): Unify window.appState with location.search format.
    if (window.appState) {
      const params = {};

      for (const name in window.appState) {
        params[name] = window.appState[name];
      }

      this.launchParams_ = new LaunchParam(params);
    } else {
      // Used by the select dialog and SWA.
      let json = {};
      if (location.search) {
        const query = location.search.substr(1);
        try {
          json = /** @type {!FilesAppState} */ (
              JSON.parse(decodeURIComponent(query)));
        } catch (e) {
          console.debug(`Error parsing location.search "${query}" due to ${e}`);
        }
      }
      this.launchParams_ = new LaunchParam(json);
    }

    // Initialize the member variables that depend this.launchParams_.
    this.dialogType = this.launchParams_.type;
  }

  /**
   * Initializes the background page.
   * @return {!Promise<void>}
   * @private
   */
  async startInitBackgroundPage_() {
    metrics.startInterval('Load.InitBackgroundPage');

    this.fileBrowserBackground_ =
        /** @type {!ForegroundWindow} */ (window).background;

    await new Promise(resolve => this.fileBrowserBackground_.ready(resolve));

    // For the SWA, we load background and foreground in the same Window, avoid
    // loading the `data` twice.
    if (!loadTimeData.isInitialized()) {
      loadTimeData.data = this.fileBrowserBackground_.stringData;
    }
    if (util.runningInBrowser()) {
      this.fileBrowserBackground_.registerDialog(window);
    }
    this.fileOperationManager_ =
        this.fileBrowserBackground_.fileOperationManager;
    this.crostini_ = this.fileBrowserBackground_.crostini;

    metrics.recordInterval('Load.InitBackgroundPage');
  }

  /**
   * Initializes the VolumeManager instance.
   * @private
   */
  async initVolumeManager_() {
    const allowedPaths = this.getAllowedPaths_();
    const writableOnly =
        this.launchParams_.type === DialogType.SELECT_SAVEAS_FILE;
    const disabledVolumes =
        /** @type {!Array<!VolumeManagerCommon.VolumeType>} */ (
            await this.getDisabledVolumes_());

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
  }

  /**
   * One time initialization of the essential UI elements in the Files app.
   * These elements will be shown to the user. Only visible elements should be
   * initialized here. Any heavy operation should be avoided. The Files app's
   * window is shown at the end of this routine.
   * @private
   */
  initEssentialUI_() {
    // Record stats of dialog types. New values must NOT be inserted into the
    // array enumerating the types. It must be in sync with
    // FileDialogType enum in tools/metrics/histograms/histogram.xml.
    const metricName = 'SWA.Create';
    metrics.recordEnum(metricName, this.dialogType, [
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
    this.ui_ = new FileManagerUI(
        assert(this.providersModel_), this.dialogDom_, this.launchParams_);
  }

  /**
   * One-time initialization of various DOM nodes. Loads the additional DOM
   * elements visible to the user. Initialize here elements, which are expensive
   * or hidden in the beginning.
   * @private
   */
  initAdditionalUI_() {
    assert(this.metadataModel_);
    assert(this.volumeManager_);
    assert(this.dialogDom_);
    assert(this.ui_);

    // Cache nodes we'll be manipulating.
    const dom = this.dialogDom_;
    assert(dom);

    const table = queryRequiredElement('.detail-table', dom);
    FileTable.decorate(
        table, this.metadataModel_, this.volumeManager_,
        /** @type {!A11yAnnounce} */ (this.ui_),
        this.dialogType == DialogType.FULL_PAGE);
    const grid = queryRequiredElement('.thumbnail-grid', dom);
    FileGrid.decorate(
        grid, this.metadataModel_, this.volumeManager_,
        /** @type {!A11yAnnounce} */ (this.ui_));

    this.ui_.initAdditionalUI(
        assertInstanceof(table, FileTable), assertInstanceof(grid, FileGrid),
        this.volumeManager_);

    // Handle UI events.
    this.progressCenter.addPanel(this.ui_.progressCenterPanel);

    // Arrange the file list.
    this.ui_.listContainer.table.normalizeColumns();
    this.ui_.listContainer.table.redraw();
  }

  /**
   * One-time initialization of focus. This should run at the last of UI
   *  initialization.
   * @private
   */
  initUIFocus_() {
    this.ui_.initUIFocus();
  }

  /**
   * Constructs table and grid (heavy operation).
   * @return {!Promise<void>}
   * @private
   */
  async initFileList_() {
    const singleSelection = this.dialogType == DialogType.SELECT_OPEN_FILE ||
        this.dialogType == DialogType.SELECT_FOLDER ||
        this.dialogType == DialogType.SELECT_UPLOAD_FOLDER ||
        this.dialogType == DialogType.SELECT_SAVEAS_FILE;

    assert(this.volumeManager_);
    assert(this.fileOperationManager_);
    assert(this.metadataModel_);
    this.directoryModel_ = new DirectoryModel(
        singleSelection, this.fileFilter_, this.metadataModel_,
        this.volumeManager_, this.fileOperationManager_);

    this.folderShortcutsModel_ =
        new FolderShortcutsDataModel(this.volumeManager_);

    this.androidAppListModel_ = new AndroidAppListModel(
        this.launchParams_.showAndroidPickerApps,
        this.launchParams_.includeAllFiles, this.launchParams_.typeList);

    this.recentEntry_ = new FakeEntryImpl(
        str('RECENT_ROOT_LABEL'), VolumeManagerCommon.RootType.RECENT,
        this.getSourceRestriction_(),
        chrome.fileManagerPrivate.FileCategory.ALL);
    if (util.isFilesAppExperimental()) {
      this.store_.dispatch(addUiEntry({entry: this.recentEntry_}));
    }
    assert(this.launchParams_);
    this.selectionHandler_ = new FileSelectionHandler(
        assert(this.directoryModel_), assert(this.fileOperationManager_),
        assert(this.ui_.listContainer), assert(this.metadataModel_),
        assert(this.volumeManager_), this.launchParams_.allowedPaths);

    // TODO(mtomasz, yoshiki): Create navigation list earlier, and here just
    // attach the directory model.
    const directoryTreePromise = this.initDirectoryTree_();

    this.ui_.listContainer.listThumbnailLoader = new ListThumbnailLoader(
        this.directoryModel_, assert(this.thumbnailModel_),
        this.volumeManager_);
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
        this.ui_.listContainer, assert(this.ui_.alertDialog),
        assert(this.ui_.confirmDialog), this.directoryModel_,
        assert(this.fileFilter_), this.selectionHandler_);

    // Create task controller.
    this.taskController_ = new TaskController(
        this.dialogType, this.volumeManager_, this.ui_, this.metadataModel_,
        this.directoryModel_, this.selectionHandler_,
        this.metadataUpdateController_, assert(this.crostini_),
        this.progressCenter);

    // Create search controller.
    this.searchController_ = new SearchController(
        this.ui_.searchContainer,
        this.directoryModel_,
        this.volumeManager_,
        assert(this.taskController_),
        assert(this.ui_),
    );

    // Create directory tree naming controller.
    this.directoryTreeNamingController_ = new DirectoryTreeNamingController(
        this.directoryModel_, assert(this.ui_.directoryTree),
        this.ui_.alertDialog);

    // Create spinner controller.
    this.spinnerController_ =
        new SpinnerController(this.ui_.listContainer.spinner);
    this.spinnerController_.blink();

    // Create dialog action controller.
    this.dialogActionController_ = new DialogActionController(
        this.dialogType, this.ui_.dialogFooter, this.directoryModel_,
        this.metadataModel_, this.volumeManager_, this.fileFilter_,
        this.namingController_, this.selectionHandler_, this.launchParams_);

    // Create file-type filter controller.
    this.fileTypeFiltersController_ = new FileTypeFiltersController(
        this.ui_.fileTypeFilterContainer, this.directoryModel_,
        this.recentEntry_, /** @type {!A11yAnnounce} */ (this.ui_));
    this.emptyFolderController_ = new EmptyFolderController(
        this.ui_.emptyFolder, this.directoryModel_, this.recentEntry_);


    return directoryTreePromise;
  }

  /**
   * Based on the dialog type and dialog caller, sets the list of volumes
   * that should be disabled according to Data Leak Prevention rules.
   * @return {Promise<!Array<!VolumeManagerCommon.VolumeType>>}
   */
  async getDisabledVolumes_() {
    if (this.dialogType !== DialogType.SELECT_SAVEAS_FILE ||
        !util.isDlpEnabled()) {
      return [];
    }
    const caller = await getDialogCaller();
    if (!caller.url) {
      return [];
    }
    const dlpBlockedComponents = await getDlpBlockedComponents(caller.url);
    const disabledVolumes = [];
    for (const c of dlpBlockedComponents) {
      disabledVolumes.push(
          /** @type {!VolumeManagerCommon.VolumeType }*/ (c));
    }
    return disabledVolumes;
  }

  /**
   * @return {!Promise<void>}
   * @private
   */
  async initDirectoryTree_() {
    this.navigationUma_ = new NavigationUma(assert(this.volumeManager_));

    const fakeEntriesVisible =
        this.dialogType !== DialogType.SELECT_SAVEAS_FILE;

    const directoryTree = /** @type {DirectoryTree} */
        (this.dialogDom_.querySelector('#directory-tree'));
    DirectoryTree.decorate(
        directoryTree, assert(this.directoryModel_),
        assert(this.volumeManager_), assert(this.metadataModel_),
        assert(this.fileOperationManager_), fakeEntriesVisible);

    directoryTree.dataModel = new NavigationListModel(
        assert(this.volumeManager_), assert(this.folderShortcutsModel_),
        fakeEntriesVisible && !isFolderDialogType(this.launchParams_.type) ?
            new NavigationModelFakeItem(
                str('RECENT_ROOT_LABEL'), NavigationModelItemType.RECENT,
                assert(this.recentEntry_)) :
            null,
        assert(this.directoryModel_), assert(this.androidAppListModel_),
        this.dialogType);

    this.ui_.initDirectoryTree(directoryTree);

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

    // The fmp.onCrostiniChanged receives enabled/disabled events via a pref
    // watcher and share/unshare events.  The enabled/disabled prefs are
    // handled in fmp.onCrostiniChanged rather than fmp.onPreferencesChanged
    // to keep crostini logic colocated, and to have an API that best supports
    // multiple VMs.
    chrome.fileManagerPrivate.onCrostiniChanged.addListener(
        this.onCrostiniChanged_.bind(this));
    this.crostiniController_ = new CrostiniController(
        assert(this.crostini_), this.directoryModel_,
        assert(this.directoryTree),
        this.volumeManager_.isDisabled(
            VolumeManagerCommon.VolumeType.CROSTINI));
    await this.crostiniController_.redraw();
    // Never show toast in an open-file dialog.
    const maybeShowToast = this.dialogType === DialogType.FULL_PAGE;
    await this.crostiniController_.loadSharedPaths(
        maybeShowToast, this.ui_.toast);

    if (util.isGuestOsEnabled()) {
      this.guestOsController_ = new GuestOsController(
          this.directoryModel_, assert(this.directoryTree),
          this.volumeManager_);
      await this.guestOsController_.refresh();
    }
  }

  /**
   * Listens for the enable and disable events in order to add or remove the
   * directory tree 'Linux files' root item.
   *
   * @param {chrome.fileManagerPrivate.CrostiniEvent} event
   * @return {!Promise<void>}
   * @private
   */
  async onCrostiniChanged_(event) {
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
              assert(this.metadataModel_), assert(this.ui_), moveMessage,
              copyMessage, this.fileTransferController_,
              assert(this.directoryModel_));
        }
        break;
    }
  }

  /**
   * Sets up the current directory during initialization.
   * @return {!Promise<void>}
   * @private
   */
  async setupCurrentDirectory_() {
    const tracker = this.directoryModel_.createDirectoryChangeTracker();
    tracker.start();

    // Wait until the volume manager is initialized.
    await new Promise(
        resolve => this.volumeManager_.ensureInitialized(resolve));

    let nextCurrentDirEntry;
    let selectionEntry;

    // Resolve the selectionURL to selectionEntry or to currentDirectoryEntry in
    // case of being a display root or a default directory to open files.
    if (this.launchParams_.selectionURL) {
      if (this.launchParams_.selectionURL == this.recentEntry_.toURL()) {
        nextCurrentDirEntry = this.recentEntry_;
      } else {
        try {
          const inEntry = await new Promise((resolve, reject) => {
            window.webkitResolveLocalFileSystemURL(
                this.launchParams_.selectionURL, resolve, reject);
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
        } catch (error) {
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
      metrics.startInterval('Load.ProcessInitialSearchQuery');
      this.store_.dispatch(updateSearch({
        query: searchQuery,
        status: PropStatus.STARTED,
        options: undefined,
      }));
      // Show a spinner, as the crossover search function call could be slow.
      const hideSpinnerCallback = this.spinnerController_.show();
      const queryMatchedDirEntry =
          await crossoverSearchUtils.findQueryMatchedDirectoryEntry(
              this.directoryTree.dataModel_, this.directoryModel_, searchQuery);
      if (queryMatchedDirEntry) {
        nextCurrentDirEntry = queryMatchedDirEntry;
      }
      hideSpinnerCallback();
      metrics.recordInterval('Load.ProcessInitialSearchQuery');
    }

    // Resolve the currentDirectoryURL to currentDirectoryEntry (if not done by
    // the previous step).
    if (!nextCurrentDirEntry && this.launchParams_.currentDirectoryURL) {
      try {
        const inEntry = await new Promise((resolve, reject) => {
          window.webkitResolveLocalFileSystemURL(
              this.launchParams_.currentDirectoryURL, resolve, reject);
        });
        const locationInfo = this.volumeManager_.getLocationInfo(inEntry);
        if (locationInfo) {
          nextCurrentDirEntry = inEntry;
        }
      } catch (error) {
        console.warn(error.stack || error);
      }
    }

    // If the directory to be changed to is not available, then first fallback
    // to the parent of the selection entry.
    if (!nextCurrentDirEntry && selectionEntry) {
      nextCurrentDirEntry = await new Promise(resolve => {
        selectionEntry.getParent(resolve);
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
            locationInfo.rootType ===
                VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME) {
          const volumeInfo =
              this.volumeManager_.getVolumeInfo(nextCurrentDirEntry);
          if (!volumeInfo) {
            nextCurrentDirEntry = null;
          } else {
            try {
              nextCurrentDirEntry = await volumeInfo.resolveDisplayRoot();
            } catch (error) {
              console.warn(error.stack || error);
              nextCurrentDirEntry = null;
            }
          }
        }
      }
    }

    // If the resolved directory to be changed is blocked by DLP, we should
    // fallback to the default display root.
    if (nextCurrentDirEntry && util.isDlpEnabled()) {
      const volumeInfo = this.volumeManager_.getVolumeInfo(nextCurrentDirEntry);
      if (volumeInfo && this.volumeManager_.isDisabled(volumeInfo.volumeType)) {
        console.warn('Target directory is DLP blocked, redirecting to MyFiles');
        nextCurrentDirEntry = null;
      }
    }

    // If the directory to be changed to is still not resolved, then fallback to
    // the default display root.
    if (!nextCurrentDirEntry) {
      nextCurrentDirEntry = await new Promise(resolve => {
        this.volumeManager_.getDefaultDisplayRoot(resolve);
      });
    }

    // If selection failed to be resolved (eg. didn't exist, in case of saving a
    // file, or in case of a fallback of the current directory, then try to
    // resolve again using the target name.
    if (!selectionEntry && nextCurrentDirEntry &&
        this.launchParams_.targetName) {
      // Try to resolve as a file first. If it fails, then as a directory.
      try {
        selectionEntry = await new Promise((resolve, reject) => {
          nextCurrentDirEntry.getFile(
              this.launchParams_.targetName, {}, resolve, reject);
        });
      } catch (error1) {
        // Failed to resolve as a file. Try to resolve as a directory.
        try {
          selectionEntry = await new Promise((resolve, reject) => {
            nextCurrentDirEntry.getDirectory(
                this.launchParams_.targetName, {}, resolve, reject);
          });
        } catch (error2) {
          // If `targetName` doesn't exist we just don't select it, thus we
          // don't need to log the failure.
          if (error1.name !== 'NotFoundError') {
            console.warn(error1.stack || error1);
            console.log(error1);
          }
          if (error2.name !== 'NotFoundError') {
            console.warn(error2.stack || error2);
          }
        }
      }
    }

    // If there is no target select MyFiles by default.
    if (!nextCurrentDirEntry && this.directoryTree.dataModel.myFilesModel_) {
      nextCurrentDirEntry = this.directoryTree.dataModel.myFilesModel_.entry;
    }

    // Check directory change.
    tracker.stop();
    if (!tracker.hasChanged) {
      // Finish setup current directory.
      await this.finishSetupCurrentDirectory_(
          nextCurrentDirEntry, selectionEntry, this.launchParams_.targetName);
    }
  }

  /**
   * @param {?DirectoryEntry} directoryEntry Directory to be opened.
   * @param {Entry=} opt_selectionEntry Entry to be selected.
   * @param {string=} opt_suggestedName Suggested name for a non-existing
   *     selection.
   * @return {!Promise<void> }
   * @private
   */
  async finishSetupCurrentDirectory_(
      directoryEntry, opt_selectionEntry, opt_suggestedName) {
    // Open the directory, and select the selection (if passed).
    const promise = (async () => {
      console.warn('Files app has started');
      if (directoryEntry) {
        await new Promise(resolve => {
          this.directoryModel_.changeDirectoryEntry(
              assert(directoryEntry), resolve);
        });
        if (opt_selectionEntry) {
          this.directoryModel_.selectEntry(opt_selectionEntry);
        }
        if (this.launchParams_.searchQuery) {
          this.searchController_.setSearchQuery(this.launchParams_.searchQuery);
        }
      } else {
        console.warn('No entry for finishSetupCurrentDirectory_');
      }
      this.ui_.addLoadedAttribute();
    })();

    if (this.dialogType === DialogType.SELECT_SAVEAS_FILE) {
      this.ui_.dialogFooter.filenameInput.value = opt_suggestedName || '';
      this.ui_.dialogFooter.selectTargetNameInFilenameInput();
    }

    return promise;
  }

  /**
   * Return DirectoryEntry of the current directory or null.
   * @return {DirectoryEntry|FakeEntry|FilesAppDirEntry} DirectoryEntry of the
   *     current directory.
   *     Returns null if the directory model is not ready or the current
   *     directory is not set.
   */
  getCurrentDirectoryEntry() {
    return this.directoryModel_ && this.directoryModel_.getCurrentDirEntry();
  }

  /**
   * Unload handler for the page.
   * @private
   */
  onUnload_() {
    if (this.directoryModel_) {
      this.directoryModel_.dispose();
    }

    if (this.volumeManager_) {
      this.volumeManager_.dispose();
    }

    if (this.fileTransferController_) {
      for (const taskId of assert(
               this.fileTransferController_.pendingTaskIds)) {
        const item = this.progressCenter.getItemById(taskId);
        item.message = '';
        item.state = ProgressItemState.CANCELED;
        this.progressCenter.updateItem(item);
      }
    }

    if (this.ui_ && this.ui_.progressCenterPanel) {
      this.progressCenter.removePanel(this.ui_.progressCenterPanel);
    }

    if (this.driveDialogController_) {
      this.fileBrowserBackground_.driveSyncHandler.removeDialog(window.appID);
    }
  }

  /**
   * Returns allowed path for the dialog by considering:
   * 1) The launch parameter which specifies generic category of valid files
   * paths.
   * 2) Files app's unique capabilities and restrictions.
   * @returns {AllowedPaths}
   */
  getAllowedPaths_() {
    let allowedPaths = this.launchParams_.allowedPaths;
    // The native implementation of the Files app creates snapshot files for
    // non-native files. But it does not work for folders (e.g., dialog for
    // loading unpacked extensions).
    if (allowedPaths === AllowedPaths.NATIVE_PATH &&
        !isFolderDialogType(this.launchParams_.type)) {
      if (this.launchParams_.type == DialogType.SELECT_SAVEAS_FILE) {
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
   * @returns {chrome.fileManagerPrivate.SourceRestriction}
   */
  getSourceRestriction_() {
    const allowedPaths = this.getAllowedPaths_();
    if (allowedPaths == AllowedPaths.NATIVE_PATH) {
      return chrome.fileManagerPrivate.SourceRestriction.NATIVE_SOURCE;
    }
    return chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE;
  }

  /**
   * @return {FileSelection} Selection object.
   */
  getSelection() {
    return this.selectionHandler_.selection;
  }

  /**
   * @return {ArrayDataModel} File list.
   */
  getFileList() {
    return this.directoryModel_.getFileList();
  }

  /**
   * @return {!List} Current list object.
   */
  getCurrentList() {
    return this.ui.listContainer.currentList;
  }

  /**
   * Add or remove the fake Drive and Trash item from the directory tree when
   * the prefs change. If Drive or Trash has been enabled by prefs, add the item
   * otherwise remove it. This supports dynamic refresh when the pref changes.
   */
  async onPreferencesChanged_() {
    let prefs = null;
    try {
      prefs = await getPreferences();
    } catch (e) {
      console.error('Failed to retrieve preferences:', e);
      return;
    }

    let redraw = false;
    if (this.driveEnabled_ !== prefs.driveEnabled) {
      this.driveEnabled_ = prefs.driveEnabled;
      this.toggleDriveRootOnPreferencesUpdate_();
      redraw = true;
    }

    if (this.trashEnabled !== prefs.trashEnabled) {
      this.trashEnabled = prefs.trashEnabled;
      this.toggleTrashRootOnPreferencesUpdate_();
      this.toolbarController_.moveToTrashCommand.disabled = !this.trashEnabled;
      this.toolbarController_.moveToTrashCommand.canExecuteChange(
          this.ui_.listContainer.currentList);
      redraw = true;
    }

    this.updateOfficePrefs_(prefs);

    if (util.isSearchV2Enabled()) {
      this.ui_.nudgeContainer.showNudge(NudgeType['SEARCH_V2_EDUCATION_NUDGE']);
    }

    if (redraw) {
      this.directoryTree.redraw(false);
    }
  }

  /**
   * @param {!chrome.fileManagerPrivate.Preferences} prefs
   * @private
   */
  async updateOfficePrefs_(prefs) {
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
      console.debug('Reset OneDrive move to cloud nudge');
    }
    if (prefs.officeFileMovedGoogleDrive === 0 &&
        await this.ui_.nudgeContainer.checkSeen(
            NudgeType['DRIVE_MOVED_FILE_NUDGE'])) {
      this.ui_.nudgeContainer.clearSeen(NudgeType['DRIVE_MOVED_FILE_NUDGE']);
      console.debug('Reset Google Drive move to cloud nudge');
    }
  }

  /**
   * Toggles the trash root visibility when the `trashEnabled` preference is
   * updated.
   * @private
   */
  toggleTrashRootOnPreferencesUpdate_() {
    if (this.trashEnabled) {
      if (!this.fakeTrashItem_) {
        this.fakeTrashItem_ = new NavigationModelFakeItem(
            str('TRASH_ROOT_LABEL'), NavigationModelItemType.TRASH,
            new TrashRootEntry());
      }
      if (util.isFilesAppExperimental()) {
        this.store_.dispatch(addUiEntry({entry: this.fakeTrashItem_.entry}));
      }
      this.directoryTree.dataModel.fakeTrashItem = this.fakeTrashItem_;
      return;
    }

    if (util.isFilesAppExperimental()) {
      this.store_.dispatch(removeUiEntry({key: trashRootKey}));
    }
    this.directoryTree.dataModel.fakeTrashItem = null;
    this.navigateAwayFromDisabledRoot_(this.fakeTrashItem_);
  }

  /**
   * Toggles the drive root visibility when the `driveEnabled` preference is
   * updated.
   * @private
   */
  toggleDriveRootOnPreferencesUpdate_() {
    if (this.driveEnabled_) {
      const driveFakeRoot = new FakeEntryImpl(
          str('DRIVE_DIRECTORY_LABEL'),
          VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);
      if (!this.fakeDriveItem_) {
        this.fakeDriveItem_ = new NavigationModelFakeItem(
            str('DRIVE_DIRECTORY_LABEL'), NavigationModelItemType.DRIVE,
            driveFakeRoot);
        this.fakeDriveItem_.disabled = this.volumeManager_.isDisabled(
            VolumeManagerCommon.VolumeType.DRIVE);
      }
      this.directoryTree.dataModel.fakeDriveItem = this.fakeDriveItem_;
      return;
    }
    this.directoryTree.dataModel.fakeDriveItem = null;
    this.navigateAwayFromDisabledRoot_(this.fakeDriveItem_);
  }

  /**
   * If the root item has been disabled but it is the current visible entry,
   * navigate away from it to the default display root.
   * @param {?NavigationModelFakeItem} rootItem The item to navigate away from.
   * @private
   */
  navigateAwayFromDisabledRoot_(rootItem) {
    if (!rootItem) {
      return;
    }
    // The fake root item is being hidden so navigate away if it's the
    // current directory.
    if (this.directoryModel_.getCurrentDirEntry() === rootItem.entry) {
      this.volumeManager_.getDefaultDisplayRoot((displayRoot) => {
        if (this.directoryModel_.getCurrentDirEntry() === rootItem.entry &&
            displayRoot) {
          this.directoryModel_.changeDirectoryEntry(displayRoot);
        }
      });
    }
  }

  /**
   * Updates the DOM to reflect the specified tablet mode `enabled` state.
   * @param {boolean} enabled
   * @private
   */
  onTabletModeChanged_(enabled) {
    this.dialogDom_.classList.toggle('tablet-mode-enabled', enabled);
  }
}
