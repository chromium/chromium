// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {setUpFileManagerOnWindow} from '../../state/for_tests.js';
import {convertEntryToFileData} from '../../state/reducers/all_entries.js';

import {isEntryInsideDrive, isEntryInsideMyDrive} from './entry_utils.js';
import {EntryList} from './files_app_entry_types.js';
import {MockFileSystem} from './mock_entry.js';
import {VolumeManagerCommon} from './volume_manager_types.js';

export function setUp() {
  setUpFileManagerOnWindow();
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
      volumeManager
          .getCurrentProfileVolumeInfo(
              VolumeManagerCommon.VolumeType.DOWNLOADS)!.fileSystem as
      MockFileSystem;
  const driveFs = volumeManager
                      .getCurrentProfileVolumeInfo(
                          VolumeManagerCommon.VolumeType.DRIVE)!.fileSystem as
      MockFileSystem;
  myFilesFs.populate(['/folder1/']);
  driveFs.populate(['/root/folder1']);
  const driveRootEntryList =
      new EntryList('Drive root', VolumeManagerCommon.RootType.DRIVE_FAKE_ROOT);

  // Convert entry into FileData.
  const driveRootFileData = convertEntryToFileData(driveRootEntryList);
  const myDrivesFileData = convertEntryToFileData(driveFs.entries['/root']);
  const teamDrivesFileData =
      convertEntryToFileData(driveFs.entries['/team_drives']);
  const computersFileData =
      convertEntryToFileData(driveFs.entries['/Computers']);
  const myFilesFileData = convertEntryToFileData(myFilesFs.entries['/']);
  const myFilesFolder1FileData =
      convertEntryToFileData(myFilesFs.entries['/folder1']);
  const myDrivesFolder1FileData =
      convertEntryToFileData(driveFs.entries['/root/folder1']);

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
  assertTrue(isEntryInsideDrive(driveRootFileData), 'Drive root');
  assertTrue(isEntryInsideDrive(myDrivesFileData), 'My Drives root');
  assertTrue(isEntryInsideDrive(teamDrivesFileData), 'Team Drives root');
  assertTrue(isEntryInsideDrive(computersFileData), 'Computers root');
  assertFalse(isEntryInsideDrive(myFilesFileData), 'MyFiles root');
  assertFalse(isEntryInsideMyDrive(myFilesFolder1FileData), 'MyFiles folder1');
  assertTrue(isEntryInsideDrive(myDrivesFolder1FileData), 'My Drives folder1');
}
