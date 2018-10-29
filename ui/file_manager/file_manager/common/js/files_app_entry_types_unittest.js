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
  if (displayRoot === undefined)
    displayRoot = createFakeDisplayRoot();
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
  assertEquals(0, entryList.children.length);
  assertTrue(entryList.isDirectory);
  assertFalse(entryList.isFile);

  entryList.addEntry(
      new EntryList('Child Entry', VolumeManagerCommon.RootType.MY_FILES));
  assertEquals(1, entryList.children.length);

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
  assertEquals(0, entryList.children.length);

  const childEntry = fakeVolumeEntry(VolumeManagerCommon.VolumeType.DOWNLOADS);
  entryList.addEntry(childEntry);
  assertEquals(1, entryList.children.length);
  assertEquals(childEntry, entryList.children[0]);
}

/** Tests methods to remove entries. */
function testEntryListRemoveEntry() {
  const entryList =
      new EntryList('My files', VolumeManagerCommon.RootType.MY_FILES);

  const childEntry = fakeVolumeEntry(VolumeManagerCommon.VolumeType.DOWNLOADS);
  entryList.addEntry(childEntry);
  assertTrue(entryList.removeEntry(childEntry));
  assertEquals(0, entryList.children.length);
}

/**
 * Tests methods findIndexByVolumeInfo, removeByVolumeType, removeByRootType.
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
  assertEquals(1, entryList.children.length);
  // Now crostini volume doesn't exist anymore, so should return False.
  assertFalse(
      entryList.removeByVolumeType(VolumeManagerCommon.VolumeType.CROSTINI));

  // Test removeByRootType.
  entryList.addEntry(fakeEntry);
  assertTrue(entryList.removeByRootType(VolumeManagerCommon.RootType.CROSTINI));
  assertEquals(1, entryList.children.length);
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
    if (readerResult.length > 0)
      reader.readEntries(accumulateResults, () => {});
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

/**
 * Returns an object that can be used as displayRoot on a FakeVolumeInfo.
 * VolumeEntry delegates many attributes and methods to displayRoot.
 */
function createFakeDisplayRoot() {
  const fakeRootEntry = {
    filesystem: 'fake-filesystem://',
    fullPath: '/fake/full/path',
    isDirectory: true,
    isFile: false,
    name: 'fs-name',
    toURL: () => {
      return 'fake-filesystem://fake/full/path';
    },
    createReader: () => {
      return 'FAKE READER';
    },
    getMetadata: (success, error) => {
      // Returns static date as modificationTime for testing.
      setTimeout(
          () => success({modificationTime: new Date(Date.UTC(2018, 6, 27))}));
    },
  };
  return fakeRootEntry;
}

/**
 * Tests VolumeEntry constructor and default public attributes/getter/methods.
 */
function testVolumeEntry() {
  const fakeRootEntry = createFakeDisplayRoot();
  const volumeEntry =
      fakeVolumeEntry(VolumeManagerCommon.VolumeType.DOWNLOADS, fakeRootEntry);

  assertEquals(fakeRootEntry, volumeEntry.rootEntry);
  assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS, volumeEntry.iconName);
  assertEquals('fake-filesystem://', volumeEntry.filesystem);
  assertEquals('/fake/full/path', volumeEntry.fullPath);
  assertEquals('fake-filesystem://fake/full/path', volumeEntry.toURL());
  assertEquals('Fake Filesystem', volumeEntry.name);
  assertEquals('FAKE READER', volumeEntry.createReader());
  assertTrue(volumeEntry.isNativeType);
  assertTrue(volumeEntry.isDirectory);
  assertFalse(volumeEntry.isFile);
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

  // rootEntry starts as null.
  assertEquals(null, volumeEntry.rootEntry);
  reportPromise(
      waitUntil(() => callbackTriggered).then(() => {
        // Eventually rootEntry gets the value.
        assertEquals(fakeRootEntry, volumeEntry.rootEntry);
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
      }).then(() => {
        // Now we can check the final result.
        assertEquals(2018, modificationTime.getUTCFullYear());
        // Date() month is 0-based, so 6 == July. :-(
        assertEquals(6, modificationTime.getUTCMonth());
        assertEquals(27, modificationTime.getUTCDate());
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
  assertEquals(1, entryList.children.length);
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
