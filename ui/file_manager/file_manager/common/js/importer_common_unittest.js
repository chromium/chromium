// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @type {!MockVolumeManager} */
let volumeManager;

/** @type {!VolumeInfo} */
let cameraVolume;

/** @type {!VolumeInfo} */
let sdVolume;

/** @type {VolumeInfo} */
let driveVolume;

/** @type {!MockFileEntry} */
let cameraFileEntry;

/** @type {!MockFileEntry} */
let rawFileEntry;

/** @type {!MockFileEntry} */
let sdFileEntry;

/** @type {!MockFileEntry} */
let driveFileEntry;

// Sadly, boilerplate setup necessary to include test support classes.
loadTimeData.data = {
  DRIVE_DIRECTORY_LABEL: 'My Drive',
  DOWNLOADS_DIRECTORY_LABEL: 'Downloads'
};

// Set up the test components.
function setUp() {
  new MockCommandLinePrivate();
  new MockChromeStorageAPI();
  importer.setupTestLogger();

  cameraVolume = MockVolumeManager.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.MTP,
          'camera-fs',
          'Some Camera');
  sdVolume = MockVolumeManager.createMockVolumeInfo(
          VolumeManagerCommon.VolumeType.REMOVABLE,
          'sd-fs',
          'Some SD Card');
  volumeManager = new MockVolumeManager();
  volumeManager.volumeInfoList.add(cameraVolume);
  volumeManager.volumeInfoList.add(sdVolume);
  driveVolume = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  cameraFileEntry = createFileEntry(cameraVolume, '/DCIM/poodles.jpg');
  rawFileEntry = createFileEntry(cameraVolume, '/DCIM/poodles.nef');
  sdFileEntry = createFileEntry(sdVolume, '/dcim/a-z/IMG1234.jpg');
  driveFileEntry = createFileEntry(driveVolume, '/someotherfile.jpg');
}

function testIsEligibleType() {
  assertTrue(importer.isEligibleType(cameraFileEntry));
  assertTrue(importer.isEligibleType(rawFileEntry));

  // Agnostic to the location of the entry.
  assertTrue(importer.isEligibleType(driveFileEntry));
}

function testIsEligibleVolume() {
  assertTrue(importer.isEligibleVolume(cameraVolume));
  assertTrue(importer.isEligibleVolume(sdVolume));
  assertFalse(importer.isEligibleVolume(driveVolume));
}

function testIsEligibleEntry() {
  assertTrue(importer.isEligibleEntry(volumeManager, cameraFileEntry));
  assertTrue(importer.isEligibleEntry(volumeManager, sdFileEntry));
  assertTrue(importer.isEligibleEntry(volumeManager, rawFileEntry));
  assertFalse(importer.isEligibleEntry(volumeManager, driveFileEntry));
}

function testIsMediaDirectory() {
  ['/DCIM', '/DCIM/', '/dcim', '/dcim/', '/MP_ROOT/' ].forEach(
      assertIsMediaDir);
  ['/blabbity/DCIM', '/blabbity/dcim', '/blabbity-blab'].forEach(
      assertIsNotMediaDir);
}

function testResolver_Resolve(callback) {
  var resolver = new importer.Resolver();
  assertFalse(resolver.settled);
  resolver.resolve(1);
  resolver.promise.then(
      function(value) {
        assertTrue(resolver.settled);
        assertEquals(1, value);
      });

  reportPromise(resolver.promise, callback);
}

function testResolver_Reject(callback) {
  var resolver = new importer.Resolver();
  assertFalse(resolver.settled);
  resolver.reject('ouch');
  resolver.promise
      .then(callback.bind(null, true))
      .catch(
          function(error) {
            assertTrue(resolver.settled);
            assertEquals('ouch', error);
            callback(false);
          });
}

function testGetMachineId_Persisted(callback) {
  var promise = importer.getMachineId().then(
      function(firstMachineId) {
        assertTrue(100000 <= firstMachineId <= 9999999);
        importer.getMachineId().then(
            function(secondMachineId) {
              assertEquals(firstMachineId, secondMachineId);
            });
      });
  reportPromise(promise, callback);
}

function testPhotosApp_DefaultDisabled(callback) {
  var promise = importer.isPhotosAppImportEnabled().then(assertFalse);

  reportPromise(promise, callback);
}

function testPhotosApp_ImportEnabled(callback) {
  var promise = importer.handlePhotosAppMessage(true).then(
      function() {
        return importer.isPhotosAppImportEnabled().then(assertTrue);
      });

  reportPromise(promise, callback);
}

function testPhotosApp_ImportDisabled(callback) {
  var promise = importer.handlePhotosAppMessage(false).then(
      function() {
        return importer.isPhotosAppImportEnabled().then(assertFalse);
      });

  reportPromise(promise, callback);
}

function testHistoryFilename(callback) {
  var promise = importer.getHistoryFilename().then(
      function(firstName) {
        assertTrue(!!firstName && firstName.length > 10);
        importer.getHistoryFilename().then(
            function(secondName) {
              assertEquals(firstName, secondName);
            });
      });

  reportPromise(promise, callback);
}

function testLocalStorageWrapper(callback) {
  var storage = new importer.ChromeLocalStorage();
  var promise = Promise.all([
    storage.set('lamb', 'chop'),
    storage.set('isPoodle', true),
    storage.set('age of grandma', 103)
  ]).then(
      function() {
        return Promise.all([
          storage.get('lamb').then(assertEquals.bind(null, 'chop')),
          storage.get('isPoodle').then(assertEquals.bind(null, true)),
          storage.get('age of grandma').then(assertEquals.bind(null, 103))
        ]);
      });

  reportPromise(promise, callback);
}

function testRotateLogs(callback) {
  var fileName;
  var fileFactory = function(namePromise) {
    return namePromise.then(
        function(name) {
          fileName = name;
          return Promise.resolve(driveFileEntry);
        });
  };

  var nextLogId = 0;
  var lastLogId = 1;
  var promise = importer.ChromeLocalStorage.getInstance()
      .set(importer.Setting.LAST_KNOWN_LOG_ID, lastLogId)
      .then(
          function() {
            // Should delete the log with the nextLogId.
            return importer.rotateLogs(nextLogId, fileFactory);
          })
      .then(
          function() {
            assertTrue(fileName !== undefined);
            // Verify the *active* log is deleted.
            assertEquals(0, fileName.search(/[0-9]{6}-import-debug-0.log/),
                'Filename (' + fileName + ') does not match next pattern.');
            driveFileEntry.asMock().assertRemoved();

            return importer.ChromeLocalStorage.getInstance()
                  .get(importer.Setting.LAST_KNOWN_LOG_ID)
                  .then(assertEquals.bind(null, nextLogId));
          });

  reportPromise(promise, callback);
}

function testRotateLogs_RemembersInitialActiveLog(callback) {
  var nextLogId = 1;

  // Should not be called.
  var fileFactory = (namePromise) => {
    assertFalse(true);
    return Promise.resolve();
  };

  var promise =
      importer.rotateLogs(nextLogId, fileFactory)
          .then(function() {
            return importer.ChromeLocalStorage.getInstance()
                  .get(importer.Setting.LAST_KNOWN_LOG_ID)
                  .then(assertEquals.bind(null, nextLogId));
          });

  reportPromise(promise, callback);
}

function testDeflateAppUrl() {
  var url = 'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj' +
      '/external/removable/USB%20Drive/DCIM/derekkind2.jpg';
  var deflated = importer.deflateAppUrl(url);

  // Just verify that the string starts with our secret sauce marker...
  assertEquals('$', deflated.substring(0, 1),
      'Deflated URLs must beging with the deflation marker.');

  // And that it is shorter than the original...
  assertTrue(deflated.length < url.length,
      'Deflated URLs must be shorter than the original.');

  // And finally that we can reconstitute the original.
  assertEquals(url, importer.inflateAppUrl(deflated),
      'Deflated then inflated URLs must match original URL.');
}

function testHasMediaDirectory(callback) {
  var dir = createDirectoryEntry(sdVolume, '/DCIM');
  var promise = importer.hasMediaDirectory(sdVolume.fileSystem.root)
      .then(assertTrue.bind(null));

  reportPromise(promise, callback);
}

/** @param {string} path */
function assertIsMediaDir(path) {
  var dir = createDirectoryEntry(sdVolume, path);
  assertTrue(importer.isMediaDirectory(dir, volumeManager));
}

/** @param {string} path */
function assertIsNotMediaDir(path) {
  var dir = createDirectoryEntry(sdVolume, path);
  assertFalse(importer.isMediaDirectory(dir, volumeManager));
}

function createFileEntry(volume, path) {
  var entry =
      new MockFileEntry(volume.fileSystem, path, /** @type{Metadata} */ ({
                          size: 1234,
                          modificationTime: new Date().toString()
                        }));
  // Ensure the file entry has a volumeID...necessary for lookups
  // via the VolumeManager.
  entry.volumeId = volume.volumeId;
  return entry;
}

function createDirectoryEntry(volume, path) {
  var entry = new MockDirectoryEntry(volume.fileSystem, path);
  // Ensure the file entry has a volumeID...necessary for lookups
  // via the VolumeManager.
  entry.volumeId = volume.volumeId;
  return entry;
}
