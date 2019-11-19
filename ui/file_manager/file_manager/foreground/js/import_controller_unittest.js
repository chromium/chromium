// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @const {!Event} */
const EMPTY_EVENT = new Event('directory-changed');

/** @type {!MockVolumeManager} */
let volumeManager;

/** @type {!TestMediaScanner} */
let mediaScanner;

/** @type {!TestImportRunner} */
let mediaImporter;

/** @type {!TestControllerEnvironment} */
let environment;

/** @type {!VolumeInfo} */
let sourceVolume;

/** @type {!VolumeInfo} */
let destinationVolume;

/** @type {!importer.TestCommandWidget} */
let widget;

/** @type {!DirectoryEntry} */
let nonDcimDirectory;

/**
 * Mock metrics.
 * @type {!Object}
 */
window.metrics = {
  recordSmallCount: function() {},
  recordUserAction: function() {},
  recordValue: function() {},
  recordBoolean: function() {},
};

// Set up the test components.
function setUp() {
  window.loadTimeData.getString = id => id;
  window.loadTimeData.data = {};

  new MockChromeStorageAPI();
  new MockCommandLinePrivate();

  widget = new importer.TestCommandWidget();

  const testFileSystem = new MockFileSystem('testFs');
  nonDcimDirectory = MockDirectoryEntry.create(testFileSystem, '/jellybeans/');

  volumeManager = new MockVolumeManager();
  MockVolumeManager.installMockSingleton(volumeManager);

  const downloads = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  assert(downloads);
  destinationVolume = downloads;

  mediaScanner = new TestMediaScanner();
  mediaImporter = new TestImportRunner();
}

function testClickMainToStartImport(callback) {
  reportPromise(startImport(importer.ClickSource.MAIN), callback);
}

function testClickPanelToStartImport(callback) {
  reportPromise(startImport(importer.ClickSource.IMPORT), callback);
}

function testClickCancel(callback) {
  const promise = startImport(importer.ClickSource.IMPORT).then(task => {
    widget.click(importer.ClickSource.CANCEL);
    return task.whenCanceled;
  });

  reportPromise(promise, callback);
}

function testVolumeUnmount_InvalidatesScans(callback) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
        '/DCIM/photos1/',
        '/DCIM/photos1/IMG00001.jpg',
        '/DCIM/photos1/IMG00003.jpg',
      ],
      '/DCIM');

  let dcim = environment.getCurrentDirectory();
  assert(dcim);

  environment.directoryChangedListener(EMPTY_EVENT);
  const promise = widget.updateResolver.promise
                      .then(() => {
                        // Reset the promise so we can wait on a second widget
                        // update.
                        widget.resetPromises();
                        environment.setCurrentDirectory(nonDcimDirectory);
                        environment.simulateUnmount();

                        dcim = /** @type {!DirectoryEntry} */ (dcim);
                        environment.setCurrentDirectory(dcim);
                        environment.directoryChangedListener(EMPTY_EVENT);
                        // Return the new promise, so subsequent "thens" only
                        // fire once the widget has been updated again.
                        return widget.updateResolver.promise;
                      })
                      .then(() => {
                        mediaScanner.assertScanCount(2);
                      });

  reportPromise(promise, callback);
}

function testDirectoryChange_TriggersUpdate(callback) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  environment.directoryChangedListener(EMPTY_EVENT);
  reportPromise(widget.updateResolver.promise, callback);
}

function testDirectoryChange_CancelsScan(callback) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
        '/DCIM/photos1/',
        '/DCIM/photos1/IMG00001.jpg',
        '/DCIM/photos1/IMG00003.jpg',
      ],
      '/DCIM');

  environment.directoryChangedListener(EMPTY_EVENT);
  const promise = widget.updateResolver.promise
                      .then(() => {
                        // Reset the promise so we can wait on a second widget
                        // update.
                        widget.resetPromises();
                        environment.setCurrentDirectory(nonDcimDirectory);
                        environment.directoryChangedListener(EMPTY_EVENT);
                      })
                      .then(() => {
                        mediaScanner.assertScanCount(1);
                        mediaScanner.assertLastScanCanceled();
                      });

  reportPromise(promise, callback);
}

function testWindowClose_CancelsScan(callback) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
        '/DCIM/photos1/',
        '/DCIM/photos1/IMG00001.jpg',
        '/DCIM/photos1/IMG00003.jpg',
      ],
      '/DCIM');

  environment.directoryChangedListener(EMPTY_EVENT);
  const promise = widget.updateResolver.promise
                      .then(() => {
                        // Reset the promise so we can wait on a second widget
                        // update.
                        widget.resetPromises();
                        environment.windowCloseListener();
                      })
                      .then(() => {
                        mediaScanner.assertScanCount(1);
                        mediaScanner.assertLastScanCanceled();
                      });

  reportPromise(promise, callback);
}

function testDirectoryChange_DetailsPanelVisibility_InitialChangeDir(callback) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  const fileSystem = new MockFileSystem('testFs');
  const event = new Event('directory-changed');
  event.newDirEntry = MockDirectoryEntry.create(fileSystem, '/DCIM/');

  // Ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  mediaScanner.fileEntries.push(MockFileEntry.create(
      fileSystem, '/DCIM/photos0/IMG00001.jpg', getDefaultMetadata()));

  // Make controller enter a scanning state.
  environment.directoryChangedListener(event);
  assertFalse(widget.detailsVisible);

  const promise = widget.updateResolver.promise
                      .then(() => {
                        // "scanning..."
                        assertFalse(widget.detailsVisible);
                        widget.resetPromises();
                        mediaScanner.finalizeScans();
                        return widget.updateResolver.promise;
                      })
                      .then(() => {
                        // "ready to update"
                        // Details should pop up.
                        assertTrue(widget.detailsVisible);
                      });

  reportPromise(promise, callback);
}

function testDirectoryChange_DetailsPanelVisibility_SubsequentChangeDir() {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  const event = new Event('directory-changed');
  event.newDirEntry =
      MockDirectoryEntry.create(new MockFileSystem('testFs'), '/DCIM/');

  // Any previous dir at all will skip the new window logic.
  event.previousDirEntry = event.newDirEntry;

  environment.directoryChangedListener(event);
  assertFalse(widget.detailsVisible);
}

function testSelectionChange_TriggersUpdate(callback) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  const fileSystem = new MockFileSystem('testFs');

  // Ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  environment.selection.push(MockFileEntry.create(
      fileSystem, '/DCIM/photos0/IMG00001.jpg', getDefaultMetadata()));

  environment.selectionChangedListener();
  mediaScanner.finalizeScans();
  reportPromise(widget.updateResolver.promise, callback);
}

function testFinalizeScans_TriggersUpdate(callback) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  const fileSystem = new MockFileSystem('testFs');

  // Ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  mediaScanner.fileEntries.push(MockFileEntry.create(
      fileSystem, '/DCIM/photos0/IMG00001.jpg', getDefaultMetadata()));

  environment.directoryChangedListener(EMPTY_EVENT);  // initiates a scan.
  widget.resetPromises();
  mediaScanner.finalizeScans();

  reportPromise(widget.updateResolver.promise, callback);
}

function testClickDestination_ShowsRootPriorToImport(callback) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
      ],
      '/DCIM');

  widget.click(importer.ClickSource.DESTINATION);

  reportPromise(environment.showImportRootResolver.promise, callback);
}

function testClickDestination_ShowsDestinationAfterImportStarted(callback) {
  const promise = startImport(importer.ClickSource.MAIN).then(() => {
    return mediaImporter.importResolver.promise.then(() => {
      widget.click(importer.ClickSource.DESTINATION);
      return environment.showImportDestinationResolver.promise;
    });
  });

  reportPromise(promise, callback);
}

function startImport(clickSource) {
  const controller = createController(
      VolumeManagerCommon.VolumeType.MTP, 'mtp-volume',
      [
        '/DCIM/',
        '/DCIM/photos0/',
        '/DCIM/photos0/IMG00001.jpg',
        '/DCIM/photos0/IMG00002.jpg',
      ],
      '/DCIM');

  const fileSystem = new MockFileSystem('testFs');

  // Ensure there is some content in the scan so the code that depends
  // on this state doesn't croak which it finds it missing.
  mediaScanner.fileEntries.push(MockFileEntry.create(
      fileSystem, '/DCIM/photos0/IMG00001.jpg', getDefaultMetadata()));

  // First we need to force the controller into a scanning state.
  environment.directoryChangedListener(EMPTY_EVENT);

  return widget.updateResolver.promise.then(() => {
    widget.resetPromises();
    mediaScanner.finalizeScans();
    return widget.updateResolver.promise.then(() => {
      widget.resetPromises();
      widget.click(clickSource);
      return mediaImporter.importResolver.promise;
    });
  });
}

/**
 * A stub that just provides interfaces from ImportTask that are required by
 * these tests.
 */
class TestImportTask {
  /**
   * @param {!importer.ScanResult} scan
   * @param {!importer.Destination} destination
   * @param {!Promise<!DirectoryEntry>} destinationDirectory
   */
  constructor(scan, destination, destinationDirectory) {
    /** @public {!importer.ScanResult} */
    this.scan = scan;

    /** @type {!importer.Destination} */
    this.destination = destination;

    /** @type {!Promise<!DirectoryEntry>} */
    this.destinationDirectory = destinationDirectory;

    /** @private {!importer.Resolver} */
    this.finishedResolver_ = new importer.Resolver();

    /** @private {!importer.Resolver} */
    this.canceledResolver_ = new importer.Resolver();

    /** @public {!Promise} */
    this.whenFinished = this.finishedResolver_.promise;

    /** @public {!Promise} */
    this.whenCanceled = this.canceledResolver_.promise;
  }

  finish() {
    this.finishedResolver_.resolve();
  }

  requestCancel() {
    this.canceledResolver_.resolve();
  }
}

/**
 * Test import runner.
 *
 * @implements {importer.ImportRunner}
 */
class TestImportRunner {
  constructor() {
    /** @public {!Array<!importer.ScanResult>} */
    this.imported = [];

    /**
     * Resolves when import is started.
     * @public {!importer.Resolver.<!TestImportTask>}
     */
    this.importResolver = new importer.Resolver();

    /** @private {!Array<!TestImportTask>} */
    this.tasks_ = [];
  }

  /** @override */
  importFromScanResult(scan, destination, destinationDirectory) {
    this.imported.push(scan);
    const task = new TestImportTask(scan, destination, destinationDirectory);
    this.tasks_.push(task);
    this.importResolver.resolve(task);
    return this.toMediaImportTask_(task);
  }

  /**
   * Returns |task| as importer.MediaImportHandler.ImportTask type.
   * @param {!Object} task
   * @return {!importer.MediaImportHandler.ImportTask}
   * @private
   */
  toMediaImportTask_(task) {
    return /** @type {!importer.MediaImportHandler.ImportTask} */ (task);
  }

  finishImportTasks() {
    this.tasks_.forEach((task) => task.finish());
  }

  cancelImportTasks() {
    this.finishImportTasks();
  }
}

/**
 * Interface abstracting away the concrete file manager available
 * to commands. By hiding file manager we make it easy to test
 * importer.ImportController.
 *
 * @implements {importer.ControllerEnvironment}
 */
class TestControllerEnvironment {
  /**
   * @param {!VolumeManager} volumeManager
   * @param {!VolumeInfo} volumeInfo
   * @param {!DirectoryEntry} directory
   */
  constructor(volumeManager, volumeInfo, directory) {
    /** @private {!VolumeManager} */
    this.volumeManager = volumeManager;

    /** @private {!VolumeInfo} */
    this.volumeInfo_ = volumeInfo;

    /** @private {!DirectoryEntry} */
    this.directory_ = directory;

    /** @public {function()} */
    this.windowCloseListener;

    /** @public {function(string)} */
    this.volumeUnmountListener;

    /** @public {function(!Event)} */
    this.directoryChangedListener;

    /** @public {function()} */
    this.selectionChangedListener;

    /** @public {!Array<!Entry>} */
    this.selection = [];

    /** @public {boolean} */
    this.isDriveMounted = true;

    /** @public {number} */
    this.freeStorageSpace = 123456789;  // bytes

    /** @public {!importer.Resolver} */
    this.showImportRootResolver = new importer.Resolver();

    /** @public {!importer.Resolver} */
    this.showImportDestinationResolver = new importer.Resolver();
  }

  /** @override */
  getSelection() {
    return this.selection;
  }

  /** @override */
  getCurrentDirectory() {
    return this.directory_;
  }

  /** @override */
  setCurrentDirectory(entry) {
    this.directory_ = entry;
  }

  /** @override */
  getVolumeInfo(entry) {
    return this.volumeInfo_;
  }

  /** @override */
  isGoogleDriveMounted() {
    return this.isDriveMounted;
  }

  /** @override */
  getFreeStorageSpace() {
    return Promise.resolve(this.freeStorageSpace);
  }

  /** @override */
  addWindowCloseListener(listener) {
    this.windowCloseListener = listener;
  }

  /** @override */
  addVolumeUnmountListener(listener) {
    this.volumeUnmountListener = listener;
  }

  /** @override */
  addDirectoryChangedListener(listener) {
    this.directoryChangedListener = listener;
  }

  /** @override */
  addSelectionChangedListener(listener) {
    this.selectionChangedListener = listener;
  }

  /** @override */
  getImportDestination(date) {
    const fileSystem = new MockFileSystem('testFs');
    const directoryEntry = MockDirectoryEntry.create(fileSystem, '/abc/123');
    return Promise.resolve(directoryEntry);
  }

  /** @override */
  showImportDestination() {
    this.showImportDestinationResolver.resolve();
    return Promise.resolve(true);
  }

  /** @override */
  showImportRoot() {
    this.showImportRootResolver.resolve();
    return Promise.resolve(true);
  }

  /**
   * Simulates an unmount event.
   */
  simulateUnmount() {
    this.volumeUnmountListener(this.volumeInfo_.volumeId);
  }
}

/**
 * Test implementation of importer.CommandWidget.
 * @implements {importer.CommandWidget}
 */
importer.TestCommandWidget = class {
  constructor() {
    /** @public {function(importer.ClickSource<string>)} */
    this.clickListener;

    /** @public {!importer.Resolver} */
    this.updateResolver = new importer.Resolver();

    /** @public {!importer.Resolver} */
    this.toggleDetailsResolver = new importer.Resolver();

    /** @public {boolean} */
    this.detailsVisible = false;
  }

  /** Resets the widget */
  resetPromises() {
    this.updateResolver = new importer.Resolver();
    this.toggleDetailsResolver = new importer.Resolver();
  }

  /** @override */
  addClickListener(listener) {
    this.clickListener = listener;
  }

  /**
   * Fires faux click.
   * @param  {!importer.ClickSource} source
   */
  click(source) {
    this.clickListener(source);
  }

  /** @override */
  update(activityState, opt_scan, opt_destinationSizeBytes) {
    assertFalse(
        this.updateResolver.settled,
        'Update promise should not have been settled.');
    this.updateResolver.resolve(activityState);
  }

  updateDetails(scan) {}

  performMainButtonRippleAnimation() {}

  /** @override */
  toggleDetails() {
    assertFalse(
        this.toggleDetailsResolver.settled,
        'Toggle details promise should not have been settled.');
    this.setDetailsVisible(!this.detailsVisible);
    this.toggleDetailsResolver.resolve();
  }

  /** @override */
  setDetailsVisible(visible) {
    this.detailsVisible = visible;
  }

  /** @override */
  setDetailsBannerVisible(visible) {}
};


/**
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {string} volumeId
 * @param {!Array<string>} fileNames
 * @param {string} currentDirectory
 * @return {!importer.ImportController}
 */
function createController(volumeType, volumeId, fileNames, currentDirectory) {
  sourceVolume = setupFileSystem(volumeType, volumeId, fileNames);

  environment = new TestControllerEnvironment(
      volumeManager, sourceVolume,
      sourceVolume.fileSystem.entries[currentDirectory]);

  return new importer.ImportController(
      environment, mediaScanner, mediaImporter, widget);
}

/**
 * @param {!VolumeManagerCommon.VolumeType} volumeType
 * @param {string} volumeId
 * @param {!Array<string>} fileNames
 * @return {!VolumeInfo}
 */
function setupFileSystem(volumeType, volumeId, fileNames) {
  const volumeInfo = volumeManager.createVolumeInfo(
      volumeType, volumeId, 'A volume known as ' + volumeId);
  assertTrue(volumeInfo != null);
  const mockFileSystem = /** @type {!MockFileSystem} */ (volumeInfo.fileSystem);
  mockFileSystem.populate(fileNames);
  return volumeInfo;
}

/**
 * @return {!Metadata}
 */
function getDefaultMetadata() {
  return /** @type {!Metadata} */ ({size: 0});
}
