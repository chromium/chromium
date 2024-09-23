// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {fakeMyFilesVolumeId, MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import type {FilesAppEntry} from '../../common/js/files_app_entry_types.js';
import {convertEntryToFileData} from '../../state/ducks/all_entries.js';
import {setUpFileManagerOnWindow} from '../../state/for_tests.js';

import {entryDebugString, isDescendantEntry, isEntryInsideMyDrive, isInsideDrive, readEntriesRecursively} from './entry_utils.js';
import {EntryList, FakeEntryImpl, VolumeEntry} from './files_app_entry_types.js';
import {MockFileSystem} from './mock_entry.js';
import {RootType, VolumeType} from './volume_manager_types.js';

let fileSystem: MockFileSystem;

export function setUp() {
  setUpFileManagerOnWindow();

  fileSystem = new MockFileSystem('fake-volume');
  const filenames = [
    '/file_a.txt',
    '/file_b.txt',
    '/file_c.txt',
    '/file_d.txt',
    '/dir_a/file_e.txt',
    '/dir_a/file_f.txt',
    '/dir_a/dir_b/dir_c/file_g.txt',
  ];
  fileSystem.populate(filenames);
}

/**
 * Test private methods: isEntryInsideDrive_() and isEntryInsideMyDrive_(),
 * which should return true when inside My Drive and any of its sub-directories;
 * Should return false for everything else, including within Team Drive.
 */
export function testInsideMyDriveAndInsideDrive() {
  // Add sub folders for MyFiles and Drive.
  const {volumeManager} = window.fileManager;
  const myFilesFs =
      volumeManager.getCurrentProfileVolumeInfo(
                       VolumeType.DOWNLOADS)!.fileSystem as MockFileSystem;
  const driveFs =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DRIVE)!.fileSystem as
      MockFileSystem;
  myFilesFs.populate(['/folder1/']);
  driveFs.populate(['/root/folder1']);
  const driveRootEntryList =
      new EntryList('Drive root', RootType.DRIVE_FAKE_ROOT);

  // Convert entry into FileData.
  const driveRootFileData = convertEntryToFileData(driveRootEntryList);
  const myDrivesFileData = convertEntryToFileData(driveFs.entries['/root']!);
  const teamDrivesFileData =
      convertEntryToFileData(driveFs.entries['/team_drives']!);
  const computersFileData =
      convertEntryToFileData(driveFs.entries['/Computers']!);
  const myFilesFileData = convertEntryToFileData(myFilesFs.entries['/']!);
  const myFilesFolder1FileData =
      convertEntryToFileData(myFilesFs.entries['/folder1']!);
  const myDrivesFolder1FileData =
      convertEntryToFileData(driveFs.entries['/root/folder1']!);

  // insideMyDrive
  assertFalse(isEntryInsideMyDrive(driveRootFileData), 'Drive root');
  assertTrue(isEntryInsideMyDrive(myDrivesFileData), 'My Drives root');
  assertFalse(isEntryInsideMyDrive(teamDrivesFileData), 'Team Drives root');
  assertFalse(isEntryInsideMyDrive(computersFileData), 'Computers root');
  assertFalse(isEntryInsideMyDrive(myFilesFileData), 'MyFiles root');
  assertFalse(isEntryInsideMyDrive(myFilesFolder1FileData), 'MyFiles folder1');
  assertTrue(
      isEntryInsideMyDrive(myDrivesFolder1FileData), 'My Drives folder1');
  // insideDrive
  assertTrue(isInsideDrive(driveRootFileData), 'Drive root');
  assertTrue(isInsideDrive(myDrivesFileData), 'My Drives root');
  assertTrue(isInsideDrive(teamDrivesFileData), 'Team Drives root');
  assertTrue(isInsideDrive(computersFileData), 'Computers root');
  assertFalse(isInsideDrive(myFilesFileData), 'MyFiles root');
  assertFalse(isEntryInsideMyDrive(myFilesFolder1FileData), 'MyFiles folder1');
  assertTrue(isInsideDrive(myDrivesFolder1FileData), 'My Drives folder1');
}


export function testIsDescendantEntry() {
  const root = fileSystem.root;
  const folder = fileSystem.entries['/dir_a']!;
  const subFolder = fileSystem.entries['/dir_a/dir_b']!;
  const file = fileSystem.entries['/file_a.txt']!;
  const deepFile = fileSystem.entries['/dir_a/dir_b/dir_c/file_g.txt']!;

  const fakeEntry = new FakeEntryImpl('fake-entry-label', RootType.CROSTINI);

  const entryList = new EntryList('entry-list-label', RootType.MY_FILES);
  entryList.addEntry(fakeEntry);

  const volumeManager = new MockVolumeManager();
  // Index 1 is Downloads.
  assertEquals(
      VolumeType.DOWNLOADS, volumeManager.volumeInfoList.item(1).volumeType);
  const downloadsVolumeInfo = volumeManager.volumeInfoList.item(1);
  const mockFs = downloadsVolumeInfo.fileSystem as MockFileSystem;
  mockFs.populate(['/folder1/']);
  const folder1 = mockFs.entries['/folder1']!;

  const volumeEntry = new VolumeEntry(downloadsVolumeInfo);
  volumeEntry.addEntry(fakeEntry);

  // No descendants.
  assertFalse(isDescendantEntry(file!, file));
  assertFalse(isDescendantEntry(root, root));
  assertFalse(isDescendantEntry(deepFile, root));
  assertFalse(isDescendantEntry(subFolder, root));
  assertFalse(isDescendantEntry(fakeEntry, root));
  assertFalse(isDescendantEntry(root, fakeEntry));
  assertFalse(isDescendantEntry(fakeEntry, entryList));
  assertFalse(isDescendantEntry(fakeEntry, volumeEntry));
  assertFalse(isDescendantEntry(folder1, volumeEntry));

  assertTrue(isDescendantEntry(root, file));
  assertTrue(isDescendantEntry(root, subFolder));
  assertTrue(isDescendantEntry(root, deepFile));
  assertTrue(isDescendantEntry(root, folder));
  assertTrue(isDescendantEntry(folder, subFolder));
  assertTrue(isDescendantEntry(folder, deepFile));
  assertTrue(isDescendantEntry(entryList, fakeEntry));
  assertTrue(isDescendantEntry(volumeEntry, fakeEntry));
  assertTrue(isDescendantEntry(volumeEntry, folder1));
}

export function testReadEntriesRecursively(callback: VoidCallback) {
  let foundEntries: Array<Entry|FilesAppEntry> = [];
  readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If all directories are read recursively, found files should be 6.
        assertEquals(7, foundEntries.length);
        callback();
      },
      () => {}, () => false);
}

export function testReadEntriesRecursivelyLevel0(callback: VoidCallback) {
  let foundEntries: Array<Entry|FilesAppEntry> = [];
  readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If only the top directory is read, found entries should be 3.
        assertEquals(4, foundEntries.length);
        callback();
      },
      () => {}, () => false, 0 /* maxDepth */);
}

export function testReadEntriesRecursivelyLevel1(callback: VoidCallback) {
  let foundEntries: Array<Entry|FilesAppEntry> = [];
  readEntriesRecursively(
      fileSystem.root,
      (entries) => {
        const fileEntries = entries.filter(entry => !entry.isDirectory);
        foundEntries = foundEntries.concat(fileEntries);
      },
      () => {
        // If we delve directories only one level, found entries should be 5.
        assertEquals(6, foundEntries.length);
        callback();
      },
      () => {}, () => false, 1 /* maxDepth */);
}

/**
 * Tests that it doesn't fail with different types of entries and inputs.
 */
export function testEntryDebugString() {
  // Check static values.
  assertEquals('entry is null', entryDebugString(null as unknown as Entry));
  (function() {
    assertEquals(
        'entry is undefined', entryDebugString(undefined as unknown as Entry));
    assertEquals('(Object) ', entryDebugString({} as unknown as Entry));
  })();

  // Construct some types of entries.
  const root = fileSystem.root;
  const folder = fileSystem.entries['/dir_a']!;
  const file = fileSystem.entries['/file_a.txt']!;
  const fakeEntry = new FakeEntryImpl('fake-entry-label', RootType.CROSTINI);
  const entryList = new EntryList('entry-list-label', RootType.MY_FILES);
  entryList.addEntry(fakeEntry);
  const volumeManager = new MockVolumeManager();
  // Index 1 is Downloads.
  assertEquals(
      VolumeType.DOWNLOADS, volumeManager.volumeInfoList.item(1).volumeType);
  const downloadsVolumeInfo = volumeManager.volumeInfoList.item(1);
  const mockFs = downloadsVolumeInfo.fileSystem as MockFileSystem;
  mockFs.populate(['/folder1/']);
  const volumeEntry = new VolumeEntry(downloadsVolumeInfo);
  volumeEntry.addEntry(fakeEntry);

  // Mocked values are identified as Object instead of DirectoryEntry and
  // FileEntry.
  assertEquals(
      '(MockDirectoryEntry) / filesystem:fake-volume/', entryDebugString(root));
  assertEquals(
      '(MockDirectoryEntry) /dir_a filesystem:fake-volume/dir_a',
      entryDebugString(folder));
  assertEquals(
      '(MockFileEntry) /file_a.txt filesystem:fake-volume/file_a.txt',
      entryDebugString(file));
  // FilesAppEntry types:
  assertEquals(
      '(FakeEntryImpl) / fake-entry://crostini', entryDebugString(fakeEntry));
  assertEquals(
      '(EntryList) / entry-list://my_files', entryDebugString(entryList));
  assertEquals(
      `(VolumeEntry) / filesystem:${fakeMyFilesVolumeId}/`,
      entryDebugString(volumeEntry));
}
