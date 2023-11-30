// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/ash/common/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {CombinedReaders, EntryList, FakeEntryImpl, StaticReader, VolumeEntry} from './files_app_entry_types.js';
import {MockFileSystem} from './mock_entry.js';
import {reportPromise, waitUntil} from './test_error_reporting.js';
import {RootType, VolumeType} from './volume_manager_types.js';


// @ts-ignore: error TS7006: Parameter 'error' implicitly has an 'any' type.
function notreached(error) {
  assertTrue(false, 'NOTREACHED(): ' + (error.stack || error));
}

/**
 * Creates a new volume with a single, mock VolumeEntry.
 * @param {?VolumeType} volumeType
 * @param {DirectoryEntry=} displayRoot
 * @param {Object=} additionalProperties
 * @return {!VolumeEntry}
 */
function fakeVolumeEntry(volumeType, displayRoot, additionalProperties) {
  const kLabel = 'Fake Filesystem';
  if (displayRoot === undefined) {
    displayRoot = createFakeDisplayRoot();
  }
  const fakeVolumeInfo = {
    volumeId: `id:${volumeType}`,
    displayRoot: displayRoot,
    label: kLabel,
    volumeType: volumeType,
  };
  Object.assign(fakeVolumeInfo, additionalProperties || {});
  // Create the VolumeEntry via casting (duck typing).
  return new VolumeEntry(
      /** @type{!import("../../externs/volume_info.js").VolumeInfo} */ (
          fakeVolumeInfo));
}

/**
 * Test constructor and default public attributes.
 * @param {()=>void} done
 */
export function testEntryList(done) {
  const entryList = new EntryList('My files', RootType.MY_FILES);
  assertEquals('My files', entryList.label);
  assertEquals('entry-list://my_files', entryList.toURL());
  assertEquals('my_files', entryList.rootType);
  assertFalse(entryList.isNativeType);
  assertEquals(null, entryList.getNativeEntry());
  assertEquals(0, entryList.getUIChildren().length);
  assertTrue(entryList.isDirectory);
  assertFalse(entryList.isFile);

  entryList.addEntry(new EntryList('Child Entry', RootType.MY_FILES));
  assertEquals(1, entryList.getUIChildren().length);

  const reader = entryList.createReader();
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  // How many times it was called with results?
  let resultCouter = 0;
  // @ts-ignore: error TS7006: Parameter 'readerResult' implicitly has an 'any'
  // type.
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
      done);
}

/**
 * Tests method EntryList.getParent.
 * @param {()=>void} done
 */
export function testEntryListGetParent(done) {
  const entryList = new EntryList('My files', RootType.MY_FILES);
  let callbackTriggered = false;
  entryList.getParent(parentEntry => {
    // EntryList should return itself since it's a root and that's what the web
    // spec says.
    callbackTriggered = true;
    assertEquals(parentEntry, entryList);
  }, notreached /* error */);
  reportPromise(waitUntil(() => callbackTriggered), done);
}

/** Tests method EntryList.addEntry. */
export function testEntryListAddEntry() {
  const entryList = new EntryList('My files', RootType.MY_FILES);
  assertEquals(0, entryList.getUIChildren().length);

  const childEntry = fakeVolumeEntry(VolumeType.DOWNLOADS);
  entryList.addEntry(childEntry);
  assertEquals(1, entryList.getUIChildren().length);
  assertEquals(childEntry, entryList.getUIChildren()[0]);
}

/**
 * Tests EntryList's methods addEntry, findIndexByVolumeInfo,
 * removeByVolumeType, removeAllByRootType, removeChildEntry.
 */
export function testEntryFindIndex() {
  const entryList = new EntryList('My files', RootType.MY_FILES);

  const downloads = fakeVolumeEntry(VolumeType.DOWNLOADS);
  const crostini = fakeVolumeEntry(VolumeType.CROSTINI);

  // @ts-ignore: error TS2352: Conversion of type '{ isDirectory: true;
  // rootType: string; name: string; toURL: () => string; }' to type
  // 'FileSystemEntry' may be a mistake because neither type sufficiently
  // overlaps with the other. If this was intentional, convert the expression to
  // 'unknown' first.
  const fakeEntry = /** @type{!Entry} */ ({
    isDirectory: true,
    rootType: RootType.CROSTINI,
    name: 'Linux files',
    toURL: function() {
      return 'fake-entry://linux-files';
    },
  });

  entryList.addEntry(downloads);
  entryList.addEntry(crostini);

  // Test findIndexByVolumeInfo.
  assertEquals(0, entryList.findIndexByVolumeInfo(downloads.volumeInfo));
  assertEquals(1, entryList.findIndexByVolumeInfo(crostini.volumeInfo));

  // Test removeByVolumeType.
  assertTrue(entryList.removeByVolumeType(VolumeType.CROSTINI));
  assertEquals(1, entryList.getUIChildren().length);
  // Now crostini volume doesn't exist anymore, so should return False.
  assertFalse(entryList.removeByVolumeType(VolumeType.CROSTINI));

  // Test removeAllByRootType.
  entryList.addEntry(fakeEntry);
  entryList.addEntry(fakeEntry);
  assertEquals(3, entryList.getUIChildren().length);
  entryList.removeAllByRootType(RootType.CROSTINI);
  assertEquals(1, entryList.getUIChildren().length);

  // Test removeChildEntry.
  // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | FilesAppEntry
  // | undefined' is not assignable to parameter of type 'FileSystemEntry |
  // FilesAppEntry'.
  assertTrue(entryList.removeChildEntry(entryList.getUIChildren()[0]));
  assertEquals(0, entryList.getUIChildren().length);
  // Nothing left to remove.
  assertFalse(entryList.removeChildEntry(/** @type {Entry} */ ({})));
}

/**
 * Tests VolumeEntry's methods findIndexByVolumeInfo, removeByVolumeType,
 * removeAllByRootType, removeChildEntry.
 */
export function testVolumeEntryFindIndex() {
  const fakeRootEntry = createFakeDisplayRoot();
  const volumeEntry = fakeVolumeEntry(VolumeType.DOWNLOADS, fakeRootEntry);

  const crostini = fakeVolumeEntry(VolumeType.CROSTINI);
  const android = fakeVolumeEntry(VolumeType.ANDROID_FILES);

  // @ts-ignore: error TS2352: Conversion of type '{ isDirectory: true;
  // rootType: string; name: string; toURL: () => string; }' to type
  // 'FileSystemEntry' may be a mistake because neither type sufficiently
  // overlaps with the other. If this was intentional, convert the expression to
  // 'unknown' first.
  const fakeEntry = /** @type{!Entry} */ ({
    isDirectory: true,
    rootType: RootType.CROSTINI,
    name: 'Linux files',
    toURL: function() {
      return 'fake-entry://linux-files';
    },
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
  assertTrue(volumeEntry.removeByVolumeType(VolumeType.CROSTINI));
  // @ts-ignore: error TS2341: Property 'children_' is private and only
  // accessible within class 'VolumeEntry'.
  assertEquals(1, volumeEntry.children_.length);
  // Now crostini volume doesn't exist anymore, so should return False.
  assertFalse(volumeEntry.removeByVolumeType(VolumeType.CROSTINI));

  // Test removeAllByRootType.
  volumeEntry.addEntry(fakeEntry);
  volumeEntry.addEntry(fakeEntry);
  // @ts-ignore: error TS2341: Property 'children_' is private and only
  // accessible within class 'VolumeEntry'.
  assertEquals(3, volumeEntry.children_.length);
  volumeEntry.removeAllByRootType(RootType.CROSTINI);
  // @ts-ignore: error TS2341: Property 'children_' is private and only
  // accessible within class 'VolumeEntry'.
  assertEquals(1, volumeEntry.children_.length);

  // Test removeChildEntry.
  // @ts-ignore: error TS2345: Argument of type 'FileSystemEntry | FilesAppEntry
  // | undefined' is not assignable to parameter of type 'FileSystemEntry |
  // FilesAppEntry'.
  assertTrue(volumeEntry.removeChildEntry(volumeEntry.getUIChildren()[0]));
  assertEquals(0, volumeEntry.getUIChildren().length);
  // Nothing left to remove.
  assertFalse(volumeEntry.removeChildEntry(/** @type {Entry} */ ({})));
}

/**
 * Tests method EntryList.getMetadata.
 * @param {()=>void} done
 */
export function testEntryListGetMetadata(done) {
  const entryList = new EntryList('My files', RootType.MY_FILES);

  // @ts-ignore: error TS7034: Variable 'modificationTime' implicitly has type
  // 'any' in some locations where its type cannot be determined.
  let modificationTime = null;
  entryList.getMetadata(metadata => {
    modificationTime = metadata.modificationTime;
  }, notreached /* error */);

  // getMetadata runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        // @ts-ignore: error TS7005: Variable 'modificationTime' implicitly has
        // an 'any' type.
        return modificationTime !== null;
      }).then(() => {
        // Now we can check the final result, it returns "now", so let's just
        // check the type and 1 attribute here.
        // @ts-ignore: error TS7005: Variable 'modificationTime' implicitly has
        // an 'any' type.
        assertTrue(modificationTime instanceof Date);
        // @ts-ignore: error TS7005: Variable 'modificationTime' implicitly has
        // an 'any' type.
        assertTrue(!!modificationTime.getUTCFullYear());
      }),
      done);
}

/**
 * Tests StaticReader.readEntries.
 * @param {()=>void} done
 */
export function testStaticReader(done) {
  // @ts-ignore: error TS2322: Type 'string' is not assignable to type
  // 'FileSystemEntry | FilesAppEntry'.
  const reader = new StaticReader(['file1', 'file2']);
  // @ts-ignore: error TS7034: Variable 'testResults' implicitly has type
  // 'any[]' in some locations where its type cannot be determined.
  const testResults = [];
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  // @ts-ignore: error TS7006: Parameter 'readerResult' implicitly has an 'any'
  // type.
  const accumulateResults = (readerResult) => {
    callCounter++;
    // merge on testResults.
    // @ts-ignore: error TS7006: Parameter 'f' implicitly has an 'any' type.
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
        // @ts-ignore: error TS7005: Variable 'testResults' implicitly has an
        // 'any[]' type.
        assertEquals('file1', testResults[0]);
        // @ts-ignore: error TS7005: Variable 'testResults' implicitly has an
        // 'any[]' type.
        assertEquals('file2', testResults[1]);
      }),
      done);
}

/**
 * Tests CombinedReader.readEntries.
 * @param {()=>void} done
 */
export function testCombinedReader(done) {
  const innerReaders = [
    // @ts-ignore: error TS2322: Type 'string' is not assignable to type
    // 'FileSystemEntry | FilesAppEntry'.
    new StaticReader(['file1']),
    // @ts-ignore: error TS2322: Type 'string' is not assignable to type
    // 'FileSystemEntry | FilesAppEntry'.
    new StaticReader(['file2']),
  ];
  const reader = new CombinedReaders(innerReaders);
  // @ts-ignore: error TS7034: Variable 'testResults' implicitly has type
  // 'any[]' in some locations where its type cannot be determined.
  const testResults = [];
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  // @ts-ignore: error TS7006: Parameter 'readerResult' implicitly has an 'any'
  // type.
  const accumulateResults = (readerResult) => {
    callCounter++;
    // merge on testResults.
    // @ts-ignore: error TS7006: Parameter 'f' implicitly has an 'any' type.
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
        // @ts-ignore: error TS7005: Variable 'testResults' implicitly has an
        // 'any[]' type.
        assertEquals('file1', testResults[0]);
        // @ts-ignore: error TS7005: Variable 'testResults' implicitly has an
        // 'any[]' type.
        assertEquals('file2', testResults[1]);
      }),
      done);
}

/**
 * @param {()=>void} done
 */
export function testCombinedReaderError(done) {
  const expectedError = new Error('a fake error');
  const alwaysFailReader = {
    // @ts-ignore: error TS7006: Parameter 'error' implicitly has an 'any' type.
    readEntries: (success, error) => {
      error(expectedError);
    },
  };
  const innerReaders = [
    // @ts-ignore: error TS2322: Type 'string' is not assignable to type
    // 'FileSystemEntry | FilesAppEntry'.
    new StaticReader(['file1']),
    alwaysFailReader,
  ];
  const reader = new CombinedReaders(innerReaders);
  // @ts-ignore: error TS7034: Variable 'errors' implicitly has type 'any[]' in
  // some locations where its type cannot be determined.
  const errors = [];
  // @ts-ignore: error TS7006: Parameter 'error' implicitly has an 'any' type.
  const accumulateFailures = (error) => {
    errors.push(error);
  };

  let callCounter = 0;
  // @ts-ignore: error TS7034: Variable 'testResults' implicitly has type
  // 'any[]' in some locations where its type cannot be determined.
  const testResults = [];
  // @ts-ignore: error TS7006: Parameter 'readerResult' implicitly has an 'any'
  // type.
  const accumulateResults = (readerResult) => {
    callCounter++;
    // merge on testResults.
    // @ts-ignore: error TS7006: Parameter 'f' implicitly has an 'any' type.
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
        // @ts-ignore: error TS7005: Variable 'testResults' implicitly has an
        // 'any[]' type.
        assertEquals('file1', testResults[0]);

        assertEquals(1, errors.length);
        // @ts-ignore: error TS7005: Variable 'errors' implicitly has an 'any[]'
        // type.
        assertEquals(expectedError, errors[0]);
      }),
      done);
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
export function testVolumeEntry() {
  const fakeRootEntry = createFakeDisplayRoot();
  const volumeEntry = fakeVolumeEntry(VolumeType.DOWNLOADS, fakeRootEntry);

  assertEquals(fakeRootEntry, volumeEntry.getNativeEntry());
  // Downloads volume is displayed with MyFiles icon.
  assertEquals(VolumeType.MY_FILES, volumeEntry.iconName);
  // @ts-ignore: error TS2339: Property 'rootURL' does not exist on type
  // 'FileSystem'.
  assertEquals('filesystem:fake-fs/', volumeEntry.filesystem.rootURL);
  assertEquals('/', volumeEntry.fullPath);
  assertEquals('filesystem:fake-fs/', volumeEntry.toURL());
  assertEquals('Fake Filesystem', volumeEntry.name);
  assertTrue(volumeEntry.isNativeType);
  assertEquals(fakeRootEntry, volumeEntry.getNativeEntry());
  assertTrue(volumeEntry.isDirectory);
  assertFalse(volumeEntry.isFile);
}

/**
 * @param {()=>void} done
 */
export function testVolumeEntryCreateReader(done) {
  const fakeRootEntry = createFakeDisplayRoot();
  // @ts-ignore: error TS2322: Type 'string' is not assignable to type
  // 'FileSystemEntry | FilesAppEntry'.
  fakeRootEntry.createReader = () => new StaticReader(['file1']);
  const volumeEntry = fakeVolumeEntry(VolumeType.DOWNLOADS, fakeRootEntry);
  const crostini = fakeVolumeEntry(VolumeType.CROSTINI);
  const android = fakeVolumeEntry(VolumeType.ANDROID_FILES);

  volumeEntry.addEntry(crostini);
  volumeEntry.addEntry(android);
  const reader = volumeEntry.createReader();

  // @ts-ignore: error TS7034: Variable 'readFiles' implicitly has type 'any[]'
  // in some locations where its type cannot be determined.
  const readFiles = [];
  // @ts-ignore: error TS7006: Parameter 'readerResult' implicitly has an 'any'
  // type.
  const accumulateResults = (readerResult) => {
    // @ts-ignore: error TS7006: Parameter 'f' implicitly has an 'any' type.
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
        // @ts-ignore: error TS7005: Variable 'readFiles' implicitly has an
        // 'any[]' type.
        assertEquals('file1', readFiles[0]);
        // @ts-ignore: error TS7005: Variable 'readFiles' implicitly has an
        // 'any[]' type.
        assertEquals(crostini, readFiles[1]);
        // @ts-ignore: error TS7005: Variable 'readFiles' implicitly has an
        // 'any[]' type.
        assertEquals(android, readFiles[2]);
      }),
      done);
}

/**
 * Tests VolumeEntry createReader when root entry isn't resolved yet.
 * @param {()=>void} done
 */
export function testVolumeEntryCreateReaderUnresolved(done) {
  // A VolumeInfo that doesn't resolve the display root.
  const fakeVolumeInfo =
      // @ts-ignore: error TS2352: Conversion of type '{ displayRoot: null;
      // label: string; volumeType: string; resolveDisplayRoot:
      // (successCallback: ((entry: FileSystemDirectoryEntry) => void) |
      // undefined, errorCallback: ((_: any) => void) | undefined) => void; }'
      // to type 'VolumeInfo' may be a mistake because neither type sufficiently
      // overlaps with the other. If this was intentional, convert the
      // expression to 'unknown' first.
      /** @type{!import("../../externs/volume_info.js").VolumeInfo} */ ({
        displayRoot: null,
        label: 'Fake Filesystem label',
        volumeType: VolumeType.DOWNLOADS,
        // @ts-ignore: error TS6133: 'errorCallback' is declared but its value
        // is never read.
        resolveDisplayRoot: (successCallback, errorCallback) => {
            // Do nothing here.
        },
      });

  const volumeEntry = new VolumeEntry(fakeVolumeInfo);
  const crostini = fakeVolumeEntry(VolumeType.CROSTINI);
  const android = fakeVolumeEntry(VolumeType.ANDROID_FILES);

  assertEquals(null, volumeEntry.filesystem);
  assertEquals('', volumeEntry.fullPath);
  assertEquals('', volumeEntry.toURL());
  assertTrue(volumeEntry.isDirectory);
  assertFalse(volumeEntry.isFile);

  volumeEntry.addEntry(crostini);
  volumeEntry.addEntry(android);
  const reader = volumeEntry.createReader();

  // @ts-ignore: error TS7034: Variable 'readFiles' implicitly has type 'any[]'
  // in some locations where its type cannot be determined.
  const readFiles = [];
  // @ts-ignore: error TS7006: Parameter 'readerResult' implicitly has an 'any'
  // type.
  const accumulateResults = (readerResult) => {
    // @ts-ignore: error TS7006: Parameter 'f' implicitly has an 'any' type.
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
        // @ts-ignore: error TS7005: Variable 'readFiles' implicitly has an
        // 'any[]' type.
        assertEquals(crostini, readFiles[0]);
        // @ts-ignore: error TS7005: Variable 'readFiles' implicitly has an
        // 'any[]' type.
        assertEquals(android, readFiles[1]);
      }),
      done);
}

/**
 * Tests VolumeEntry getFile and getDirectory methods.
 * @param {()=>void} done
 */
export function testVolumeEntryGetDirectory(done) {
  const root = createFakeDisplayRoot();
  // @ts-ignore: error TS2339: Property 'populate' does not exist on type
  // 'FileSystem'.
  root.filesystem.populate(['/bla/', '/bla.txt']);

  const volumeEntry = fakeVolumeEntry(null, root);
  // @ts-ignore: error TS7034: Variable 'foundDir' implicitly has type 'any' in
  // some locations where its type cannot be determined.
  let foundDir = null;
  // @ts-ignore: error TS7034: Variable 'foundFile' implicitly has type 'any' in
  // some locations where its type cannot be determined.
  let foundFile = null;
  volumeEntry.getDirectory('/bla', {create: false}, (entry) => {
    foundDir = entry;
  });
  volumeEntry.getFile('/bla.txt', {create: false}, (entry) => {
    foundFile = entry;
  });

  reportPromise(
      waitUntil(() => {
        // @ts-ignore: error TS7005: Variable 'foundFile' implicitly has an
        // 'any' type.
        return foundDir !== null && foundFile !== null;
      }),
      done);
}

/**
 * Tests VolumeEntry which initially doesn't have displayRoot.
 * @param {()=>void} done
 */
export function testVolumeEntryDelayedDisplayRoot(done) {
  let callbackTriggered = false;
  const fakeRootEntry = createFakeDisplayRoot();

  // Create an entry using a VolumeInfo without displayRoot.
  // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
  // parameter of type 'FileSystemDirectoryEntry | undefined'.
  const volumeEntry = fakeVolumeEntry(null, null, {
    // @ts-ignore: error TS7006: Parameter 'errorCallback' implicitly has an
    // 'any' type.
    resolveDisplayRoot: function(successCallback, errorCallback) {
      setTimeout(() => {
        successCallback(fakeRootEntry);
        callbackTriggered = true;
      }, 0);
    },
  });

  // rootEntry_ starts as null.
  assertEquals(null, volumeEntry.rootEntry_);
  assertEquals(null, volumeEntry.getNativeEntry());
  reportPromise(
      waitUntil(() => callbackTriggered).then(() => {
        // Eventually rootEntry_ gets the value.
        assertEquals(fakeRootEntry, volumeEntry.getNativeEntry());
      }),
      done);
}

/**
 * Tests VolumeEntry.getParent
 * @param {()=>void} done
 */
export function testVolumeEntryGetParent(done) {
  const volumeEntry = fakeVolumeEntry(null);
  let callbackTriggered = false;
  volumeEntry.getParent(parentEntry => {
    callbackTriggered = true;
    // VolumeEntry should return itself since it's a root and that's what the
    // web spec says.
    assertEquals(parentEntry, volumeEntry);
  }, notreached /* error */);
  reportPromise(waitUntil(() => callbackTriggered), done);
}

/**
 * Tests VolumeEntry.getMetadata
 * @param {()=>void} done
 */
export function testVolumeEntryGetMetadata(done) {
  const volumeEntry = fakeVolumeEntry(null);
  // @ts-ignore: error TS7034: Variable 'modificationTime' implicitly has type
  // 'any' in some locations where its type cannot be determined.
  let modificationTime = null;
  volumeEntry.getMetadata(metadata => {
    modificationTime = metadata.modificationTime;
  }, notreached /* error */);

  // getMetadata runs asynchronously, so let's wait it to be called.
  reportPromise(
      waitUntil(() => {
        // @ts-ignore: error TS7005: Variable 'modificationTime' implicitly has
        // an 'any' type.
        return modificationTime !== null;
      }),
      done);
}

/**
 * Test EntryList.addEntry sets prefix on VolumeEntry.
 */
export function testEntryListAddEntrySetsPrefix() {
  const volumeEntry = fakeVolumeEntry(null);
  const entryList = new EntryList('My files', RootType.MY_FILES);

  entryList.addEntry(volumeEntry);
  assertEquals(1, entryList.getUIChildren().length);
  // entryList is parent of volumeEntry so it should be its prefix.
  assertEquals(entryList, volumeEntry.volumeInfo.prefixEntry);
}

/**
 * Test FakeEntry, which is only static data.
 * @param {()=>void} done
 */
export function testFakeEntry(done) {
  let fakeEntry = new FakeEntryImpl('label', RootType.CROSTINI);

  assertEquals(undefined, fakeEntry.sourceRestriction);
  assertEquals('FakeEntry', fakeEntry.type_name);
  assertEquals('label', fakeEntry.label);
  assertEquals('label', fakeEntry.name);
  assertEquals('fake-entry://crostini', fakeEntry.toURL());
  assertEquals('crostini', fakeEntry.iconName);
  assertEquals(RootType.CROSTINI, fakeEntry.rootType);
  assertFalse(fakeEntry.isNativeType);
  assertEquals(null, fakeEntry.getNativeEntry());
  assertTrue(fakeEntry.isDirectory);
  assertFalse(fakeEntry.isFile);

  // Check sourceRestriction constructor args.
  const kSourceRestriction =
      /** @type{chrome.fileManagerPrivate.SourceRestriction} */ ('fake');
  fakeEntry = new FakeEntryImpl('label', RootType.CROSTINI, kSourceRestriction);
  assertEquals(kSourceRestriction, fakeEntry.sourceRestriction);

  let callCounter = 0;

  fakeEntry.getMetadata((metadata) => {
    // Returns default initialized values (current date and 0 size).
    assert(metadata);
    assertEquals(2, Object.keys(metadata).length);
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
      done);
}
