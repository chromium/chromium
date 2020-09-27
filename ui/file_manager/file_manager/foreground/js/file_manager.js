// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * FileManager constructor.
 *
 * FileManager objects encapsulate the functionality of the file selector
 * dialogs, as well as the full screen file manager application.
 *
 * @implements {CommandHandlerDeps}
 */
class FileManager extends cr.EventTarget {
  constructor() {
    super();

    // ------------------------------------------------------------------------
    // Services FileManager depends on.

    /**
     * Volume manager.
     * @private {!FilteredVolumeManager}
     */
    this.volumeManager_;

    /** @private {?importer.HistoryLoader} */
    this.historyLoader_ = null;

    /** @private {?Crostini} */
    this.crostini_ = null;

    /** @private {?CrostiniController} */
    this.crostiniController_ = null;

    /**
     * ImportHistory. Non-null only once history observer is added in
     * {@code addHistoryObserver}.
     * @private {?importer.ImportHistory}
     */
    this.importHistory_ = null;

    /**
     * Bound observer for use with {@code importer.ImportHistory.Observer}.
     * The instance is bound once here as {@code ImportHistory.removeObserver}
     * uses object equivilency to remove observers.
     *
     * @private @const {function(!importer.ImportHistory.ChangedEvent)}
     */
    this.onHistoryChangedBound_ = this.onHistoryChanged_.bind(this);

    /** @private {?importer.MediaScanner} */
    this.mediaScanner_ = null;

    /** @private {?importer.ImportController} */
    this.importController_ = null;

    /** @private {?importer.ImportRunner} */
    this.mediaImportHandler_ = null;

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
     * Empty folder controller.
     * @private {EmptyFolderController}
     */
    this.emptyFolderController_ = null;

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

    /** @private {ColumnVisibilityController} */
    this.columnVisibilityController_ = null;

    /** @private {?QuickViewUma} */
    this.quickViewUma_ = null;

    /** @private {?QuickViewController} */
    this.quickViewController_ = null;

    /** @private {?FileTypeFiltersController} */
    this.fileTypeFiltersController_ = null;

    /**
     * Records histograms of directory-changed event.
     * @private {?NavigationUma}
     */
    this.navigationUma_ = null;

    // ------------------------------------------------------------------------
    // DOM elements.

    /**
     * Background page.
     * @private {?BackgroundWindow}
     */
    this.backgroundPage_ = null;

    /**
     * @private {?FileBrowserBackgroundFull}
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
     * Flags async retrieved once at startup and can be used to switch behaviour
     * on sync functions.
     * @dict
     * @private
     */
    this.commandLineFlags_ = {};

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
     * A fake entry for Recents.
     * @private {?FakeEntry}
     */
    this.recentEntry_ = null;
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
   * @return {importer.ImportController}
   */
  get importController() {
    return this.importController_;
  }

  /**
   * @return {importer.HistoryLoader}
   */
  get historyLoader() {
    return this.historyLoader_;
  }

  /**
   * @return {Crostini}
   */
  get crostini() {
    return this.crostini_;
  }

  /**
   * @return {importer.ImportRunner}
   */
  get mediaImportHandler() {
    return this.mediaImportHandler_;
  }

  /**
   * @return {FileManagerUI}
   */
  get ui() {
    return this.ui_;
  }

  /**
   * Launch a new File Manager app.
   * @param {Object=} appState App state.
   */
  launchFileManager(appState) {
    this.backgroundPage_.launcher.launchFileManager(appState);
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
    assert(this.dialogDom_);
    assert(this.fileFilter_);

    this.scanController_ = new ScanController(
        this.directoryModel_, this.ui_.listContainer, this.spinnerController_,
        this.commandHandler_, this.selectionHandler_);
    this.sortMenuController_ = new SortMenuController(
        this.ui_.sortButton, this.ui_.sortButtonToggleRipple,
        assert(this.directoryModel_.getFileList()));
    this.gearMenuController_ = new GearMenuController(
        this.ui_.gearButton, this.ui_.gearButtonToggleRipple, this.ui_.gearMenu,
        this.ui_.providersMenu, this.directoryModel_, this.commandHandler_,
        assert(this.providersModel_));
    this.selectionMenuController_ = new SelectionMenuController(
        this.ui_.selectionMenuButton,
        util.queryDecoratedElement('#file-context-menu', cr.ui.Menu));
    this.toolbarController_ = new ToolbarController(
        this.ui_.toolbar, this.ui_.dialogNavigationList, this.ui_.listContainer,
        assert(this.ui_.locationLine), this.selectionHandler_,
        this.directoryModel_, this.volumeManager_,
        /** @type {!A11yAnnounce} */ (this.ui_));
    this.emptyFolderController_ = new EmptyFolderController(
        this.ui_.emptyFolder, this.directoryModel_, this.ui_.alertDialog);
    this.actionsController_ = new ActionsController(
        this.volumeManager_, assert(this.metadataModel_), this.directoryModel_,
        assert(this.folderShortcutsModel_),
        this.fileBrowserBackground_.driveSyncHandler, this.selectionHandler_,
        assert(this.ui_));
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

    if (this.dialogType === DialogType.FULL_PAGE) {
      this.importController_ = new importer.ImportController(
          new importer.RuntimeControllerEnvironment(
              this, assert(this.selectionHandler_)),
          assert(this.mediaScanner_), assert(this.mediaImportHandler_),
          new importer.RuntimeCommandWidget());
    }

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

    this.ui_.initBanners(new Banners(
        this.directoryModel_, this.volumeManager_, this.document_,
        // Whether to show any welcome banner.
        this.dialogType === DialogType.FULL_PAGE));

    this.ui_.attachFilesTooltip();
    this.ui_.decorateFilesMenuItems();
    this.ui_.selectionMenuButton.hidden = false;

    await Promise.all([fileListPromise, currentDirectoryPromise]);
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
        assert(this.thumbnailModel_), assert(this.directoryModel_),
        assert(this.volumeManager_), assert(this.selectionHandler_));
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

    this.document_.addEventListener(
        'command',
        this.ui_.listContainer.clearHover.bind(this.ui_.listContainer));
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

    cr.ui.contextMenuHandler.setContextMenu(input, this.ui_.textContextMenu);
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
      const key = util.getKeyModifiers(e) + e.keyCode;
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
    this.initBackgroundPagePromise_ = this.startInitBackgroundPage_();
    this.initBackgroundPagePromise_.then(() => this.initVolumeManager_());

    window.addEventListener('pagehide', this.onUnload_.bind(this));
  }

  /**
   * @return {!Promise<void>}
   */
  async initializeUI(dialogDom) {
    console.warn('Files app starting up');
    this.dialogDom_ = dialogDom;
    this.document_ = this.dialogDom_.ownerDocument;

    metrics.startInterval('Load.InitDocuments');
    await Promise.all(
        [this.initBackgroundPagePromise_, window.importElementsPromise]);
    metrics.recordInterval('Load.InitDocuments');

    metrics.startInterval('Load.InitUI');
    if (util.isFilesNg()) {
      this.document_.documentElement.classList.add('files-ng');
      this.dialogDom_.classList.add('files-ng');
    } else {
      this.document_.documentElement.classList.remove('files-ng');
      this.dialogDom_.classList.remove('files-ng');
    }
    this.initEssentialUI_();
    this.initAdditionalUI_();
    await this.initSettingsPromise_;
    const fileSystemUIPromise = this.initFileSystemUI_();
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

      for (const name in window.appState.params) {
        params[name] = window.appState.params[name];
      }

      this.launchParams_ = new LaunchParam(params);
    } else {
      // Used by the select dialog only.
      const json = location.search ?
          JSON.parse(decodeURIComponent(location.search.substr(1))) :
          {};
      this.launchParams_ = new LaunchParam(json instanceof Object ? json : {});
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

    /** @type {!Window} */
    const backgroundPage =
        await new Promise(resolve => chrome.runtime.getBackgroundPage(resolve));
    assert(backgroundPage);
    this.backgroundPage_ =
        /** @type {!BackgroundWindow} */ (backgroundPage);
    this.fileBrowserBackground_ =
        /** @type {!FileBrowserBackgroundFull} */ (
            this.backgroundPage_.background);

    await new Promise(resolve => this.fileBrowserBackground_.ready(resolve));
    loadTimeData.data = this.fileBrowserBackground_.stringData;
    if (util.runningInBrowser()) {
      this.backgroundPage_.registerDialog(window);
    }
    this.fileOperationManager_ =
        this.fileBrowserBackground_.fileOperationManager;
    this.mediaImportHandler_ = this.fileBrowserBackground_.mediaImportHandler;
    this.mediaScanner_ = this.fileBrowserBackground_.mediaScanner;
    this.historyLoader_ = this.fileBrowserBackground_.historyLoader;
    this.crostini_ = this.fileBrowserBackground_.crostini;
    metrics.recordInterval('Load.InitBackgroundPage');
  }

  /**
   * Initializes the VolumeManager instance.
   * @private
   */
  initVolumeManager_() {
    const allowedPaths = this.getAllowedPaths_();
    const writableOnly =
        this.launchParams_.type === DialogType.SELECT_SAVEAS_FILE;

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
        allowedPaths, writableOnly, this.backgroundPage_);
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
    metrics.recordEnum('Create', this.dialogType, [
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
    if (util.isFilesNg()) {
      dialogHeader.classList.add('files-ng');
      // Move the dialog header to the side of the splitter above the list view.
      const dialogMain = queryRequiredElement('.dialog-main');
      dialogMain.insertBefore(dialogHeader, dialogMain.firstChild);
    } else {
      dialogHeader.classList.remove('files-ng');
    }

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
    assert(this.historyLoader_);
    assert(this.dialogDom_);
    assert(this.ui_);

    // Cache nodes we'll be manipulating.
    const dom = this.dialogDom_;
    assert(dom);

    const table = queryRequiredElement('.detail-table', dom);
    FileTable.decorate(
        table, this.metadataModel_, this.volumeManager_, this.historyLoader_,
        /** @type {!A11yAnnounce} */ (this.ui_),
        this.dialogType == DialogType.FULL_PAGE);
    const grid = queryRequiredElement('.thumbnail-grid', dom);
    FileGrid.decorate(
        grid, this.metadataModel_, this.volumeManager_, this.historyLoader_,
        /** @type {!A11yAnnounce} */ (this.ui_));

    this.addHistoryObserver_();

    this.ui_.initAdditionalUI(
        assertInstanceof(table, FileTable), assertInstanceof(grid, FileGrid),
        this.volumeManager_);

    // Handle UI events.
    this.progressCenter.addPanel(this.ui_.progressCenterPanel);

    util.addIsFocusedMethod();

    // The cwd is not known at this point.  Hide the import status column before
    // redrawing, to avoid ugly flashing in the UI, caused when the first redraw
    // has a visible status column, and then the cwd is later discovered to be
    // not an import-eligible location.
    this.ui_.listContainer.table.setImportStatusVisible(false);

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
   * One-time initialization of import history observer. Provides
   * the glue that updates the UI when history changes.
   *
   * @private
   */
  addHistoryObserver_() {
    // If, and only if history is ever fully loaded (it may not be),
    // we want to update grid/list view when it changes.
    this.historyLoader_.addHistoryLoadedListener(
        /**
         * @param {!importer.ImportHistory} history
         * @this {FileManager}
         */
        history => {
          this.importHistory_ = history;
          history.addObserver(this.onHistoryChangedBound_);
        });
  }

  /**
   * Handles events when import history changed.
   *
   * @param {!importer.ImportHistory.ChangedEvent} event
   * @private
   */
  onHistoryChanged_(event) {
    // Ignore any entry that isn't an immediate child of the
    // current directory.
    util.isChildEntry(event.entry, this.getCurrentDirectoryEntry())
        .then(
            /**
             * @param {boolean} isChild
             */
            isChild => {
              if (isChild) {
                this.ui_.listContainer.grid.updateListItemsMetadata(
                    'import-history', [event.entry]);
                this.ui_.listContainer.table.updateListItemsMetadata(
                    'import-history', [event.entry]);
              }
            });
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

    this.recentEntry_ = new FakeEntry(
        str('RECENT_ROOT_LABEL'), VolumeManagerCommon.RootType.RECENT,
        this.getSourceRestriction_());

    assert(this.launchParams_);
    this.selectionHandler_ = new FileSelectionHandler(
        assert(this.directoryModel_), assert(this.fileOperationManager_),
        assert(this.ui_.listContainer), assert(this.metadataModel_),
        assert(this.volumeManager_), this.launchParams_.allowedPaths);

    this.directoryModel_.getFileListSelection().addEventListener(
        'change',
        this.selectionHandler_.onFileSelectionChanged.bind(
            this.selectionHandler_));

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

    if (this.dialogType === DialogType.FULL_PAGE) {
      this.columnVisibilityController_ = new ColumnVisibilityController(
          this.ui_, this.directoryModel_, this.volumeManager_);
    }

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
        this.metadataUpdateController_, this.namingController_,
        assert(this.crostini_), this.progressCenter);

    // Create search controller.
    this.searchController_ = new SearchController(
        this.ui_.searchBox, assert(this.ui_.locationLine), this.directoryModel_,
        this.volumeManager_, assert(this.taskController_), assert(this.ui_));

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
    if (util.isRecentsFilterEnabled()) {
      this.fileTypeFiltersController_ = new FileTypeFiltersController(
          this.ui_.fileTypeFilterContainer, this.directoryModel_,
          this.recentEntry_);
    }

    return directoryTreePromise;
  }

  /**
   * @return {!Promise<void>}
   * @private
   */
  async initDirectoryTree_() {
    const directoryTree = /** @type {DirectoryTree} */
        (this.dialogDom_.querySelector('#directory-tree'));
    const fakeEntriesVisible =
        this.dialogType !== DialogType.SELECT_SAVEAS_FILE;
    this.navigationUma_ = new NavigationUma(assert(this.volumeManager_));
    DirectoryTree.decorate(
        directoryTree, assert(this.directoryModel_),
        assert(this.volumeManager_), assert(this.metadataModel_),
        assert(this.fileOperationManager_), fakeEntriesVisible);
    directoryTree.dataModel = new NavigationListModel(
        assert(this.volumeManager_), assert(this.folderShortcutsModel_),
        fakeEntriesVisible &&
                !DialogType.isFolderDialog(this.launchParams_.type) ?
            new NavigationModelFakeItem(
                str('RECENT_ROOT_LABEL'), NavigationModelItemType.RECENT,
                assert(this.recentEntry_)) :
            null,
        assert(this.directoryModel_), assert(this.androidAppListModel_));

    this.ui_.initDirectoryTree(directoryTree);
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
        assert(this.crostini_), /** @type {!FilesMessage} */
        (this.document_.querySelector('#files-message')), this.directoryModel_,
        assert(this.directoryTree));
    await this.crostiniController_.redraw();
    // Never show toast in an open-file dialog.
    const maybeShowToast = this.dialogType === DialogType.FULL_PAGE;
    return this.crostiniController_.loadSharedPaths(
        maybeShowToast, this.ui_.toast);
  }

  /**
   * Listens for the enable/disable events in order to show/hide
   * the 'Linux files' root.
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
        this.crostini_.setEnabled(event.vmName, true);
        return this.crostiniController_.redraw();

      case chrome.fileManagerPrivate.CrostiniEventType.DISABLE:
        this.crostini_.setEnabled(event.vmName, false);
        return this.crostiniController_.redraw();
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
      try {
        const inEntry = await new Promise((resolve, reject) => {
          window.webkitResolveLocalFileSystemURL(
              this.launchParams_.selectionURL, resolve, reject);
        });
        const locationInfo = this.volumeManager_.getLocationInfo(inEntry);
        // If location information is not available, then the volume is no
        // longer (or never) available.
        if (locationInfo) {
          // If the selection is root, then use it as a current directory
          // instead. This is because, selecting a root entry is done as opening
          // it.
          if (locationInfo.isRootEntry) {
            nextCurrentDirEntry = inEntry;
          }

          // If the |selectionURL| is a directory make it the current directory.
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
        console.warn(error.stack || error);
      }
    }

    // If searchQuery param is set, find the first directory that matches the
    // query, and select it if exists.
    const searchQuery = this.launchParams_.searchQuery;
    if (searchQuery) {
      this.searchController_.setSearchQuery(searchQuery);
      // Show a spinner, as the crossover search function call could be slow.
      const hideSpinnerCallback = this.spinnerController_.show();
      const queryMatchedDirEntry =
          await crossoverSearchUtils.findQueryMatchedDirectoryEntry(
              this.directoryTree.dataModel_, this.directoryModel_, searchQuery);
      if (queryMatchedDirEntry) {
        nextCurrentDirEntry = queryMatchedDirEntry;
      }
      hideSpinnerCallback();
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
        // Having root directory of DRIVE_OTHER here should be only for shared
        // with me files. Fallback to Drive root in such case.
        if (locationInfo.isRootEntry &&
            locationInfo.rootType ===
                VolumeManagerCommon.RootType.DRIVE_OTHER) {
          const volumeInfo =
              this.volumeManager_.getVolumeInfo(nextCurrentDirEntry);
          if (!volumeInfo) {
            nextCurrentDirEntry = null;
          } else {
            try {
              nextCurrentDirEntry = await volumeInfo.resolveDisplayRoot();
            } catch (error) {
              console.error(error.stack || error);
              nextCurrentDirEntry = null;
            }
          }
        }
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
          // Failed to resolve as either file or directory.
          console.error(error1.stack || error1);
          console.error(error2.stack || error2);
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
    if (this.importHistory_) {
      this.importHistory_.removeObserver(this.onHistoryChangedBound_);
    }

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
        !DialogType.isFolderDialog(this.launchParams_.type)) {
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
   * @return {cr.ui.ArrayDataModel} File list.
   */
  getFileList() {
    return this.directoryModel_.getFileList();
  }

  /**
   * @return {!cr.ui.List} Current list object.
   */
  getCurrentList() {
    return this.ui.listContainer.currentList;
  }

  /**
   * Refreshes Drive prefs when they change. If Drive has been enabled or
   * disabled, add or remove, respectively, the fake Drive item, creating it if
   * necessary.
   */
  onPreferencesChanged_() {
    chrome.fileManagerPrivate.getPreferences(
        (/** chrome.fileManagerPrivate.Preferences|undefined */ prefs) => {
          if (chrome.runtime.lastError ||
              this.driveEnabled_ === prefs.driveEnabled) {
            return;
          }
          this.driveEnabled_ = prefs.driveEnabled;
          if (prefs.driveEnabled) {
            if (!this.fakeDriveItem_) {
              this.fakeDriveItem_ = new NavigationModelFakeItem(
                  str('DRIVE_DIRECTORY_LABEL'), NavigationModelItemType.DRIVE,
                  new FakeEntry(
                      str('DRIVE_DIRECTORY_LABEL'),
                      VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT));
            }
            this.directoryTree.dataModel.fakeDriveItem = this.fakeDriveItem_;
          } else {
            this.directoryTree.dataModel.fakeDriveItem = null;
            // The fake Drive item is being hidden so navigate away if it's the
            // current directory.
            if (this.directoryModel_.getCurrentDirEntry() ===
                this.fakeDriveItem_.entry) {
              this.volumeManager_.getDefaultDisplayRoot((displayRoot) => {
                if (this.directoryModel_.getCurrentDirEntry() ===
                        this.fakeDriveItem_.entry &&
                    displayRoot) {
                  this.directoryModel_.changeDirectoryEntry(displayRoot);
                }
              });
            }
          }
          this.directoryTree.redraw(false);
        });
  }
}
