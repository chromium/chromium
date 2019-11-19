// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

function notreached(error) {
  assertTrue(false, 'NOTREACHED(): ' + (error.stack || error));
}

/**
 * Creates a new volume with a single, mock VolumeEntry.
 * @param {?VolumeManagerCommon.VolumeType} volumeType
 * @param {DirectoryEntry=} displayRoot
 * @param {Object=} additionalProperties
 * @return {!VolumeEntry}
 */
function fakeVolumeEntry(volumeType, displayRoot, additionalProperties) {
  const kLabel = 'Fake Filesystem';
  if (displayRoot === undefined) {
    displayRoot = createFakeDisplayRoot();
  }
  let fakeVolumeInfo = {
    displayRoot: displayRoot,
    label: kLabel,
    volumeType: volumeType
  };
  Object.assign(fakeVolumeInfo, additionalProperties || {});
  // Create the VolumeEntry via casting (duck typing).
  return new VolumeEntry(/** @type{!VolumeInfo} */ (fakeVolumeInfo));
}

/**  Test constructor and default public attributes. */
function testEntryList(testReportCallback) {
  const entryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);
  assertEquals('My files', entryList.label);
  assertEquals('entry-list://my_files', entryList.toURL());
  assertEquals('my_files', entryList.rootType);
  assertFalse(entryList.isNativeType);
  assertEquals(null, entryList.getNativeEntry());
  assertEquals(0, entryList.getUIChildren().length);
  assertTrue(entryList.isDirectory);
  assertFalse(entryList.isFile);

  entryList.addEntry(
      new EntryList('Child Entry', VolumeManagerCommon.RootType.MY_FILES));
  assertEquals(1, entryList.getUIChildren().length);

  const reader = entryList.createReader();
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  // How many times it was called with results?
  let resultCouter = 0;
  const accumulateResults = (readerResult) => {
    // It's called with readerResult==[] a last time to indicate no more files.
    callCounter++;
    if (readerResult.length > 0) {
      resultCouter++;
      reader.readEntries(accumulateResults, () => {});
    }
  };

  reader.readEntries(accumulateResults, () => {});
  // readEntries runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        // accumulateResults should be called 2x in normal conditions;
        return callCounter >= 2;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(2, callCounter);
        assertEquals(1, resultCouter);
      }),
      testReportCallback);
}

/** Tests method EntryList.getParent. */
function testEntryListGetParent(testReportCallback) {
  const entryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);
  let callbackTriggered = false;
  entryList.getParent(parentEntry => {
    // EntryList should return itself since it's a root and that's what the web
    // spec says.
    callbackTriggered = true;
    assertEquals(parentEntry, entryList);
  }, notreached /* error */);
  reportPromise(waitUntil(() => callbackTriggered), testReportCallback);
}

/** Tests method EntryList.addEntry. */
function testEntryListAddEntry() {
  const entryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);
  assertEquals(0, entryList.getUIChildren().length);

  const childEntry = fakeVolumeEntry(VolumeManagerCommon.VolumeType.DOWNLOADS);
  entryList.addEntry(childEntry);
  assertEquals(1, entryList.getUIChildren().length);
  assertEquals(childEntry, entryList.getUIChildren()[0]);
}

/**
 * Tests EntryList's methods addEntry, findIndexByVolumeInfo,
 * removeByVolumeType, removeByRootType, removeChildEntry.
 */
function testEntryFindIndex() {
  const entryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);

  const downloads = fakeVolumeEntry(VolumeManagerCommon.VolumeType.DOWNLOADS);
  const crostini = fakeVolumeEntry(VolumeManagerCommon.VolumeType.CROSTINI);

  const fakeEntry = /** @type{!Entry} */ ({
    isDirectory: true,
    rootType: VolumeManagerCommon.RootType.CROSTINI,
    name: 'Linux files',
    toURL: function() {
      return 'fake-entry://linux-files';
    }
  });

  entryList.addEntry(downloads);
  entryList.addEntry(crostini);

  // Test findIndexByVolumeInfo.
  assertEquals(0, entryList.findIndexByVolumeInfo(downloads.volumeInfo));
  assertEquals(1, entryList.findIndexByVolumeInfo(crostini.volumeInfo));

  // Test removeByVolumeType.
  assertTrue(
      entryList.removeByVolumeType(VolumeManagerCommon.VolumeType.CROSTINI));
  assertEquals(1, entryList.getUIChildren().length);
  // Now crostini volume doesn't exist anymore, so should return False.
  assertFalse(
      entryList.removeByVolumeType(VolumeManagerCommon.VolumeType.CROSTINI));

  // Test removeByRootType.
  entryList.addEntry(fakeEntry);
  assertTrue(entryList.removeByRootType(VolumeManagerCommon.RootType.CROSTINI));
  assertEquals(1, entryList.getUIChildren().length);

  // Test removeChildEntry.
  assertTrue(entryList.removeChildEntry(entryList.getUIChildren()[0]));
  assertEquals(0, entryList.getUIChildren().length);
  // Nothing left to remove.
  assertFalse(entryList.removeChildEntry(/** @type {Entry} */ ({})));
}

/**
 * Tests VolumeEntry's methods findIndexByVolumeInfo, removeByVolumeType,
 * removeByRootType, removeChildEntry.
 * @suppress {accessControls} to be able to access private properties.
 */
function testVolumeEntryFindIndex() {
  const fakeRootEntry = createFakeDisplayRoot();
  const volumeEntry =
      fakeVolumeEntry(VolumeManagerCommon.VolumeType.DOWNLOADS, fakeRootEntry);

  const crostini = fakeVolumeEntry(VolumeManagerCommon.VolumeType.CROSTINI);
  const android = fakeVolumeEntry(VolumeManagerCommon.VolumeType.ANDROID_FILES);

  const fakeEntry = /** @type{!Entry} */ ({
    isDirectory: true,
    rootType: VolumeManagerCommon.RootType.CROSTINI,
    name: 'Linux files',
    toURL: function() {
      return 'fake-entry://linux-files';
    }
  });

  volumeEntry.addEntry(crostini);
  volumeEntry.addEntry(android);

  // Test findIndexByVolumeInfo.
  assertEquals(0, volumeEntry.findIndexByVolumeInfo(crostini.volumeInfo));
  assertEquals(1, volumeEntry.findIndexByVolumeInfo(android.volumeInfo));
  assertEquals(2, volumeEntry.getUIChildren().length);
  assertEquals(crostini, volumeEntry.getUIChildren()[0]);
  assertEquals(android, volumeEntry.getUIChildren()[1]);

  // Test removeByVolumeType.
  assertTrue(
      volumeEntry.removeByVolumeType(VolumeManagerCommon.VolumeType.CROSTINI));
  assertEquals(1, volumeEntry.children_.length);
  // Now crostini volume doesn't exist anymore, so should return False.
  assertFalse(
      volumeEntry.removeByVolumeType(VolumeManagerCommon.VolumeType.CROSTINI));

  // Test removeByRootType.
  volumeEntry.addEntry(fakeEntry);
  assertTrue(
      volumeEntry.removeByRootType(VolumeManagerCommon.RootType.CROSTINI));
  assertEquals(1, volumeEntry.children_.length);

  // Test removeChildEntry.
  assertTrue(volumeEntry.removeChildEntry(volumeEntry.getUIChildren()[0]));
  assertEquals(0, volumeEntry.getUIChildren().length);
  // Nothing left to remove.
  assertFalse(volumeEntry.removeChildEntry(/** @type {Entry} */ ({})));
}

/** Tests method EntryList.getMetadata. */
function testEntryListGetMetadata(testReportCallback) {
  const entryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);

  let modificationTime = null;
  entryList.getMetadata(metadata => {
    modificationTime = metadata.modificationTime;
  }, notreached /* error */);

  // getMetadata runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        return modificationTime !== null;
      }).then(() => {
        // Now we can check the final result, it returns "now", so let's just
        // check the type and 1 attribute here.
        assertTrue(modificationTime instanceof Date);
        assertTrue(!!modificationTime.getUTCFullYear());
      }),
      testReportCallback);
}

/** Tests StaticReader.readEntries. */
function testStaticReader(testReportCallback) {
  const reader = new StaticReader(['file1', 'file2']);
  const testResults = [];
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  const accumulateResults = (readerResult) => {
    callCounter++;
    // merge on testResults.
    readerResult.map(f => testResults.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults, () => {});
    }
  };

  reader.readEntries(accumulateResults, () => {});
  // readEntries runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        // accumulateResults should be called 2x in normal conditions;
        return callCounter >= 2;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(2, callCounter);
        assertEquals(2, testResults.length);
        assertEquals('file1', testResults[0]);
        assertEquals('file2', testResults[1]);
      }),
      testReportCallback);
}

/** Tests CombinedReader.readEntries. */
function testCombinedReader(testReportCallback) {
  const innerReaders = [
    new StaticReader(['file1']),
    new StaticReader(['file2']),
  ];
  const reader = new CombinedReaders(innerReaders);
  const testResults = [];
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  const accumulateResults = (readerResult) => {
    callCounter++;
    // merge on testResults.
    readerResult.map(f => testResults.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults, () => {});
    }
  };

  reader.readEntries(accumulateResults, () => {});
  // readEntries runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        // accumulateResults should be called 2x in normal conditions;
        return callCounter >= 3;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(3, callCounter);
        assertEquals(2, testResults.length);
        assertEquals('file1', testResults[0]);
        assertEquals('file2', testResults[1]);
      }),
      testReportCallback);
}

function testCombinedReaderError(testReportCallback) {
  const expectedError = new Error('a fake error');
  const alwaysFailReader = {
    readEntries: (success, error) => {
      error(expectedError);
    },
  };
  const innerReaders = [
    new StaticReader(['file1']),
    alwaysFailReader,
  ];
  const reader = new CombinedReaders(innerReaders);
  const errors = [];
  const accumulateFailures = (error) => {
    errors.push(error);
  };

  let callCounter = 0;
  const testResults = [];
  const accumulateResults = (readerResult) => {
    callCounter++;
    // merge on testResults.
    readerResult.map(f => testResults.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults, accumulateFailures);
    }
  };


  reader.readEntries(accumulateResults, accumulateFailures);
  // readEntries runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        // accumulateResults should be called 2x in normal conditions;
        return callCounter >= 1 && errors.length >= 1;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(1, callCounter);
        assertEquals(1, testResults.length);
        assertEquals('file1', testResults[0]);

        assertEquals(1, errors.length);
        assertEquals(expectedError, errors[0]);
      }),
      testReportCallback);
}

/**
 * Returns an object that can be used as displayRoot on a FakeVolumeInfo.
 * VolumeEntry delegates many attributes and methods to displayRoot.
 */
function createFakeDisplayRoot() {
  const fs = new MockFileSystem('fake-fs');
  return fs.root;
}

/**
 * Tests VolumeEntry constructor and default public attributes/getter/methods.
 */
function testVolumeEntry() {
  const fakeRootEntry = createFakeDisplayRoot();
  const volumeEntry =
      fakeVolumeEntry(VolumeManagerCommon.VolumeType.DOWNLOADS, fakeRootEntry);

  assertEquals(fakeRootEntry, volumeEntry.getNativeEntry());
  assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS, volumeEntry.iconName);
  assertEquals('filesystem:fake-fs/', volumeEntry.filesystem.rootURL);
  assertEquals('/', volumeEntry.fullPath);
  assertEquals('filesystem:fake-fs/', volumeEntry.toURL());
  assertEquals('Fake Filesystem', volumeEntry.name);
  assertTrue(volumeEntry.isNativeType);
  assertEquals(fakeRootEntry, volumeEntry.getNativeEntry());
  assertTrue(volumeEntry.isDirectory);
  assertFalse(volumeEntry.isFile);
}

function testVolumeEntryCreateReader(testReportCallback) {
  const fakeRootEntry = createFakeDisplayRoot();
  fakeRootEntry.createReader = () => new StaticReader(['file1']);
  const volumeEntry =
      fakeVolumeEntry(VolumeManagerCommon.VolumeType.DOWNLOADS, fakeRootEntry);
  const crostini = fakeVolumeEntry(VolumeManagerCommon.VolumeType.CROSTINI);
  const android = fakeVolumeEntry(VolumeManagerCommon.VolumeType.ANDROID_FILES);

  volumeEntry.addEntry(crostini);
  volumeEntry.addEntry(android);
  const reader = volumeEntry.createReader();

  const readFiles = [];
  const accumulateResults = (readerResult) => {
    readerResult.map((f) => readFiles.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults);
    }
  };

  reader.readEntries(accumulateResults);
  // readEntries runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        return readFiles.length >= 3;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(3, readFiles.length);
        assertEquals('file1', readFiles[0]);
        assertEquals(crostini, readFiles[1]);
        assertEquals(android, readFiles[2]);
      }),
      testReportCallback);
}

/** Tests VolumeEntry createReader when root entry isn't resolved yet. */
function testVolumeEntryCreateReaderUnresolved(testReportCallback) {
  // A VolumeInfo that doesn't resolve the display root.
  const fakeVolumeInfo = /** @type{!VolumeInfo} */ ({
    displayRoot: null,
    label: 'Fake Filesystem label',
    volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
    resolveDisplayRoot: (successCallback, errorCallback) => {
      // Do nothing here.
    },
  });

  const volumeEntry = new VolumeEntry(fakeVolumeInfo);
  const crostini = fakeVolumeEntry(VolumeManagerCommon.VolumeType.CROSTINI);
  const android = fakeVolumeEntry(VolumeManagerCommon.VolumeType.ANDROID_FILES);

  assertEquals(null, volumeEntry.filesystem);
  assertEquals('', volumeEntry.fullPath);
  assertEquals('', volumeEntry.toURL());
  assertTrue(volumeEntry.isDirectory);
  assertFalse(volumeEntry.isFile);

  volumeEntry.addEntry(crostini);
  volumeEntry.addEntry(android);
  const reader = volumeEntry.createReader();

  const readFiles = [];
  const accumulateResults = (readerResult) => {
    readerResult.map((f) => readFiles.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults);
    }
  };

  reader.readEntries(accumulateResults);
  // readEntries runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        return readFiles.length >= 2;
      }).then(() => {
        // Now we can check the final result.
        assertEquals(2, readFiles.length);
        assertEquals(crostini, readFiles[0]);
        assertEquals(android, readFiles[1]);
      }),
      testReportCallback);
}

/**
 * Tests VolumeEntry getFile and getDirectory methods.
 */
function testVolumeEntryGetDirectory(testReportCallback) {
  const root = createFakeDisplayRoot();
  root.filesystem.populate(['/bla/', '/bla.txt']);

  const volumeEntry = fakeVolumeEntry(null, root);
  let foundDir = null;
  let foundFile = null;
  volumeEntry.getDirectory('/bla', {create: false}, (entry) => {
    foundDir = entry;
  });
  volumeEntry.getFile('/bla.txt', {create: false}, (entry) => {
    foundFile = entry;
  });

  reportPromise(
      waitUntil(() => {
        return foundDir !== null && foundFile !== null;
      }),
      testReportCallback);
}

/**
 * Tests VolumeEntry which initially doesn't have displayRoot.
 */
function testVolumeEntryDelayedDisplayRoot(testReportCallback) {
  let callbackTriggered = false;
  const fakeRootEntry = createFakeDisplayRoot();

  // Create an entry using a VolumeInfo without displayRoot.
  const volumeEntry = fakeVolumeEntry(null, null, {
    resolveDisplayRoot: function(successCallback, errorCallback) {
      setTimeout(() => {
        successCallback(fakeRootEntry);
        callbackTriggered = true;
      }, 0);
    }
  });

  // rootEntry_ starts as null.
  assertEquals(null, volumeEntry.rootEntry_);
  assertEquals(null, volumeEntry.getNativeEntry());
  reportPromise(
      waitUntil(() => callbackTriggered).then(() => {
        // Eventually rootEntry_ gets the value.
        assertEquals(fakeRootEntry, volumeEntry.getNativeEntry());
      }),
      testReportCallback);
}

/** Tests VolumeEntry.getParent */
function testVolumeEntryGetParent(testReportCallback) {
  const volumeEntry = fakeVolumeEntry(null);
  let callbackTriggered = false;
  volumeEntry.getParent(parentEntry => {
    callbackTriggered = true;
    // VolumeEntry should return itself since it's a root and that's what the
    // web spec says.
    assertEquals(parentEntry, volumeEntry);
  }, notreached /* error */);
  reportPromise(waitUntil(() => callbackTriggered), testReportCallback);
}

/**  Tests VolumeEntry.getMetadata */
function testVolumeEntryGetMetadata(testReportCallback) {
  const volumeEntry = fakeVolumeEntry(null);
  let modificationTime = null;
  volumeEntry.getMetadata(metadata => {
    modificationTime = metadata.modificationTime;
  }, notreached /* error */);

  // getMetadata runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        return modificationTime !== null;
      }),
      testReportCallback);
}

/**
 * Test EntryList.addEntry sets prefix on VolumeEntry.
 */
function testEntryListAddEntrySetsPrefix() {
  const volumeEntry = fakeVolumeEntry(null);
  const entryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);

  entryList.addEntry(volumeEntry);
  assertEquals(1, entryList.getUIChildren().length);
  // entryList is parent of volumeEntry so it should be its prefix.
  assertEquals(entryList, volumeEntry.volumeInfo.prefixEntry);
}

/**
 * Test FakeEntry, which is only static data.
 */
function testFakeEntry(testReportCallback) {
  let fakeEntry = new FakeEntry('label', VolumeManagerCommon.RootType.CROSTINI);

  assertEquals(undefined, fakeEntry.sourceRestriction);
  assertEquals('FakeEntry', fakeEntry.type_name);
  assertEquals('label', fakeEntry.label);
  assertEquals('label', fakeEntry.name);
  assertEquals('fake-entry://crostini', fakeEntry.toURL());
  assertEquals('crostini', fakeEntry.iconName);
  assertEquals(VolumeManagerCommon.RootType.CROSTINI, fakeEntry.rootType);
  assertFalse(fakeEntry.isNativeType);
  assertEquals(null, fakeEntry.getNativeEntry());
  assertTrue(fakeEntry.isDirectory);
  assertFalse(fakeEntry.isFile);

  // Check sourceRestriction constructor args.
  const kSourceRestriction =
      /** @type{chrome.fileManagerPrivate.SourceRestriction} */ ('fake');
  fakeEntry = new FakeEntry(
      'label', VolumeManagerCommon.RootType.CROSTINI, kSourceRestriction);
  assertEquals(kSourceRestriction, fakeEntry.sourceRestriction);

  let callCounter = 0;

  fakeEntry.getMetadata((metadata) => {
    // Returns empty (but non-null) metadata {}.
    assert(metadata);
    assertEquals(0, Object.keys(metadata).length);
    callCounter++;
  }, notreached /* error */);
  fakeEntry.getParent((parentEntry) => {
    // Should return itself.
    assertEquals(fakeEntry, parentEntry);
    callCounter++;
  }, notreached /* error */);

  reportPromise(
      waitUntil(() => {
        // It should be called for getMetadata and for getParent.
        return callCounter == 2;
      }),
      testReportCallback);
}
