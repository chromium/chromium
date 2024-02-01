// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import type {VolumeInfo} from '../../background/js/volume_info.js';

import {CombinedReaders, EntryList, FakeEntryImpl, StaticReader, VolumeEntry} from './files_app_entry_types.js';
import {MockFileSystem} from './mock_entry.js';
import {waitUntil} from './test_error_reporting.js';
import {RootType, VolumeType} from './volume_manager_types.js';


function notReached(error: any) {
  assertTrue(false, 'NOTREACHED(): ' + (error.stack || error));
}

/**
 * Creates a new volume with a single, mock VolumeEntry.
 */
function fakeVolumeEntry(
    volumeType: VolumeType|null, displayRoot?: DirectoryEntry,
    additionalProperties?: Object): VolumeEntry {
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
  return new VolumeEntry(fakeVolumeInfo as VolumeInfo);
}

/** Tests constructor and default public attributes. */
export async function testEntryList() {
  const entryList = new EntryList('My files', RootType.MY_FILES);
  assertEquals('My files', entryList.label);
  assertEquals('entry-list://my_files', entryList.toURL());
  assertEquals('my_files', entryList.rootType);
  assertFalse(entryList.isNativeType);
  assertEquals(null, entryList.getNativeEntry());
  assertEquals(0, entryList.getUiChildren().length);
  assertTrue(entryList.isDirectory);
  assertFalse(entryList.isFile);

  entryList.addEntry(new EntryList('Child Entry', RootType.MY_FILES));
  assertEquals(1, entryList.getUiChildren().length);

  const reader = entryList.createReader();
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  // How many times it was called with results?
  let resultCounter = 0;
  const accumulateResults: EntriesCallback = (readerResult) => {
    // It's called with readerResult==[] a last time to indicate no more files.
    callCounter++;
    if (readerResult.length > 0) {
      resultCounter++;
      reader.readEntries(accumulateResults, () => {});
    }
  };

  reader.readEntries(accumulateResults, () => {});
  // readEntries runs asynchronously, so let's wait it to be called.

  // accumulateResults should be called 2x in normal conditions;
  await waitUntil(() => callCounter >= 2);

  // Now we can check the final result.
  assertEquals(2, callCounter);
  assertEquals(1, resultCounter);
}

/** Tests method EntryList.getParent. */
export async function testEntryListGetParent() {
  const entryList = new EntryList('My files', RootType.MY_FILES);
  let callbackTriggered = false;
  entryList.getParent(parentEntry => {
    // EntryList should return itself since it's a root and that's what the web
    // spec says.
    callbackTriggered = true;
    assertEquals(parentEntry, entryList);
  }, notReached /* error */);
  await waitUntil(() => callbackTriggered);
}

/** Tests method EntryList.addEntry. */
export function testEntryListAddEntry() {
  const entryList = new EntryList('My files', RootType.MY_FILES);
  assertEquals(0, entryList.getUiChildren().length);

  const childEntry = fakeVolumeEntry(VolumeType.DOWNLOADS);
  entryList.addEntry(childEntry);
  assertEquals(1, entryList.getUiChildren().length);
  assertEquals(childEntry, entryList.getUiChildren()[0]);
}

/**
 * Tests EntryList's methods addEntry, findIndexByVolumeInfo,
 * removeByVolumeType, removeAllByRootType, removeChildEntry.
 */
export function testEntryFindIndex() {
  const entryList = new EntryList('My files', RootType.MY_FILES);

  const downloads = fakeVolumeEntry(VolumeType.DOWNLOADS);
  const crostini = fakeVolumeEntry(VolumeType.CROSTINI);

  const fakeEntry: Entry = {
    isDirectory: true,
    rootType: RootType.CROSTINI,
    name: 'Linux files',
    toURL: () => 'fake-entry://linux-files',
  } as unknown as Entry;

  entryList.addEntry(downloads);
  entryList.addEntry(crostini);

  // Test findIndexByVolumeInfo.
  assertEquals(0, entryList.findIndexByVolumeInfo(downloads.volumeInfo));
  assertEquals(1, entryList.findIndexByVolumeInfo(crostini.volumeInfo));

  // Test removeByVolumeType.
  assertTrue(entryList.removeByVolumeType(VolumeType.CROSTINI));
  assertEquals(1, entryList.getUiChildren().length);
  // Now Crostini volume doesn't exist anymore, so should return False.
  assertFalse(entryList.removeByVolumeType(VolumeType.CROSTINI));

  // Test removeAllByRootType.
  entryList.addEntry(fakeEntry);
  entryList.addEntry(fakeEntry);
  assertEquals(3, entryList.getUiChildren().length);
  entryList.removeAllByRootType(RootType.CROSTINI);
  assertEquals(1, entryList.getUiChildren().length);

  // Test removeChildEntry.
  assertTrue(entryList.removeChildEntry(entryList.getUiChildren()[0]!));
  assertEquals(0, entryList.getUiChildren().length);
  // Nothing left to remove.
  assertFalse(entryList.removeChildEntry({} as Entry));
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

  const fakeEntry = {
    isDirectory: true,
    rootType: RootType.CROSTINI,
    name: 'Linux files',
    toURL: () => 'fake-entry://linux-files',
  } as unknown as Entry;

  volumeEntry.addEntry(crostini);
  volumeEntry.addEntry(android);

  // Test findIndexByVolumeInfo.
  assertEquals(0, volumeEntry.findIndexByVolumeInfo(crostini.volumeInfo));
  assertEquals(1, volumeEntry.findIndexByVolumeInfo(android.volumeInfo));
  assertEquals(2, volumeEntry.getUiChildren().length);
  assertEquals(crostini, volumeEntry.getUiChildren()[0]);
  assertEquals(android, volumeEntry.getUiChildren()[1]);

  // Test removeByVolumeType.
  assertTrue(volumeEntry.removeByVolumeType(VolumeType.CROSTINI));
  assertEquals(1, volumeEntry.getUiChildren().length);
  // Now crostini volume doesn't exist anymore, so should return False.
  assertFalse(volumeEntry.removeByVolumeType(VolumeType.CROSTINI));

  // Test removeAllByRootType.
  volumeEntry.addEntry(fakeEntry);
  volumeEntry.addEntry(fakeEntry);
  assertEquals(3, volumeEntry.getUiChildren().length);
  volumeEntry.removeAllByRootType(RootType.CROSTINI);
  assertEquals(1, volumeEntry.getUiChildren().length);

  // Test removeChildEntry.
  assertTrue(volumeEntry.removeChildEntry(volumeEntry.getUiChildren()[0]!));
  assertEquals(0, volumeEntry.getUiChildren().length);
  // Nothing left to remove.
  assertFalse(volumeEntry.removeChildEntry({} as Entry));
}

/** Tests method EntryList.getMetadata. */
export async function testEntryListGetMetadata() {
  const entryList = new EntryList('My files', RootType.MY_FILES);

  let modificationTime: Date|null = null;
  entryList.getMetadata(metadata => {
    modificationTime = metadata.modificationTime;
  }, notReached /* error */);

  // getMetadata runs asynchronously, so let's wait it to be called.
  await waitUntil(() => modificationTime !== null);

  // Now we can check the final result, it returns "now", so let's just
  // check the type and 1 attribute here.
  assertTrue(modificationTime! instanceof Date);
  assertTrue(!!modificationTime!.getUTCFullYear());
}

/** Tests StaticReader.readEntries. */
export async function testStaticReader() {
  const file1 = new FakeEntryImpl('file1', RootType.MY_FILES);
  const file2 = new FakeEntryImpl('file2', RootType.MY_FILES);
  const reader = new StaticReader([file1, file2]);
  const testResults: Entry[] = [];
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  const accumulateResults: EntriesCallback = (readerResult) => {
    callCounter++;
    // merge on testResults.
    readerResult.map(f => testResults.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults, () => {});
    }
  };

  reader.readEntries(accumulateResults, () => {});
  // readEntries runs asynchronously, so let's wait it to be called.
  // accumulateResults should be called 2x in normal conditions;
  await waitUntil(() => callCounter >= 2);

  // Now we can check the final result.
  assertEquals(2, callCounter);
  assertEquals(2, testResults.length);
  assertEquals(file1, testResults[0]);
  assertEquals(file2, testResults[1]);
}

/** Tests CombinedReader.readEntries. */
export async function testCombinedReader() {
  const file1 = new FakeEntryImpl('file1', RootType.MY_FILES);
  const file2 = new FakeEntryImpl('file2', RootType.MY_FILES);
  const innerReaders = [
    new StaticReader([file1]),
    new StaticReader([file2]),
  ];
  const reader = new CombinedReaders(innerReaders);
  const testResults: Entry[] = [];
  // How many times the reader callback |accumulateResults| has been called?
  let callCounter = 0;
  const accumulateResults: EntriesCallback = (readerResult) => {
    callCounter++;
    // merge on testResults.
    readerResult.map(f => testResults.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults, () => {});
    }
  };

  reader.readEntries(accumulateResults, () => {});
  // readEntries runs asynchronously, so let's wait it to be called.
  // accumulateResults should be called 2x in normal conditions;
  await waitUntil(() => callCounter >= 3);

  // Now we can check the final result.
  assertEquals(3, callCounter);
  assertEquals(2, testResults.length);
  assertEquals(file1, testResults[0]);
  assertEquals(file2, testResults[1]);
}

export async function testCombinedReaderError() {
  const expectedError = new Error('a fake error');
  const alwaysFailReader = {
    readEntries: (_success: EntriesCallback, error: ErrorCallback) => {
      error(expectedError);
    },
  };
  const file1 = new FakeEntryImpl('file1', RootType.MY_FILES);
  const innerReaders = [
    new StaticReader([file1]),
    alwaysFailReader,
  ];
  const reader = new CombinedReaders(innerReaders);
  const errors: Error[] = [];
  const accumulateFailures = (error: Error) => {
    errors.push(error);
  };

  let callCounter = 0;
  const testResults: any[] = [];
  const accumulateResults = (readerResult: any[]) => {
    callCounter++;
    // merge on testResults.
    readerResult.map((f: any) => testResults.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults, accumulateFailures);
    }
  };


  reader.readEntries(accumulateResults, accumulateFailures);
  // readEntries runs asynchronously, so let's wait it to be called.
  // accumulateResults should be called 2x in normal conditions;
  await waitUntil(() => callCounter >= 1 && errors.length >= 1);

  // Now we can check the final result.
  assertEquals(1, callCounter);
  assertEquals(1, testResults.length);
  assertEquals(file1, testResults[0]);

  assertEquals(1, errors.length);
  assertEquals(expectedError, errors[0]);
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
  assertEquals(
      'filesystem:fake-fs/',
      (volumeEntry.filesystem! as MockFileSystem).rootURL);
  assertEquals('/', volumeEntry.fullPath);
  assertEquals('filesystem:fake-fs/', volumeEntry.toURL());
  assertEquals('Fake Filesystem', volumeEntry.name);
  assertTrue(volumeEntry.isNativeType);
  assertEquals(fakeRootEntry, volumeEntry.getNativeEntry());
  assertTrue(volumeEntry.isDirectory);
  assertFalse(volumeEntry.isFile);
}

export async function testVolumeEntryCreateReader() {
  const fakeRootEntry = createFakeDisplayRoot();
  const file1 = new FakeEntryImpl('file1', RootType.MY_FILES);
  fakeRootEntry.createReader = () => new StaticReader([file1]);
  const volumeEntry = fakeVolumeEntry(VolumeType.DOWNLOADS, fakeRootEntry);
  const crostini = fakeVolumeEntry(VolumeType.CROSTINI);
  const android = fakeVolumeEntry(VolumeType.ANDROID_FILES);

  volumeEntry.addEntry(crostini);
  volumeEntry.addEntry(android);
  const reader = volumeEntry.createReader();

  const readFiles: any[] = [];
  const accumulateResults = (readerResult: any[]) => {
    readerResult.map((f: any) => readFiles.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults);
    }
  };

  reader.readEntries(accumulateResults);
  // readEntries runs asynchronously, so let's wait it to be called.
  await waitUntil(() => readFiles.length >= 3);

  // Now we can check the final result.
  assertEquals(3, readFiles.length);
  assertEquals(file1, readFiles[0]);
  assertEquals(crostini, readFiles[1]);
  assertEquals(android, readFiles[2]);
}

/** Tests VolumeEntry createReader when root entry isn't resolved yet. */
export async function testVolumeEntryCreateReaderUnresolved() {
  // A VolumeInfo that doesn't resolve the display root.
  const fakeVolumeInfo = {
    displayRoot: null,
    label: 'Fake Filesystem label',
    volumeType: VolumeType.DOWNLOADS,
    resolveDisplayRoot: (_onSuccess: any, _onError: any) => {
        // Do nothing here.
    },
  } as unknown as VolumeInfo;

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

  const readFiles: any[] = [];
  const accumulateResults = (readerResult: any[]) => {
    readerResult.map((f: any) => readFiles.push(f));
    if (readerResult.length > 0) {
      reader.readEntries(accumulateResults);
    }
  };

  reader.readEntries(accumulateResults);
  // readEntries runs asynchronously, so let's wait it to be called.
  await waitUntil(() => readFiles.length >= 2);

  // Now we can check the final result.
  assertEquals(2, readFiles.length);
  assertEquals(crostini, readFiles[0]);
  assertEquals(android, readFiles[1]);
}

/** Tests VolumeEntry getFile and getDirectory methods. */
export async function testVolumeEntryGetDirectory() {
  const root = createFakeDisplayRoot();
  (root.filesystem as MockFileSystem).populate(['/bla/', '/bla.txt']);

  const volumeEntry = fakeVolumeEntry(null, root);
  let foundDir: DirectoryEntry|null = null;
  let foundFile: FileEntry|null = null;
  volumeEntry.getDirectory('/bla', {create: false}, (entry) => {
    foundDir = entry;
  });
  volumeEntry.getFile('/bla.txt', {create: false}, (entry) => {
    foundFile = entry;
  });

  await waitUntil(() => foundDir !== null && foundFile !== null);
}

/** Tests VolumeEntry which initially doesn't have displayRoot. */
export async function xtestVolumeEntryDelayedDisplayRoot() {
  let callbackTriggered = false;
  const fakeRootEntry = createFakeDisplayRoot();

  // Create an entry using a VolumeInfo without displayRoot.
  const volumeEntry = fakeVolumeEntry(null, undefined, {
    resolveDisplayRoot:
        (onSuccess: (arg0: DirectoryEntry) => void,
         _onError: (a: any) => void) => {
          setTimeout(() => {
            onSuccess(fakeRootEntry);
            callbackTriggered = true;
          }, 0);
        },
  });

  // rootEntry_ starts as null.
  assertEquals('', volumeEntry.fullPath);
  assertTrue(volumeEntry.isDirectory);
  assertFalse(volumeEntry.isFile);
  assertEquals(null, volumeEntry.getNativeEntry());
  await waitUntil(() => callbackTriggered);
  // Eventually rootEntry_ gets the value.
  assertEquals(fakeRootEntry, volumeEntry.getNativeEntry());
}

/** Tests VolumeEntry.getParent */
export async function testVolumeEntryGetParent() {
  const volumeEntry = fakeVolumeEntry(null);
  let callbackTriggered = false;
  volumeEntry.getParent(parentEntry => {
    callbackTriggered = true;
    // VolumeEntry should return itself since it's a root and that's what the
    // web spec says.
    assertEquals(parentEntry, volumeEntry);
  }, notReached /* error */);
  await waitUntil(() => callbackTriggered);
}

/** Tests VolumeEntry.getMetadata */
export async function testVolumeEntryGetMetadata() {
  const volumeEntry = fakeVolumeEntry(null);
  let modificationTime: Date|null = null;
  volumeEntry.getMetadata(metadata => {
    modificationTime = metadata.modificationTime;
  }, notReached /* error */);

  // getMetadata runs asynchronously, so let's wait it to be called.
  await waitUntil(() => modificationTime !== null);
}

/**
 * Test EntryList.addEntry sets prefix on VolumeEntry.
 */
export function testEntryListAddEntrySetsPrefix() {
  const volumeEntry = fakeVolumeEntry(null);
  const entryList = new EntryList('My files', RootType.MY_FILES);

  entryList.addEntry(volumeEntry);
  assertEquals(1, entryList.getUiChildren().length);
  // entryList is parent of volumeEntry so it should be its prefix.
  assertEquals(entryList, volumeEntry.volumeInfo.prefixEntry);
}

/** Test FakeEntry, which is only static data. */
export async function testFakeEntry() {
  let fakeEntry = new FakeEntryImpl('label', RootType.CROSTINI);

  assertEquals(undefined, fakeEntry.sourceRestriction);
  assertEquals('FakeEntry', fakeEntry.typeName);
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
  fakeEntry = new FakeEntryImpl(
      'label', RootType.CROSTINI,
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE);
  assertEquals(
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE,
      fakeEntry.sourceRestriction);

  let callCounter = 0;

  fakeEntry.getMetadata((metadata) => {
    // Returns default initialized values (current date and 0 size).
    assert(metadata);
    assertEquals(2, Object.keys(metadata).length);
    callCounter++;
  }, notReached /* error */);
  fakeEntry.getParent((parentEntry) => {
    // Should return itself.
    assertEquals(fakeEntry, parentEntry);
    callCounter++;
  }, notReached /* error */);

  // It should be called for getMetadata and for getParent.
  await waitUntil(() => callCounter === 2);
}
