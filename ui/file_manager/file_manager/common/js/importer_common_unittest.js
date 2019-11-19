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

// Set up the test components.
function setUp() {
  window.loadTimeData.getString = id => id;
  new MockCommandLinePrivate();
  new MockChromeStorageAPI();
  importer.setupTestLogger();

  cameraVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.MTP, 'camera-fs', 'Some Camera');
  sdVolume = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.REMOVABLE, 'sd-fs', 'Some SD Card');
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
  ['/DCIM', '/DCIM/', '/dcim', '/dcim/', '/MP_ROOT/'].forEach(assertIsMediaDir);
  ['/blabbity/DCIM', '/blabbity/dcim', '/blabbity-blab'].forEach(
      assertIsNotMediaDir);
}

function testResolver_Resolve(callback) {
  const resolver = new importer.Resolver();
  assertFalse(resolver.settled);
  resolver.resolve(1);
  resolver.promise.then(value => {
    assertTrue(resolver.settled);
    assertEquals(1, value);
  });

  reportPromise(resolver.promise, callback);
}

function testResolver_Reject(callback) {
  const resolver = new importer.Resolver();
  assertFalse(resolver.settled);
  resolver.reject('ouch');
  resolver.promise.then(callback.bind(null, true)).catch(error => {
    assertTrue(resolver.settled);
    assertEquals('ouch', error);
    callback(false);
  });
}

function testGetMachineId_Persisted(callback) {
  const promise = importer.getMachineId().then(firstMachineId => {
    assertTrue(100000 <= firstMachineId <= 9999999);
    importer.getMachineId().then(secondMachineId => {
      assertEquals(firstMachineId, secondMachineId);
    });
  });
  reportPromise(promise, callback);
}

function testPhotosApp_DefaultDisabled(callback) {
  const promise = importer.isPhotosAppImportEnabled().then(assertFalse);

  reportPromise(promise, callback);
}

function testPhotosApp_ImportEnabled(callback) {
  const promise = importer.handlePhotosAppMessage(true).then(() => {
    return importer.isPhotosAppImportEnabled().then(assertTrue);
  });

  reportPromise(promise, callback);
}

function testPhotosApp_ImportDisabled(callback) {
  const promise = importer.handlePhotosAppMessage(false).then(() => {
    return importer.isPhotosAppImportEnabled().then(assertFalse);
  });

  reportPromise(promise, callback);
}

function testHistoryFilename(callback) {
  const promise = importer.getHistoryFilename().then(firstName => {
    assertTrue(!!firstName && firstName.length > 10);
    importer.getHistoryFilename().then(secondName => {
      assertEquals(firstName, secondName);
    });
  });

  reportPromise(promise, callback);
}

function testLocalStorageWrapper(callback) {
  const storage = new importer.ChromeLocalStorage();
  const promise =
      Promise
          .all([
            storage.set('lamb', 'chop'),
            storage.set('isPoodle', true),
            storage.set('age of grandma', 103),
          ])
          .then(() => {
            return Promise.all([
              storage.get('lamb').then(assertEquals.bind(null, 'chop')),
              storage.get('isPoodle').then(assertEquals.bind(null, true)),
              storage.get('age of grandma').then(assertEquals.bind(null, 103))
            ]);
          });

  reportPromise(promise, callback);
}

function testRotateLogs(callback) {
  let fileName;
  const fileFactory = namePromise => {
    return namePromise.then(name => {
      fileName = name;
      return Promise.resolve(driveFileEntry);
    });
  };

  const nextLogId = 0;
  const lastLogId = 1;
  const promise =
      importer.ChromeLocalStorage.getInstance()
          .set(importer.Setting.LAST_KNOWN_LOG_ID, lastLogId)
          .then(() => {
            // Should delete the log with the nextLogId.
            return importer.rotateLogs(nextLogId, fileFactory);
          })
          .then(() => {
            assertTrue(fileName !== undefined);
            // Verify the *active* log is deleted.
            assertEquals(
                0, fileName.search(/[0-9]{6}-import-debug-0.log/),
                'Filename (' + fileName + ') does not match next pattern.');
            driveFileEntry.asMock().assertRemoved();

            return importer.ChromeLocalStorage.getInstance()
                .get(importer.Setting.LAST_KNOWN_LOG_ID)
                .then(assertEquals.bind(null, nextLogId));
          });

  reportPromise(promise, callback);
}

function testRotateLogs_RemembersInitialActiveLog(callback) {
  const nextLogId = 1;

  // Should not be called.
  const fileFactory = (namePromise) => {
    assertFalse(true);
    return Promise.resolve();
  };

  const promise = importer.rotateLogs(nextLogId, fileFactory).then(() => {
    return importer.ChromeLocalStorage.getInstance()
        .get(importer.Setting.LAST_KNOWN_LOG_ID)
        .then(assertEquals.bind(null, nextLogId));
  });

  reportPromise(promise, callback);
}

function testDeflateAppUrl() {
  const url = 'filesystem:chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj' +
      '/external/removable/USB%20Drive/DCIM/derekkind2.jpg';
  const deflated = importer.deflateAppUrl(url);

  // Just verify that the string starts with our secret sauce marker...
  assertEquals(
      '$', deflated.substring(0, 1),
      'Deflated URLs must beging with the deflation marker.');

  // And that it is shorter than the original...
  assertTrue(
      deflated.length < url.length,
      'Deflated URLs must be shorter than the original.');

  // And finally that we can reconstitute the original.
  assertEquals(
      url, importer.inflateAppUrl(deflated),
      'Deflated then inflated URLs must match original URL.');
}

function testHasMediaDirectory(callback) {
  const dir = createDirectoryEntry(sdVolume, '/DCIM');
  const promise = importer.hasMediaDirectory(sdVolume.fileSystem.root)
                      .then(assertTrue.bind(null));

  reportPromise(promise, callback);
}

/** @param {string} path */
function assertIsMediaDir(path) {
  const dir = createDirectoryEntry(sdVolume, path);
  assertTrue(importer.isMediaDirectory(dir, volumeManager));
}

/** @param {string} path */
function assertIsNotMediaDir(path) {
  const dir = createDirectoryEntry(sdVolume, path);
  assertFalse(importer.isMediaDirectory(dir, volumeManager));
}

function createFileEntry(volume, path) {
  const entry =
      MockFileEntry.create(volume.fileSystem, path, /** @type{Metadata} */ ({
                             size: 1234,
                             modificationTime: new Date().toString()
                           }));
  // Ensure the file entry has a volumeID...necessary for lookups
  // via the VolumeManager.
  entry.volumeId = volume.volumeId;
  return entry;
}

function createDirectoryEntry(volume, path) {
  const entry = MockDirectoryEntry.create(volume.fileSystem, path);
  // Ensure the file entry has a volumeID...necessary for lookups
  // via the VolumeManager.
  entry.volumeId = volume.volumeId;
  return entry;
}
