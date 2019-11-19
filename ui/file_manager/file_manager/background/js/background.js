// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Root class of the background page.
 * @implements {FileBrowserBackgroundFull}
 */
class FileBrowserBackgroundImpl extends BackgroundBase {
  constructor() {
    super();
    this.setLaunchHandler(this.launch_);

    /**
     * Progress center of the background page.
     * @type {!ProgressCenter}
     */
    this.progressCenter = new ProgressCenterImpl();

    /**
     * File operation manager.
     * @type {FileOperationManager}
     */
    this.fileOperationManager = null;

    /**
     * Class providing loading of import history, used in
     * cloud import.
     *
     * @type {!importer.HistoryLoader}
     */
    this.historyLoader =
        new importer.SynchronizedHistoryLoader(importer.getHistoryFiles);

    /**
     * Event handler for progress center.
     * @private {FileOperationHandler}
     */
    this.fileOperationHandler_ = null;

    /**
     * Event handler for C++ sides notifications.
     * @private {!DeviceHandler}
     */
    this.deviceHandler_ = new DeviceHandler();

    // Handle device navigation requests.
    this.deviceHandler_.addEventListener(
        DeviceHandler.VOLUME_NAVIGATION_REQUESTED,
        this.handleViewEvent_.bind(this));

    /**
     * Drive sync handler.
     * @type {!DriveSyncHandler}
     */
    this.driveSyncHandler = new DriveSyncHandlerImpl(this.progressCenter);

    /**
     * @type {!importer.DispositionChecker.CheckerFunction}
     */
    this.dispositionChecker_ =
        importer.DispositionCheckerImpl.createChecker(this.historyLoader);

    /**
     * Provides support for scanning media devices as part of Cloud Import.
     * @type {!importer.MediaScanner}
     */
    this.mediaScanner = new importer.DefaultMediaScanner(
        importer.createMetadataHashcode, this.dispositionChecker_,
        importer.DefaultDirectoryWatcher.create);

    /**
     * Handles importing of user media (e.g. photos, videos) from removable
     * devices.
     * @type {!importer.MediaImportHandler}
     */
    this.mediaImportHandler = new importer.MediaImportHandlerImpl(
        this.progressCenter, this.historyLoader, this.dispositionChecker_,
        this.driveSyncHandler);

    /** @type {!Crostini} */
    this.crostini = new CrostiniImpl();

    /** @type {!MountMetrics} */
    this.mountMetrics = new MountMetrics();

    /**
     * String assets.
     * @type {Object<string>}
     */
    this.stringData = null;

    /**
     * Provides drive search to app launcher.
     * @private {!LauncherSearch}
     */
    this.launcherSearch_ = new LauncherSearch();

    // Initialize listeners.
    chrome.runtime.onMessageExternal.addListener(
        this.onExternalMessageReceived_.bind(this));
    chrome.contextMenus.onClicked.addListener(
        this.onContextMenuClicked_.bind(this));

    // Initialize string and volume manager related stuffs.
    this.initializationPromise_.then(strings => {
      this.stringData = strings;
      this.initContextMenu_();
      this.crostini.initEnabled();

      volumeManagerFactory.getInstance().then(volumeManager => {
        volumeManager.addEventListener(
            VolumeManagerCommon.VOLUME_ALREADY_MOUNTED,
            this.handleViewEvent_.bind(this));

        this.crostini.initVolumeManager(volumeManager);
        this.crostini.listen();
      });

      this.fileOperationManager = new FileOperationManagerImpl();
      this.fileOperationHandler_ = new FileOperationHandler(
          this.fileOperationManager, this.progressCenter);
    });

    // Handle newly mounted FSP file systems. Workaround for crbug.com/456648.
    // TODO(mtomasz): Replace this hack with a proper solution.
    chrome.fileManagerPrivate.onMountCompleted.addListener(
        this.onMountCompleted_.bind(this));

    launcher.queue.run(callback => {
      this.initializationPromise_.then(callback);
    });
  }

  /**
   * Register callback to be invoked after initialization.
   * If the initialization is already done, the callback is invoked immediately.
   *
   * @param {function()} callback Initialize callback to be registered.
   */
  ready(callback) {
    this.initializationPromise_.then(callback);
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param {!Event} event An event with the volumeId or
   *     devicePath.
   * @private
   */
  handleViewEvent_(event) {
    util.doIfPrimaryContext(() => {
      this.handleViewEventInternal_(event);
    });
  }

  /**
   * @param {!Event} event An event with the volumeId or
   *     devicePath.
   * @private
   */
  handleViewEventInternal_(event) {
    volumeManagerFactory.getInstance().then(
        /**
         * Retrieves the root file entry of the volume on the requested
         * device.
         * @param {!VolumeManager} volumeManager
         */
        volumeManager => {
          if (event.devicePath) {
            let volume = volumeManager.findByDevicePath(event.devicePath);
            if (volume) {
              this.navigateToVolumeRoot_(volume, event.filePath);
            } else {
              console.error('Got view event with invalid volume id.');
            }
          } else if (event.volumeId) {
            if (event.type === VolumeManagerCommon.VOLUME_ALREADY_MOUNTED) {
              this.navigateToVolumeInFocusedWindowWhenReady_(
                  event.volumeId, event.filePath);
            } else {
              this.navigateToVolumeWhenReady_(event.volumeId, event.filePath);
            }
          } else {
            console.error('Got view event with no actionable destination.');
          }
        });
  }

  /**
   * Retrieves the root file entry of the volume on the requested device.
   *
   * @param {!string} volumeId ID of the volume to navigate to.
   * @return {!Promise<!VolumeInfo>}
   * @private
   */
  retrieveVolumeInfo_(volumeId) {
    return volumeManagerFactory.getInstance().then(
        (/**
          * @param {!VolumeManager} volumeManager
          */
         (volumeManager) => {
           return volumeManager.whenVolumeInfoReady(volumeId).catch((e) => {
             console.error(
                 'Unable to find volume for id: ' + volumeId +
                 '. Error: ' + e.message);
           });
         }));
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param {!string} volumeId ID of the volume to navigate to.
   * @param {!string=} opt_directoryPath Optional path to be opened.
   * @private
   */
  navigateToVolumeWhenReady_(volumeId, opt_directoryPath) {
    this.retrieveVolumeInfo_(volumeId).then(volume => {
      this.navigateToVolumeRoot_(volume, opt_directoryPath);
    });
  }

  /**
   * Opens the volume root (or opt directoryPath) in the main UI of the focused
   * window.
   *
   * @param {!string} volumeId ID of the volume to navigate to.
   * @param {!string=} opt_directoryPath Optional path to be opened.
   * @private
   */
  navigateToVolumeInFocusedWindowWhenReady_(volumeId, opt_directoryPath) {
    this.retrieveVolumeInfo_(volumeId).then(volume => {
      this.navigateToVolumeInFocusedWindow_(volume, opt_directoryPath);
    });
  }

  /**
   * If a path was specified, retrieve that directory entry,
   * otherwise return the root entry of the volume.
   *
   * @param {!VolumeInfo} volume
   * @param {string=} opt_directoryPath Optional directory path to be opened.
   * @return {!Promise<!DirectoryEntry>}
   * @private
   */
  retrieveEntryInVolume_(volume, opt_directoryPath) {
    return volume.resolveDisplayRoot().then(root => {
      if (opt_directoryPath) {
        return new Promise(
            root.getDirectory.bind(root, opt_directoryPath, {create: false}));
      } else {
        return Promise.resolve(root);
      }
    });
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI.
   *
   * @param {!VolumeInfo} volume
   * @param {string=} opt_directoryPath Optional directory path to be opened.
   * @private
   */
  navigateToVolumeRoot_(volume, opt_directoryPath) {
    this.retrieveEntryInVolume_(volume, opt_directoryPath)
        .then(
            /**
             * Launches app opened on {@code directory}.
             * @param {DirectoryEntry} directory
             */
            directory => {
              launcher.launchFileManager(
                  {currentDirectoryURL: directory.toURL()},
                  /* App ID */ undefined, LaunchType.FOCUS_SAME_OR_CREATE);
            });
  }

  /**
   * Opens the volume root (or opt directoryPath) in main UI of the focused
   * window.
   *
   * @param {!VolumeInfo} volume
   * @param {string=} opt_directoryPath Optional directory path to be opened.
   * @private
   */
  navigateToVolumeInFocusedWindow_(volume, opt_directoryPath) {
    this.retrieveEntryInVolume_(volume, opt_directoryPath)
        .then(function(directoryEntry) {
          if (directoryEntry) {
            volumeManagerFactory.getInstance().then(volumeManager => {
              volumeManager.dispatchEvent(
                  VolumeManagerCommon.createArchiveOpenedEvent(directoryEntry));
            });
          }
        });
  }

  /**
   * Launches the app.
   * @private
   * @override
   */
  onLaunched_(launchData) {
    metrics.startInterval('Load.BackgroundLaunch');
    if (!launchData || !launchData.items || launchData.items.length == 0) {
      this.launch_(undefined);
      return;
    }
    BackgroundBase.prototype.onLaunched_.apply(this, [launchData]);
  }

  /**
   * Launches the app.
   * @private
   * @param {!Array<string>|undefined} urls
   */
  launch_(urls) {
    return this.initializationPromise_.then(() => {
      if (nextFileManagerWindowID == 0) {
        // The app just launched. Remove unneeded window state records.
        chrome.storage.local.get(items => {
          for (const key in items) {
            if (items.hasOwnProperty(key)) {
              if (key.match(FILES_ID_PATTERN)) {
                chrome.storage.local.remove(key);
              }
            }
          }
        });
      }
      let appState = {};
      let launchType = LaunchType.FOCUS_ANY_OR_CREATE;
      if (urls) {
        appState.selectionURL = urls[0];
        launchType = LaunchType.FOCUS_SAME_OR_CREATE;
      }
      launcher.launchFileManager(appState, undefined, launchType, () => {
        metrics.recordInterval('Load.BackgroundLaunch');
      });
    });
  }

  /**
   * Handles a message received via chrome.runtime.sendMessageExternal.
   *
   * @param {*} message
   * @param {MessageSender} sender
   */
  onExternalMessageReceived_(message, sender) {
    if ('id' in sender && sender.id === GPLUS_PHOTOS_APP_ID) {
      importer.handlePhotosAppMessage(message);
    }
  }

  /**
   * Restarted the app, restore windows.
   * @private
   * @override
   */
  onRestarted_() {
    // Reopen file manager windows.
    chrome.storage.local.get(items => {
      for (const key in items) {
        if (items.hasOwnProperty(key)) {
          const match = key.match(FILES_ID_PATTERN);
          if (match) {
            metrics.startInterval('Load.BackgroundRestart');
            const id = Number(match[1]);
            try {
              const appState = /** @type {Object} */ (JSON.parse(items[key]));
              launcher.launchFileManager(appState, id, undefined, () => {
                metrics.recordInterval('Load.BackgroundRestart');
              });
            } catch (e) {
              console.error('Corrupt launch data for ' + id);
            }
          }
        }
      }
    });
  }

  /**
   * Handles clicks on a custom item on the launcher context menu.
   * @param {!Object} info Event details.
   * @private
   */
  onContextMenuClicked_(info) {
    if (info.menuItemId == 'new-window') {
      // Find the focused window (if any) and use it's current url for the
      // new window. If not found, then launch with the default url.
      this.findFocusedWindow_()
          .then(key => {
            if (!key) {
              launcher.launchFileManager();
              return;
            }
            const appState = {
              // Do not clone the selection url, only the current directory.
              currentDirectoryURL:
                  window.appWindows[key]
                      .contentWindow.appState.currentDirectoryURL
            };
            launcher.launchFileManager(appState);
          })
          .catch(error => {
            console.error(error.stack || error);
          });
    }
  }

  /**
   * Looks for a focused window.
   *
   * @return {!Promise<?string>} Promise fulfilled with a key of the focused
   *     window, or null if not found.
   * @private
   */
  findFocusedWindow_() {
    return new Promise((fulfill, reject) => {
      for (const key in window.appWindows) {
        try {
          if (window.appWindows[key].contentWindow.isFocused()) {
            fulfill(key);
            return;
          }
        } catch (ignore) {
          // The isFocused method may not be defined during initialization.
          // Therefore, wrapped with a try-catch block.
        }
      }
      fulfill(null);
    });
  }

  /**
   * Handles mounted FSP volumes and fires the Files app. This is a quick fix
   * for crbug.com/456648.
   * @param {!Object} event Event details.
   * @private
   */
  onMountCompleted_(event) {
    util.doIfPrimaryContext(() => {
      this.onMountCompletedInternal_(event);
    });
  }

  /**
   * @param {!Object} event Event details.
   * @private
   */
  onMountCompletedInternal_(event) {
    // If there is no focused window, then create a new one opened on the
    // mounted volume.
    this.findFocusedWindow_()
        .then(key => {
          let statusOK = event.status === 'success' ||
              event.status === 'error_path_already_mounted';
          let volumeTypeOK = event.volumeMetadata.volumeType ===
                  VolumeManagerCommon.VolumeType.PROVIDED &&
              event.volumeMetadata.source === VolumeManagerCommon.Source.FILE;
          if (key === null && event.eventType === 'mount' && statusOK &&
              event.volumeMetadata.mountContext === 'user' && volumeTypeOK) {
            this.navigateToVolumeWhenReady_(event.volumeMetadata.volumeId);
          }
        })
        .catch(error => {
          console.error(error.stack || error);
        });
  }

  /**
   * Initializes the context menu. Recreates if already exists.
   * @private
   */
  initContextMenu_() {
    try {
      // According to the spec [1], the callback is optional. But no callback
      // causes an error for some reason, so we call it with null-callback to
      // prevent the error. http://crbug.com/353877
      // Also, we read the runtime.lastError here not to output the message on
      // the console as an unchecked error.
      // - [1]
      // https://developer.chrome.com/extensions/contextMenus#method-remove
      chrome.contextMenus.remove('new-window', () => {
        const ignore = chrome.runtime.lastError;
      });
    } catch (ignore) {
      // There is no way to detect if the context menu is already added,
      // therefore try to recreate it every time.
    }
    chrome.contextMenus.create({
      id: 'new-window',
      contexts: ['launcher'],
      title: str('NEW_WINDOW_BUTTON_LABEL')
    });
  }
}

/**
 * Prefix for the dialog ID.
 * @type {!string}
 * @const
 */
const DIALOG_ID_PREFIX = 'dialog#';

/**
 * Value of the next file manager dialog ID.
 * @type {number}
 */
let nextFileManagerDialogID = 0;

/**
 * Registers dialog window to the background page.
 *
 * @param {!Window} dialogWindow Window of the dialog.
 */
function registerDialog(dialogWindow) {
  const id = DIALOG_ID_PREFIX + (nextFileManagerDialogID++);
  window.background.dialogs[id] = dialogWindow;
  if (window.IN_TEST) {
    dialogWindow.IN_TEST = true;
  }
  dialogWindow.addEventListener('pagehide', () => {
    delete window.background.dialogs[id];
  });
}

/** @const {!string} */
const GPLUS_PHOTOS_APP_ID = 'efjnaogkjbogokcnohkmnjdojkikgobo';

/**
 * Singleton instance of Background object.
 * @type {!FileBrowserBackgroundImpl}
 */
window.background = new FileBrowserBackgroundImpl();

/**
 * Lastly, end recording of the background page Load.BackgroundScript metric.
 * NOTE: This call must come after the call to metrics.clearUserId.
 */
metrics.recordInterval('Load.BackgroundScript');
