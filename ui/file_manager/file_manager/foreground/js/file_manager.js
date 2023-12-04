// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../definitions/file_manager_private.js';
import '../../widgets/xf_jellybean.js';
import 'chrome://resources/cros_components/switch/switch.js';
import '../../background/js/test_util.js';
import '../../background/js/file_manager_base.js';

import {assert, assertInstanceof} from 'chrome://resources/ash/common/assert.js';
import {NativeEventTarget as EventTarget} from 'chrome://resources/ash/common/event_target.js';
import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
// @ts-ignore: error TS2792: Cannot find module
// 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js'.
// Did you mean to set the 'moduleResolution' option to 'nodenext', or to add
// aliases to the 'paths' option?
import {ColorChangeUpdater} from 'chrome://resources/cr_components/color_change_listener/colors_css_updater.js';
import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {FileManagerBase} from '../../background/js/file_manager_base.js';
import {getBulkPinProgress, getDialogCaller, getDlpBlockedComponents, getDriveConnectionState, getPreferences} from '../../common/js/api.js';
import {ArrayDataModel} from '../../common/js/array_data_model.js';
import {isFolderDialogType} from '../../common/js/dialog_type.js';
import {getKeyModifiers, queryDecoratedElement, queryRequiredElement} from '../../common/js/dom_utils.js';
import {EntryList, FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {FilesAppState} from '../../common/js/files_app_state.js';
import {FilteredVolumeManager} from '../../common/js/filtered_volume_manager.js';
import {isDlpEnabled, isGuestOsEnabled, isNewDirectoryTreeEnabled} from '../../common/js/flags.js';
import {recordEnum, recordInterval, startInterval} from '../../common/js/metrics.js';
import {ProgressItemState} from '../../common/js/progress_center_common.js';
import {str} from '../../common/js/translations.js';
import {TrashRootEntry} from '../../common/js/trash.js';
import {getLastVisitedURL, isInGuestMode, runningInBrowser} from '../../common/js/util.js';
import {AllowedPaths, ARCHIVE_OPENED_EVENT_TYPE, RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {DirectoryTreeContainer} from '../../containers/directory_tree_container.js';
import {NudgeType} from '../../containers/nudge_container.js';
import {Crostini} from '../../externs/background/crostini.js';
import {ProgressCenter} from '../../externs/background/progress_center.js';
import {CommandHandlerDeps} from '../../externs/command_handler_deps.js';
import {FakeEntry, FilesAppDirEntry} from '../../externs/files_app_entry_interfaces.js';
import {DialogType, PropStatus, SearchLocation} from '../../externs/ts/state.js';
import {Store} from '../../externs/ts/store.js';
import {getMyFiles} from '../../state/ducks/all_entries.js';
import {updateBulkPinProgress} from '../../state/ducks/bulk_pinning.js';
import {updateDeviceConnectionState} from '../../state/ducks/device.js';
import {updateDriveConnectionStatus} from '../../state/ducks/drive.js';
import {setLaunchParameters} from '../../state/ducks/launch_params.js';
import {updatePreferences} from '../../state/ducks/preferences.js';
import {getDefaultSearchOptions, updateSearch} from '../../state/ducks/search.js';
import {addUiEntry, removeUiEntry} from '../../state/ducks/ui_entries.js';
import {driveRootEntryListKey, trashRootKey} from '../../state/ducks/volumes.js';
import {getEmptyState, getEntry, getStore} from '../../state/store.js';

import {ActionsController} from './actions_controller.js';
import {AndroidAppListModel} from './android_app_list_model.js';
import {AppStateController} from './app_state_controller.js';
import {BannerController} from './banner_controller.js';
import {findQueryMatchedDirectoryEntry} from './crossover_search_utils.js';
import {CrostiniController} from './crostini_controller.js';
import {DialogActionController} from './dialog_action_controller.js';
import {FileFilter} from './directory_contents.js';
import {DirectoryModel} from './directory_model.js';
import {DirectoryTreeNamingController} from './directory_tree_naming_controller.js';
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
     * @private @type {!FilteredVolumeManager}
     */
    this.volumeManager_;

    /** @private @type {?Crostini} */
    this.crostini_ = null;

    /** @private @type {?CrostiniController} */
    this.crostiniController_ = null;

    /** @private @type {?GuestOsController} */
    this.guestOsController_ = null;

    /** @private @type {?MetadataModel} */
    this.metadataModel_ = null;

    /** @private @const @type {!FileMetadataFormatter} */
    this.fileMetadataFormatter_ = new FileMetadataFormatter();

    /** @private @type {?ThumbnailModel} */
    this.thumbnailModel_ = null;

    /**
     * File filter.
     * @private @type {?FileFilter}
     */
    this.fileFilter_ = null;

    /**
     * Model of current directory.
     * @private @type {?DirectoryModel}
     */
    this.directoryModel_ = null;

    /**
     * Model of folder shortcuts.
     * @private @type {?FolderShortcutsDataModel}
     */
    this.folderShortcutsModel_ = null;

    /**
     * Model of Android apps.
     * @private @type {?AndroidAppListModel}
     */
    this.androidAppListModel_ = null;

    /**
     * Model for providers (providing extensions).
     * @private @type {?ProvidersModel}
     */
    this.providersModel_ = null;

    /**
     * Model for quick view.
     * @private @type {?QuickViewModel}
     */
    this.quickViewModel_ = null;

    /**
     * Controller for actions for current selection.
     * @private @type {ActionsController}
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'ActionsController'.
    this.actionsController_ = null;

    /**
     * Handler for command events.
     * @private @type {CommandHandler}
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'CommandHandler'.
    this.commandHandler_ = null;

    /**
     * Handler for the change of file selection.
     * @private @type {?FileSelectionHandler}
     */
    this.selectionHandler_ = null;

    /**
     * UI management class of file manager.
     * @private @type {?FileManagerUI}
     */
    this.ui_ = null;

    // ------------------------------------------------------------------------
    // Parameters determining the type of file manager.

    /**
     * Dialog type of this window.
     * @public @type {DialogType}
     */
    this.dialogType = DialogType.FULL_PAGE;

    /**
     * Startup parameters for this application.
     * @private @type {?LaunchParam}
     */
    this.launchParams_ = null;

    // ------------------------------------------------------------------------
    // Controllers.

    /**
     * File transfer controller.
     * @private @type {?FileTransferController}
     */
    this.fileTransferController_ = null;

    /**
     * Naming controller.
     * @private @type {?NamingController}
     */
    this.namingController_ = null;

    /**
     * Directory tree naming controller.
     * @private @type {DirectoryTreeNamingController}
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'DirectoryTreeNamingController'.
    this.directoryTreeNamingController_ = null;

    /**
     * Controller for directory scan.
     * @private @type {?ScanController}
     */
    this.scanController_ = null;

    /**
     * Controller for spinner.
     * @private @type {?SpinnerController}
     */
    this.spinnerController_ = null;

    /**
     * Sort menu controller.
     * @private @type {?SortMenuController}
     */
    this.sortMenuController_ = null;

    /**
     * Gear menu controller.
     * @private @type {?GearMenuController}
     */
    this.gearMenuController_ = null;

    /**
     * Controller for the context menu opened by the action bar button in the
     * check-select mode.
     * @private @type {?SelectionMenuController}
     */
    this.selectionMenuController_ = null;

    /**
     * Toolbar controller.
     * @private @type {?ToolbarController}
     */
    this.toolbarController_ = null;

    /**
     * App state controller.
     * @private @type {?AppStateController}
     */
    this.appStateController_ = null;

    /**
     * Dialog action controller.
     * @private @type {?DialogActionController}
     */
    this.dialogActionController_ = null;

    /**
     * List update controller.
     * @private @type {?MetadataUpdateController}
     */
    this.metadataUpdateController_ = null;

    /**
     * Last modified controller.
     * @private @type {LastModifiedController}
     */
    // @ts-ignore: error TS2322: Type 'null' is not assignable to type
    // 'LastModifiedController'.
    this.lastModifiedController_ = null;

    /**
     * Component for main window and its misc UI parts.
     * @private @type {?MainWindowComponent}
     */
    this.mainWindowComponent_ = null;

    /** @private @type {?TaskController} */
    this.taskController_ = null;

    /** @private @type {?QuickViewUma} */
    this.quickViewUma_ = null;

    /** @private @type {?QuickViewController} */
    this.quickViewController_ = null;

    /** @private @type {?FileTypeFiltersController} */
    this.fileTypeFiltersController_ = null;

    /**
     * Empty folder controller.
     * @private @type {?EmptyFolderController}
     */
    this.emptyFolderController_ = null;

    /**
     * Records histograms of directory-changed event.
     * @private @type {?NavigationUma}
     */
    this.navigationUma_ = null;

    // ------------------------------------------------------------------------
    // DOM elements.

    /**
     * @private @type {?FileManagerBase}
     */
    this.fileBrowserBackground_ = null;

    /**
     * The root DOM element of this app.
     * @private @type {?HTMLBodyElement}
     */
    this.dialogDom_ = null;

    /**
     * The document object of this app.
     * @private @type {?Document}
     */
    this.document_ = null;

    // ------------------------------------------------------------------------
    // Miscellaneous FileManager's states.

    /**
     * Promise object which is fulfilled when initialization for app state
     * controller is done.
     * @private @type {?Promise<void>}
     */
    this.initSettingsPromise_ = null;

    /**
     * Promise object which is fulfilled when initialization related to the
     * background page is done.
     * @private @type {?Promise<void>}
     */
    this.initBackgroundPagePromise_ = null;

    /**
     * Whether Drive is enabled. Retrieved from user preferences.
     * @private @type {boolean}
     */
    this.driveEnabled_ = false;

    /**
     * Whether Drive bulk-pinning is available on this device. Retrieved from
     * user preferences.
     * @private @type {boolean}
     */
    this.bulkPinningAvailable_ = false;

    /**
     * Whether Drive bulk-pinning has been initialized in Files App.
     * @private @type {boolean}
     */
    this.bulkPinningInitialized_ = false;

    /**
     * A fake Drive placeholder item.
     * @private @type {?NavigationModelFakeItem}
     */
    this.fakeDriveItem_ = null;

    /**
     * Whether Trash is enabled or not, retrieved from user preferences.
     * @type {boolean}
     */
    this.trashEnabled = false;

    /**
     * A fake Trash placeholder item.
     * @private @type {?NavigationModelFakeItem}
     */
    this.fakeTrashItem_ = null;

    /**
     * A fake entry for Recents.
     * @private @type {?FakeEntry}
     */
    this.recentEntry_ = null;

    /**
     * Whether or not we are running in guest mode.
     * @private @type {boolean}
     */
    this.guestMode_ = false;

    /** @private @type {!Store} */
    this.store_ = getStore();

    (function() {
      ColorChangeUpdater.forDocument().start();
    })();
  }

  /**
   * @return {!ProgressCenter}
   */
  get progressCenter() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return assert(this.fileBrowserBackground_.progressCenter);
  }

  /**
   * @return {DirectoryModel}
   */
  get directoryModel() {
    // @ts-ignore: error TS2322: Type 'DirectoryModel | null' is not assignable
    // to type 'DirectoryModel'.
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
    // @ts-ignore: error TS2322: Type 'FileFilter | null' is not assignable to
    // type 'FileFilter'.
    return this.fileFilter_;
  }

  /**
   * @return {FolderShortcutsDataModel}
   */
  get folderShortcutsModel() {
    // @ts-ignore: error TS2322: Type 'FolderShortcutsDataModel | null' is not
    // assignable to type 'FolderShortcutsDataModel'.
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
    // @ts-ignore: error TS2322: Type 'ProvidersModel | null' is not assignable
    // to type 'ProvidersModel'.
    return this.providersModel_;
  }

  /**
   * @return {MetadataModel}
   */
  get metadataModel() {
    // @ts-ignore: error TS2322: Type 'MetadataModel | null' is not assignable
    // to type 'MetadataModel'.
    return this.metadataModel_;
  }

  /**
   * @return {FileSelectionHandler}
   */
  get selectionHandler() {
    // @ts-ignore: error TS2322: Type 'FileSelectionHandler | null' is not
    // assignable to type 'FileSelectionHandler'.
    return this.selectionHandler_;
  }

  /**
   * @return {Document}
   */
  get document() {
    // @ts-ignore: error TS2322: Type 'Document | null' is not assignable to
    // type 'Document'.
    return this.document_;
  }

  /**
   * @return {FileTransferController}
   */
  get fileTransferController() {
    // @ts-ignore: error TS2322: Type 'FileTransferController | null' is not
    // assignable to type 'FileTransferController'.
    return this.fileTransferController_;
  }

  /**
   * @return {NamingController}
   */
  get namingController() {
    // @ts-ignore: error TS2322: Type 'NamingController | null' is not
    // assignable to type 'NamingController'.
    return this.namingController_;
  }

  /**
   * @return {TaskController}
   */
  get taskController() {
    // @ts-ignore: error TS2322: Type 'TaskController | null' is not assignable
    // to type 'TaskController'.
    return this.taskController_;
  }

  /**
   * @return {SpinnerController}
   */
  get spinnerController() {
    // @ts-ignore: error TS2322: Type 'SpinnerController | null' is not
    // assignable to type 'SpinnerController'.
    return this.spinnerController_;
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
    // @ts-ignore: error TS2322: Type 'Crostini | null' is not assignable to
    // type 'Crostini'.
    return this.crostini_;
  }

  /**
   * @return {FileManagerUI}
   */
  get ui() {
    // @ts-ignore: error TS2322: Type 'FileManagerUI | null' is not assignable
    // to type 'FileManagerUI'.
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
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.fileBrowserBackground_.launchFileManager(appState);
  }

  /**
   * Returns the last URL visited with visitURL() (e.g. for "Manage in Drive").
   * Used by the integration tests.
   * @return {string}
   */
  getLastVisitedURL() {
    return getLastVisitedURL();
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
  async setGuestMode_() {
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
   * @return {!Promise<void>}
   * @private
   */
  async initFileSystemUI_() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.listContainer.startBatchUpdates();

    const fileListPromise = this.initFileList_();
    const currentDirectoryPromise = this.setupCurrentDirectory_();

    const self = this;

    // @ts-ignore: error TS7034: Variable 'listBeingUpdated' implicitly has type
    // 'any' in some locations where its type cannot be determined.
    let listBeingUpdated = null;
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.directoryModel_.addEventListener('begin-update-files', () => {
      // @ts-ignore: error TS2339: Property 'startBatchUpdates' does not exist
      // on type 'List'.
      self.ui_.listContainer.currentList.startBatchUpdates();
      // Remember the list which was used when updating files started, so
      // endBatchUpdates() is called on the same list.
      // @ts-ignore: error TS18047: 'self.ui_' is possibly 'null'.
      listBeingUpdated = self.ui_.listContainer.currentList;
    });
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.directoryModel_.addEventListener('end-update-files', () => {
      // @ts-ignore: error TS18047: 'self.namingController_' is possibly 'null'.
      self.namingController_.restoreItemBeingRenamed();
      // @ts-ignore: error TS7005: Variable 'listBeingUpdated' implicitly has an
      // 'any' type.
      listBeingUpdated.endBatchUpdates();
      listBeingUpdated = null;
    });
    this.volumeManager_.addEventListener(ARCHIVE_OPENED_EVENT_TYPE, event => {
      // @ts-ignore: error TS2339: Property 'detail' does not exist on type
      // 'Event'.
      assert(event.detail.mountPoint);
      // @ts-ignore: error TS2339: Property 'isFocused' does not exist on
      // type 'Window & typeof globalThis'.
      if (window.isFocused()) {
        // @ts-ignore: error TS2339: Property 'detail' does not exist on
        // type 'Event'.
        this.directoryModel_.changeDirectoryEntry(event.detail.mountPoint);
      }
    });

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.directoryModel_.addEventListener('directory-changed', event => {
      const
          customEvent = /**
                           @type {import('../../definitions/directory_change_event.js').DirectoryChangeEvent}
                             */
          (event);
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.navigationUma_.onDirectoryChanged(customEvent.detail.newDirEntry);
    });

    this.initCommands_();

    assert(this.directoryModel_);
    assert(this.spinnerController_);
    assert(this.commandHandler_);
    assert(this.selectionHandler_);
    assert(this.launchParams_);
    assert(this.volumeManager_);
    assert(this.dialogDom_);

    // @ts-ignore: error TS2322: Type 'MetadataModel | null' is not assignable
    // to type 'Object'.
    this.fileBrowserBackground_.driveSyncHandler.metadataModel =
        assert(this.metadataModel_);
    this.scanController_ = new ScanController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.directoryModel_, this.ui_.listContainer, this.spinnerController_,
        this.selectionHandler_);
    this.sortMenuController_ = new SortMenuController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.sortButton, assert(this.directoryModel_.getFileList()));
    this.gearMenuController_ = new GearMenuController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.gearButton, this.ui_.gearMenu, this.ui_.providersMenu,
        // @ts-ignore: error TS2345: Argument of type 'DirectoryModel | null' is
        // not assignable to parameter of type 'DirectoryModel'.
        this.directoryModel_, this.commandHandler_,
        assert(this.providersModel_));
    this.selectionMenuController_ = new SelectionMenuController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.selectionMenuButton,
        // @ts-ignore: error TS2345: Argument of type '(arg0?: Object |
        // undefined) => Element' is not assignable to parameter of type 'new
        // (...args: any) => Menu'.
        queryDecoratedElement('#file-context-menu', Menu));
    this.toolbarController_ = new ToolbarController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.toolbar, this.ui_.dialogNavigationList, this.ui_.listContainer,
        this.selectionHandler_, this.directoryModel_, this.volumeManager_,
        /** @type {!A11yAnnounce} */ (this.ui_));
    this.actionsController_ = new ActionsController(
        // @ts-ignore: error TS2345: Argument of type 'MetadataModel | null' is
        // not assignable to parameter of type 'MetadataModel'.
        this.volumeManager_, assert(this.metadataModel_), this.directoryModel_,
        assert(this.folderShortcutsModel_), this.selectionHandler_,
        assert(this.ui_));
    this.lastModifiedController_ = new LastModifiedController(
        // @ts-ignore: error TS2345: Argument of type 'DirectoryModel | null' is
        // not assignable to parameter of type 'DirectoryModel'.
        this.ui_.listContainer.table, this.directoryModel_);

    this.quickViewModel_ = new QuickViewModel();
    const fileListSelectionModel = /** @type {!FileListSelectionModel} */ (
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.directoryModel_.getFileListSelection());
    this.quickViewUma_ =
        new QuickViewUma(assert(this.volumeManager_), assert(this.dialogType));
    const metadataBoxController = new MetadataBoxController(
        // @ts-ignore: error TS2345: Argument of type 'MetadataModel | null' is
        // not assignable to parameter of type 'MetadataModel'.
        this.metadataModel_, this.quickViewModel_, this.fileMetadataFormatter_,
        assert(this.volumeManager_));
    this.quickViewController_ = new QuickViewController(
        // @ts-ignore: error TS2345: Argument of type 'MetadataModel | null' is
        // not assignable to parameter of type 'MetadataModel'.
        this, assert(this.metadataModel_), assert(this.selectionHandler_),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
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
        // @ts-ignore: error TS2345: Argument of type 'FileManagerUI | null' is
        // not assignable to parameter of type 'FileManagerUI'.
        this.dialogType, this.ui_, this.volumeManager_, this.directoryModel_,
        this.selectionHandler_, this.namingController_,
        this.appStateController_, this.taskController_);

    this.initDataTransferOperations_();
    fileListPromise.then(() => {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.taskController_.setFileTransferController(
          // @ts-ignore: error TS2345: Argument of type 'FileTransferController
          // | null' is not assignable to parameter of type
          // 'FileTransferController'.
          this.fileTransferController_);
    });

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.selectionHandler_.onFileSelectionChanged();
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.listContainer.endBatchUpdates();

    const bannerController = new BannerController(
        // @ts-ignore: error TS2345: Argument of type 'DirectoryModel | null' is
        // not assignable to parameter of type 'DirectoryModel'.
        this.directoryModel_, this.volumeManager_, assert(this.crostini_),
        this.dialogType);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.initBanners(bannerController);
    bannerController.initialize();

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.attachFilesTooltip();
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.decorateFilesMenuItems();
    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
   * @private
   */
  async initBulkPinning_() {
    try {
      const promise = getBulkPinProgress();

      if (!this.bulkPinningInitialized_) {
        // @ts-ignore: error TS7006: Parameter 'progress' implicitly has an
        // 'any' type.
        chrome.fileManagerPrivate.onBulkPinProgress.addListener((progress) => {
          console.debug('Got bulk-pinning event:', progress);
          this.store_.dispatch(updateBulkPinProgress(progress));
        });

        this.bulkPinningInitialized_ = true;
      }

      const progress = await promise;
      if (progress) {
        console.debug('Got initial bulk-pinning state:', progress);
        this.store_.dispatch(updateBulkPinProgress(progress));
      }
    } catch (e) {
      console.warn('Cannot get initial bulk-pinning state:', e);
    }
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
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        assert(this.document_), assert(this.ui_.listContainer),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        assert(this.ui_.directoryTree),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.showConfirmationDialog.bind(this.ui_), this.progressCenter,
        assert(this.metadataModel_), assert(this.directoryModel_),
        assert(this.volumeManager_),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        assert(this.selectionHandler_), this.ui_.toast);
  }

  /**
   * One-time initialization of commands.
   * @private
   */
  initCommands_() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    assert(this.ui_.textContextMenu);

    this.commandHandler_ =
        // @ts-ignore: error TS2345: Argument of type 'FileSelectionHandler |
        // null' is not assignable to parameter of type 'FileSelectionHandler'.
        new CommandHandler(this, assert(this.selectionHandler_));

    // TODO(hirono): Move the following block to the UI part.
    // Hook up the cr-button commands.
    // @ts-ignore: error TS2488: Type 'NodeListOf<Element>' must have a
    // '[Symbol.iterator]()' method that returns an iterator.
    for (const crButton of this.dialogDom_.querySelectorAll(
             'cr-button[command]')) {
      CommandButton.decorate(/** @type {CrButtonElement} */ (crButton));
    }

    // @ts-ignore: error TS2488: Type 'NodeListOf<Element>' must have a
    // '[Symbol.iterator]()' method that returns an iterator.
    for (const input of this.getDomInputs_()) {
      this.setContextMenuForInput_(input);
    }

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.setContextMenuForInput_(this.ui_.listContainer.renameInput);
    this.setContextMenuForInput_(
        this.directoryTreeNamingController_.getInputElement());
  }

  /**
   * Get input elements from root DOM element of this app.
   * @private
   */
  getDomInputs_() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.dialogDom_.querySelectorAll(
        'input[type=text], input[type=search], textarea, cr-input');
  }

  /**
   * Set context menu and handlers for an input element.
   * @private
   */
  // @ts-ignore: error TS7006: Parameter 'input' implicitly has an 'any' type.
  setContextMenuForInput_(input) {
    let touchInduced = false;

    // stop contextmenu propagation for touch-induced events.
    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    input.addEventListener('touchstart', (e) => {
      touchInduced = true;
    });
    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    input.addEventListener('contextmenu', (e) => {
      if (touchInduced) {
        e.stopImmediatePropagation();
      }
      touchInduced = false;
    });
    // @ts-ignore: error TS7006: Parameter 'e' implicitly has an 'any' type.
    input.addEventListener('click', (e) => {
      touchInduced = false;
    });

    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
      // @ts-ignore: error TS2339: Property 'keyCode' does not exist on type
      // 'Event'.
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
  // @ts-ignore: error TS7006: Parameter 'dialogDom' implicitly has an 'any'
  // type.
  async initializeUI(dialogDom) {
    console.warn(`Files app starting up: ${this.dialogType}`);
    this.dialogDom_ = dialogDom;
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.document_ = this.dialogDom_.ownerDocument;

    startInterval('Load.InitDocuments');
    // importElements depend on loadTimeData which is initialized in the
    // initBackgroundPagePromise_.
    await this.initBackgroundPagePromise_;
    await importElements();
    recordInterval('Load.InitDocuments');

    startInterval('Load.InitUI');
    this.document_.documentElement.classList.add('files-ng');
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.dialogDom_.classList.add('files-ng');

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
   * @private
   */
  initGeneral_() {
    // Initialize the application state.
    // TODO(mtomasz): Unify window.appState with location.search format.
    // @ts-ignore: error TS2339: Property 'appState' does not exist on type
    // 'Window & typeof globalThis'.
    if (window.appState) {
      const params = {};

      // @ts-ignore: error TS2339: Property 'appState' does not exist on type
      // 'Window & typeof globalThis'.
      for (const name in window.appState) {
        // @ts-ignore: error TS2339: Property 'appState' does not exist on type
        // 'Window & typeof globalThis'.
        params[name] = window.appState[name];
      }

      // @ts-ignore: error TS2345: Argument of type '{}' is not assignable to
      // parameter of type 'FilesAppState'.
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
      // @ts-ignore: error TS2345: Argument of type '{}' is not assignable to
      // parameter of type 'FilesAppState'.
      this.launchParams_ = new LaunchParam(json);
    }
    this.store_.dispatch(
        setLaunchParameters({dialogType: this.launchParams_.type}));

    // Initialize the member variables that depend this.launchParams_.
    this.dialogType = this.launchParams_.type;
  }

  /**
   * Initializes the background page.
   * @return {!Promise<void>}
   * @private
   */
  async startInitBackgroundPage_() {
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
   * @private
   */
  async initVolumeManager_() {
    const allowedPaths = this.getAllowedPaths_();
    const writableOnly =
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.launchParams_.type === DialogType.SELECT_SAVEAS_FILE;
    const disabledVolumes =
        /** @type {!Array<!VolumeType>} */ (await this.getDisabledVolumes_());

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
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.fileBrowserBackground_.getVolumeManager(),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.launchParams_.volumeFilter, disabledVolumes);

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    await this.fileBrowserBackground_.getVolumeManager();
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
    this.ui_ = new FileManagerUI(
        // @ts-ignore: error TS2345: Argument of type 'HTMLBodyElement | null'
        // is not assignable to parameter of type 'HTMLElement'.
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

    // @ts-ignore: error TS2345: Argument of type 'HTMLBodyElement | null' is
    // not assignable to parameter of type 'Document | Element | HTMLElement |
    // DocumentFragment | undefined'.
    const table = queryRequiredElement('.detail-table', dom);
    FileTable.decorate(
        // @ts-ignore: error TS2345: Argument of type 'MetadataModel | null' is
        // not assignable to parameter of type 'MetadataModel'.
        table, this.metadataModel_, this.volumeManager_,
        /** @type {!A11yAnnounce} */ (this.ui_),
        this.dialogType == DialogType.FULL_PAGE);
    // @ts-ignore: error TS2345: Argument of type 'HTMLBodyElement | null' is
    // not assignable to parameter of type 'Document | Element | HTMLElement |
    // DocumentFragment | undefined'.
    const grid = queryRequiredElement('.thumbnail-grid', dom);
    FileGrid.decorate(
        // @ts-ignore: error TS2345: Argument of type 'MetadataModel | null' is
        // not assignable to parameter of type 'MetadataModel'.
        grid, this.metadataModel_, this.volumeManager_,
        /** @type {!A11yAnnounce} */ (this.ui_));

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.initAdditionalUI(
        assertInstanceof(table, FileTable), assertInstanceof(grid, FileGrid),
        this.volumeManager_);

    // Handle UI events.
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.progressCenter.addPanel(this.ui_.progressCenterPanel);

    // Arrange the file list.
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.listContainer.table.normalizeColumns();
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.listContainer.table.redraw();
  }

  /**
   * One-time initialization of focus. This should run at the last of UI
   *  initialization.
   * @private
   */
  initUIFocus_() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
    assert(this.metadataModel_);
    this.directoryModel_ = new DirectoryModel(
        // @ts-ignore: error TS2345: Argument of type 'FileFilter | null' is not
        // assignable to parameter of type 'FileFilter'.
        singleSelection, this.fileFilter_, this.metadataModel_,
        this.volumeManager_);

    this.folderShortcutsModel_ =
        new FolderShortcutsDataModel(this.volumeManager_);

    this.androidAppListModel_ = new AndroidAppListModel(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.launchParams_.showAndroidPickerApps,
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.launchParams_.includeAllFiles, this.launchParams_.typeList);

    this.recentEntry_ = new FakeEntryImpl(
        str('RECENT_ROOT_LABEL'), RootType.RECENT, this.getSourceRestriction_(),
        chrome.fileManagerPrivate.FileCategory.ALL);
    // @ts-ignore: error TS2741: Property 'getUIChildren' is missing in type
    // 'FakeEntry' but required in type 'FakeEntryImpl'.
    this.store_.dispatch(addUiEntry({entry: this.recentEntry_}));
    assert(this.launchParams_);
    this.selectionHandler_ = new FileSelectionHandler(
        assert(this.directoryModel_),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        assert(this.ui_.listContainer), assert(this.metadataModel_),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        assert(this.volumeManager_), this.launchParams_.allowedPaths);

    // TODO(mtomasz, yoshiki): Create navigation list earlier, and here just
    // attach the directory model.
    const directoryTreePromise = this.initDirectoryTree_();

    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.listContainer.listThumbnailLoader = new ListThumbnailLoader(
        // @ts-ignore: error TS2345: Argument of type 'ThumbnailModel | null' is
        // not assignable to parameter of type 'ThumbnailModel'.
        this.directoryModel_, assert(this.thumbnailModel_),
        this.volumeManager_);
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.listContainer.dataModel = this.directoryModel_.getFileList();
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.listContainer.emptyDataModel =
        this.directoryModel_.getEmptyFileList();
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.ui_.listContainer.selectionModel =
        this.directoryModel_.getFileListSelection();

    // @ts-ignore: error TS2345: Argument of type 'FileManagerUI | null' is not
    // assignable to parameter of type 'FileManagerUI'.
    this.appStateController_.initialize(this.ui_, this.directoryModel_);

    // Create metadata update controller.
    this.metadataUpdateController_ = new MetadataUpdateController(
        // @ts-ignore: error TS2345: Argument of type 'MetadataModel | null' is
        // not assignable to parameter of type 'MetadataModel'.
        this.ui_.listContainer, this.directoryModel_, this.metadataModel_,
        this.fileMetadataFormatter_);

    // Create naming controller.
    this.namingController_ = new NamingController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.listContainer, assert(this.ui_.alertDialog),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        assert(this.ui_.confirmDialog), this.directoryModel_,
        assert(this.fileFilter_), this.selectionHandler_);

    // Create task controller.
    this.taskController_ = new TaskController(
        // @ts-ignore: error TS2345: Argument of type 'FileManagerUI | null' is
        // not assignable to parameter of type 'FileManagerUI'.
        this.volumeManager_, this.ui_, this.metadataModel_,
        this.directoryModel_, this.selectionHandler_,
        this.metadataUpdateController_, assert(this.crostini_),
        this.progressCenter);

    // Create directory tree naming controller.
    this.directoryTreeNamingController_ = new DirectoryTreeNamingController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.directoryModel_, assert(this.ui_.directoryTree),
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.directoryTreeContainer, this.ui_.alertDialog);


    // Create spinner controller.
    this.spinnerController_ =
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        new SpinnerController(this.ui_.listContainer.spinner);
    this.spinnerController_.blink();

    // Create dialog action controller.
    this.dialogActionController_ = new DialogActionController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.dialogType, this.ui_.dialogFooter, this.directoryModel_,
        // @ts-ignore: error TS2345: Argument of type 'FileFilter | null' is not
        // assignable to parameter of type 'FileFilter'.
        this.volumeManager_, this.fileFilter_, this.namingController_,
        this.selectionHandler_, this.launchParams_);

    // Create file-type filter controller.
    this.fileTypeFiltersController_ = new FileTypeFiltersController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.fileTypeFilterContainer, this.directoryModel_,
        this.recentEntry_, /** @type {!A11yAnnounce} */ (this.ui_));
    this.emptyFolderController_ = new EmptyFolderController(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.ui_.emptyFolder, this.directoryModel_,
        // @ts-ignore: error TS2345: Argument of type 'ProvidersModel | null' is
        // not assignable to parameter of type 'ProvidersModel'.
        assert(this.providersModel_), this.recentEntry_);


    return directoryTreePromise;
  }

  /**
   * Based on the dialog type and dialog caller, sets the list of volumes
   * that should be disabled according to Data Leak Prevention rules.
   * @return {Promise<!Array<!VolumeType>>}
   */
  async getDisabledVolumes_() {
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
      disabledVolumes.push(
          /** @type {!VolumeType }*/ (c));
    }
    return disabledVolumes;
  }

  /**
   * @return {!Promise<void>}
   * @private
   */
  async initDirectoryTree_() {
    this.navigationUma_ = new NavigationUma(assert(this.volumeManager_));

    const directoryTree = /** @type {DirectoryTree} */
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        (this.dialogDom_.querySelector('#directory-tree'));

    if (isNewDirectoryTreeEnabled()) {
      const treeContainer = directoryTree.parentElement;
      directoryTree.remove();
      const directoryTreeContainer = new DirectoryTreeContainer(
          // @ts-ignore: error TS2345: Argument of type 'HTMLElement | null' is
          // not assignable to parameter of type 'HTMLElement'.
          treeContainer, this.directoryModel_, this.volumeManager_);
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.ui_.initDirectoryTree(directoryTreeContainer);
    } else {
      const fakeEntriesVisible =
          this.dialogType !== DialogType.SELECT_SAVEAS_FILE;

      DirectoryTree.decorate(
          // @ts-ignore: error TS2345: Argument of type 'DirectoryModel | null'
          // is not assignable to parameter of type 'DirectoryModel'.
          directoryTree, assert(this.directoryModel_),
          assert(this.volumeManager_), assert(this.metadataModel_),
          fakeEntriesVisible);

      directoryTree.dataModel = new NavigationListModel(
          // @ts-ignore: error TS2345: Argument of type
          // 'FolderShortcutsDataModel | null' is not assignable to parameter of
          // type 'FolderShortcutsDataModel'.
          assert(this.volumeManager_), assert(this.folderShortcutsModel_),
          // @ts-ignore: error TS2531: Object is possibly 'null'.
          fakeEntriesVisible && !isFolderDialogType(this.launchParams_.type) ?
              new NavigationModelFakeItem(
                  str('RECENT_ROOT_LABEL'), NavigationModelItemType.RECENT,
                  // @ts-ignore: error TS2345: Argument of type 'FakeEntry |
                  // null' is not assignable to parameter of type
                  // 'FilesAppEntry'.
                  assert(this.recentEntry_)) :
              null,
          assert(this.directoryModel_), assert(this.androidAppListModel_),
          this.dialogType);
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.ui_.initDirectoryTree(directoryTree);
    }

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
    this.crostiniController_ = new CrostiniController(
        // @ts-ignore: error TS2345: Argument of type 'Crostini | null' is not
        // assignable to parameter of type 'Crostini'.
        assert(this.crostini_), assert(this.directoryModel_),
        // TODO(b/285977941): `DirectoryTree` is only used when FileExperimental
        // flag is off, remove it after the tree replacement.
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        assert(/** @type {DirectoryTree} */ (this.ui_.directoryTree)),
        this.volumeManager_.isDisabled(VolumeType.CROSTINI));
    await this.crostiniController_.redraw();
    // Never show toast in an open-file dialog.
    const maybeShowToast = this.dialogType === DialogType.FULL_PAGE;
    await this.crostiniController_.loadSharedPaths(
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        maybeShowToast, this.ui_.toast);

    if (isGuestOsEnabled()) {
      this.guestOsController_ = new GuestOsController(
          // @ts-ignore: error TS2345: Argument of type 'DirectoryModel | null'
          // is not assignable to parameter of type 'DirectoryModel'.
          assert(this.directoryModel_),
          // TODO(b/285977941): `DirectoryTree` is only used when
          // FileExperimental flag is off, remove it after the tree replacement.
          // @ts-ignore: error TS2531: Object is possibly 'null'.
          assert(/** @type {DirectoryTree} */ (this.ui_.directoryTree)),
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
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.crostini_.setEnabled(event.vmName, event.containerName, true);
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        return this.crostiniController_.redraw();

      case chrome.fileManagerPrivate.CrostiniEventType.DISABLE:
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.crostini_.setEnabled(event.vmName, event.containerName, false);
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        return this.crostiniController_.redraw();

      // Event is sent when a user drops an unshared file on Plugin VM.
      // We show the move dialog so the user can move the file or share the
      // directory.
      case chrome.fileManagerPrivate.CrostiniEventType
          .DROP_FAILED_PLUGIN_VM_DIRECTORY_NOT_SHARED:
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        if (this.ui_.dragInProcess) {
          const moveMessage =
              str('UNABLE_TO_DROP_IN_PLUGIN_VM_DIRECTORY_NOT_SHARED_MESSAGE');
          const copyMessage =
              str('UNABLE_TO_DROP_IN_PLUGIN_VM_EXTERNAL_DRIVE_MESSAGE');
          FileTasks.showPluginVmNotSharedDialog(
              this.selectionHandler.selection.entries, this.volumeManager_,
              // @ts-ignore: error TS2345: Argument of type 'MetadataModel |
              // null' is not assignable to parameter of type 'MetadataModel'.
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
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const tracker = this.directoryModel_.createDirectoryChangeTracker();
    tracker.start();

    // Wait until the volume manager is initialized.
    await new Promise(
        // @ts-ignore: error TS2345: Argument of type '(value: any) => void' is
        // not assignable to parameter of type '() => any'.
        resolve => this.volumeManager_.ensureInitialized(resolve));

    // @ts-ignore: error TS7034: Variable 'nextCurrentDirEntry' implicitly has
    // type 'any' in some locations where its type cannot be determined.
    let nextCurrentDirEntry;
    // @ts-ignore: error TS7034: Variable 'selectionEntry' implicitly has type
    // 'any' in some locations where its type cannot be determined.
    let selectionEntry;

    // Resolve the selectionURL to selectionEntry or to currentDirectoryEntry in
    // case of being a display root or a default directory to open files.
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    if (this.launchParams_.selectionURL) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      if (this.launchParams_.selectionURL == this.recentEntry_.toURL()) {
        nextCurrentDirEntry = this.recentEntry_;
      } else {
        try {
          const inEntry = await new Promise((resolve, reject) => {
            window.webkitResolveLocalFileSystemURL(
                // @ts-ignore: error TS2531: Object is possibly 'null'.
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
          // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
          if (error.name !== 'NotFoundError') {
            // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
            console.warn(error.stack || error);
          }
        }
      }
    }

    // If searchQuery param is set, find the first directory that matches the
    // query, and select it if exists.
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    const searchQuery = this.launchParams_.searchQuery;
    if (searchQuery) {
      startInterval('Load.ProcessInitialSearchQuery');
      if (!isNewDirectoryTreeEnabled()) {
        this.store_.dispatch(updateSearch({
          query: searchQuery,
          status: PropStatus.STARTED,
          options: getDefaultSearchOptions(),
        }));
      }
      // Show a spinner, as the crossover search function call could be slow.
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      const hideSpinnerCallback = this.spinnerController_.show();
      const queryMatchedDirEntry = await findQueryMatchedDirectoryEntry(
          // @ts-ignore: error TS2345: Argument of type 'DirectoryModel |
          // null' is not assignable to parameter of type 'DirectoryModel'.
          this.volumeManager_, this.directoryModel_, searchQuery);
      if (queryMatchedDirEntry) {
        nextCurrentDirEntry = queryMatchedDirEntry;
      }
      hideSpinnerCallback();
      recordInterval('Load.ProcessInitialSearchQuery');
    }

    // Resolve the currentDirectoryURL to currentDirectoryEntry (if not done by
    // the previous step).
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    if (!nextCurrentDirEntry && this.launchParams_.currentDirectoryURL) {
      try {
        const inEntry = await new Promise((resolve, reject) => {
          window.webkitResolveLocalFileSystemURL(
              // @ts-ignore: error TS2531: Object is possibly 'null'.
              this.launchParams_.currentDirectoryURL, resolve, reject);
        });
        const locationInfo = this.volumeManager_.getLocationInfo(inEntry);
        if (locationInfo) {
          nextCurrentDirEntry = inEntry;
        }
      } catch (error) {
        // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
        console.warn(error.stack || error);
      }
    }

    // If the directory to be changed to is not available, then first fallback
    // to the parent of the selection entry.
    if (!nextCurrentDirEntry && selectionEntry) {
      nextCurrentDirEntry = await new Promise(resolve => {
        // @ts-ignore: error TS7005: Variable 'selectionEntry' implicitly has an
        // 'any' type.
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
            locationInfo.rootType === RootType.DRIVE_SHARED_WITH_ME) {
          const volumeInfo =
              this.volumeManager_.getVolumeInfo(nextCurrentDirEntry);
          if (!volumeInfo) {
            nextCurrentDirEntry = null;
          } else {
            try {
              nextCurrentDirEntry = await volumeInfo.resolveDisplayRoot();
            } catch (error) {
              // @ts-ignore: error TS18046: 'error' is of type 'unknown'.
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
      nextCurrentDirEntry = await new Promise(resolve => {
        this.volumeManager_.getDefaultDisplayRoot(resolve);
      });
    }

    // If selection failed to be resolved (eg. didn't exist, in case of saving a
    // file, or in case of a fallback of the current directory, then try to
    // resolve again using the target name.
    if (!selectionEntry && nextCurrentDirEntry &&
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        this.launchParams_.targetName) {
      // Try to resolve as a file first. If it fails, then as a directory.
      try {
        selectionEntry = await new Promise((resolve, reject) => {
          // @ts-ignore: error TS7005: Variable 'nextCurrentDirEntry' implicitly
          // has an 'any' type.
          nextCurrentDirEntry.getFile(
              // @ts-ignore: error TS2531: Object is possibly 'null'.
              this.launchParams_.targetName, {}, resolve, reject);
        });
      } catch (error1) {
        // Failed to resolve as a file. Try to resolve as a directory.
        try {
          selectionEntry = await new Promise((resolve, reject) => {
            // @ts-ignore: error TS7005: Variable 'nextCurrentDirEntry'
            // implicitly has an 'any' type.
            nextCurrentDirEntry.getDirectory(
                // @ts-ignore: error TS2531: Object is possibly 'null'.
                this.launchParams_.targetName, {}, resolve, reject);
          });
        } catch (error2) {
          // If `targetName` doesn't exist we just don't select it, thus we
          // don't need to log the failure.
          // @ts-ignore: error TS18046: 'error1' is of type 'unknown'.
          if (error1.name !== 'NotFoundError') {
            // @ts-ignore: error TS18046: 'error1' is of type 'unknown'.
            console.warn(error1.stack || error1);
            console.log(error1);
          }
          // @ts-ignore: error TS18046: 'error2' is of type 'unknown'.
          if (error2.name !== 'NotFoundError') {
            // @ts-ignore: error TS18046: 'error2' is of type 'unknown'.
            console.warn(error2.stack || error2);
          }
        }
      }
    }

    // If there is no target select MyFiles by default.
    if (!nextCurrentDirEntry) {
      if (isNewDirectoryTreeEnabled()) {
        const myFiles = getMyFiles(this.store_.getState());
        // When MyFiles volume is mounted, we rely on the current directory
        // change to make it as selected (controlled by DirectoryModel),
        // that's why we can't set MyFiles entry list as the current directory
        // here.
        // TODO(b/308504417): MyFiles entry list should be selected before
        // MyFiles volume mounts.
        if (myFiles.myFilesVolume) {
          nextCurrentDirEntry = myFiles.myFilesEntry;
        }
        // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
        // 'XfTree | DirectoryTree'.
      } else if (this.ui_.directoryTree.dataModel.myFilesModel_) {
        nextCurrentDirEntry =
            // @ts-ignore: error TS2339: Property 'dataModel' does not exist on
            // type 'XfTree | DirectoryTree'.
            this.ui_.directoryTree.dataModel.myFilesModel_.entry;
      }
    }

    // Check directory change.
    tracker.stop();
    if (!tracker.hasChanged) {
      // Finish setup current directory.
      await this.finishSetupCurrentDirectory_(
          // @ts-ignore: error TS2531: Object is possibly 'null'.
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
          // @ts-ignore: error TS2531: Object is possibly 'null'.
          this.directoryModel_.changeDirectoryEntry(
              // @ts-ignore: error TS2345: Argument of type '(value: any) =>
              // void' is not assignable to parameter of type '() => any'.
              assert(directoryEntry), resolve);
        });
        if (opt_selectionEntry) {
          // @ts-ignore: error TS2531: Object is possibly 'null'.
          this.directoryModel_.selectEntry(opt_selectionEntry);
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
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.ui_.addLoadedAttribute();
    })();

    if (this.dialogType === DialogType.SELECT_SAVEAS_FILE) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.ui_.dialogFooter.filenameInput.value = opt_suggestedName || '';
      // @ts-ignore: error TS2531: Object is possibly 'null'.
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
    // @ts-ignore: error TS2322: Type 'FileSystemDirectoryEntry |
    // FilesAppDirEntry | FakeEntry | null' is not assignable to type
    // 'FileSystemDirectoryEntry | FilesAppDirEntry | FakeEntry'.
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
        // @ts-ignore: error TS18047: 'item' is possibly 'null'.
        item.message = '';
        // @ts-ignore: error TS18047: 'item' is possibly 'null'.
        item.state = ProgressItemState.CANCELED;
        // @ts-ignore: error TS2345: Argument of type 'ProgressCenterItem |
        // null' is not assignable to parameter of type 'ProgressCenterItem'.
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
   * @returns {AllowedPaths}
   */
  getAllowedPaths_() {
    let allowedPaths =
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        /** @type {AllowedPaths} */ (this.launchParams_.allowedPaths);
    // The native implementation of the Files app creates snapshot files for
    // non-native files. But it does not work for folders (e.g., dialog for
    // loading unpacked extensions).
    if (allowedPaths === AllowedPaths.NATIVE_PATH &&
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        !isFolderDialogType(this.launchParams_.type)) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
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
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    return this.selectionHandler_.selection;
  }

  /**
   * @return {ArrayDataModel} File list.
   */
  getFileList() {
    // @ts-ignore: error TS2531: Object is possibly 'null'.
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
      console.error('Cannot get preferences:', e);
      return;
    }

    this.store_.dispatch(updatePreferences(prefs));

    let redraw = false;
    if (this.driveEnabled_ !== prefs.driveEnabled) {
      this.driveEnabled_ = prefs.driveEnabled;
      this.toggleDriveRootOnPreferencesUpdate_();
      redraw = true;
    }

    if (this.bulkPinningAvailable_ !== prefs.driveFsBulkPinningAvailable) {
      this.bulkPinningAvailable_ = prefs.driveFsBulkPinningAvailable;
      console.debug(`Bulk-pinning is now ${
          this.bulkPinningAvailable_ ? 'available' : 'unavailable'}`);
      if (this.bulkPinningAvailable_) {
        await this.initBulkPinning_();
      }
    }

    if (this.trashEnabled !== prefs.trashEnabled) {
      this.trashEnabled = prefs.trashEnabled;
      this.toggleTrashRootOnPreferencesUpdate_();
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.toolbarController_.moveToTrashCommand.disabled = !this.trashEnabled;
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.toolbarController_.moveToTrashCommand.canExecuteChange(
          // @ts-ignore: error TS2531: Object is possibly 'null'.
          this.ui_.listContainer.currentList);
      redraw = true;
    }

    await this.updateOfficePrefs_(prefs);

    if (redraw && !isNewDirectoryTreeEnabled()) {
      // @ts-ignore: error TS2339: Property 'redraw' does not exist on type
      // 'XfTree | DirectoryTree'.
      this.ui_.directoryTree.redraw(false);
    }
  }

  async onDriveConnectionStatusChanged_() {
    let connectionState = null;
    try {
      connectionState = await getDriveConnectionState();
    } catch (e) {
      console.error('Failed to retrieve drive connection state:', e);
      return;
    }
    this.store_.dispatch(updateDriveConnectionStatus(connectionState));
  }

  /**
   * @param {!chrome.fileManagerPrivate.Preferences} prefs
   * @private
   */
  async updateOfficePrefs_(prefs) {
    // These prefs starts with value 0. We only want to display when they're
    // non-zero and show the most recent (larger value).
    if (prefs.officeFileMovedOneDrive > prefs.officeFileMovedGoogleDrive) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.ui_.nudgeContainer.showNudge(
          NudgeType['ONE_DRIVE_MOVED_FILE_NUDGE']);
    } else if (
        prefs.officeFileMovedOneDrive < prefs.officeFileMovedGoogleDrive) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.ui_.nudgeContainer.showNudge(NudgeType['DRIVE_MOVED_FILE_NUDGE']);
    }
    // Reset the seen state for office nudge. For normal users these 2 prefs
    // will never reset to 0, however for manual tests it can be reset in
    // chrome://files-internals.
    if (prefs.officeFileMovedOneDrive === 0 &&
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        await this.ui_.nudgeContainer.checkSeen(
            NudgeType['ONE_DRIVE_MOVED_FILE_NUDGE'])) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.ui_.nudgeContainer.clearSeen(
          NudgeType['ONE_DRIVE_MOVED_FILE_NUDGE']);
      console.debug('Reset OneDrive move to cloud nudge');
    }
    if (prefs.officeFileMovedGoogleDrive === 0 &&
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        await this.ui_.nudgeContainer.checkSeen(
            NudgeType['DRIVE_MOVED_FILE_NUDGE'])) {
      // @ts-ignore: error TS2531: Object is possibly 'null'.
      this.ui_.nudgeContainer.clearSeen(NudgeType['DRIVE_MOVED_FILE_NUDGE']);
      console.debug('Reset Google Drive move to cloud nudge');
    }
  }

  /**
   * Invoked when the device connection status changes.
   * @param {chrome.fileManagerPrivate.DeviceConnectionState} state
   * @private
   */
  updateDeviceConnectionState_(state) {
    this.store_.dispatch(updateDeviceConnectionState({connection: state}));
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
      // @ts-ignore: error TS2740: Type 'FilesAppEntry' is missing the following
      // properties from type 'FakeEntryImpl': label, disabled,
      // sourceRestriction, fileCategory, and 7 more.
      this.store_.dispatch(addUiEntry({entry: this.fakeTrashItem_.entry}));
      if (!isNewDirectoryTreeEnabled()) {
        // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
        // 'XfTree | DirectoryTree'.
        this.ui_.directoryTree.dataModel.fakeTrashItem = this.fakeTrashItem_;
      }
      return;
    }

    this.store_.dispatch(removeUiEntry({key: trashRootKey}));
    if (!isNewDirectoryTreeEnabled()) {
      // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
      // 'XfTree | DirectoryTree'.
      this.ui_.directoryTree.dataModel.fakeTrashItem = null;
    }
    this.navigateAwayFromDisabledRoot_(this.fakeTrashItem_);
  }

  /**
   * Toggles the drive root visibility when the `driveEnabled` preference is
   * updated.
   * @private
   */
  toggleDriveRootOnPreferencesUpdate_() {
    if (this.driveEnabled_) {
      let driveFakeRoot = /** @type {?EntryList} */
          (getEntry(this.store_.getState(), driveRootEntryListKey));
      if (!driveFakeRoot) {
        driveFakeRoot = new EntryList(
            str('DRIVE_DIRECTORY_LABEL'), RootType.DRIVE_FAKE_ROOT);
        this.store_.dispatch(addUiEntry({entry: driveFakeRoot}));
      }
      if (!isNewDirectoryTreeEnabled()) {
        // TODO(b/285977941): Remove the old FakeEntry based drive root.
        const driveFakeRoot = new FakeEntryImpl(
            str('DRIVE_DIRECTORY_LABEL'), RootType.DRIVE_FAKE_ROOT);
        if (!this.fakeDriveItem_) {
          this.fakeDriveItem_ = new NavigationModelFakeItem(
              str('DRIVE_DIRECTORY_LABEL'), NavigationModelItemType.DRIVE,
              driveFakeRoot);
          this.fakeDriveItem_.disabled =
              this.volumeManager_.isDisabled(VolumeType.DRIVE);
        }
        // @ts-ignore: error TS2339: Property 'dataModel' does not exist on
        // type 'XfTree | DirectoryTree'.
        this.ui_.directoryTree.dataModel.fakeDriveItem = this.fakeDriveItem_;
      }
      return;
    }
    this.store_.dispatch(removeUiEntry({key: driveRootEntryListKey}));
    if (!isNewDirectoryTreeEnabled()) {
      // @ts-ignore: error TS2339: Property 'dataModel' does not exist on type
      // 'XfTree | DirectoryTree'.
      this.ui_.directoryTree.dataModel.fakeDriveItem = null;
    }
    this.navigateAwayFromDisabledRoot_(this.fakeDriveItem_);
  }

  /**
   * If the root item has been disabled but it is the current visible entry,
   * navigate away from it to the default display root.
   * @param {?NavigationModelFakeItem} rootItem The item to navigate away
   *     from.
   * @private
   */
  navigateAwayFromDisabledRoot_(rootItem) {
    if (!rootItem) {
      return;
    }
    // The fake root item is being hidden so navigate away if it's the
    // current directory.
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    if (this.directoryModel_.getCurrentDirEntry() === rootItem.entry) {
      // @ts-ignore: error TS7006: Parameter 'displayRoot' implicitly has an
      // 'any' type.
      this.volumeManager_.getDefaultDisplayRoot((displayRoot) => {
        // @ts-ignore: error TS2531: Object is possibly 'null'.
        if (this.directoryModel_.getCurrentDirEntry() === rootItem.entry &&
            displayRoot) {
          // @ts-ignore: error TS2531: Object is possibly 'null'.
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
    // @ts-ignore: error TS2531: Object is possibly 'null'.
    this.dialogDom_.classList.toggle('tablet-mode-enabled', enabled);
  }
}
