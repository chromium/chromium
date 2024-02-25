// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {fakeDriveVolumeId, fakeMyFilesVolumeId, MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import type {MockFileSystem} from '../../common/js/mock_entry.js';
import {str} from '../../common/js/translations.js';
import {VolumeType} from '../../common/js/volume_manager_types.js';

import {PathComponent} from './path_component.js';

type LabelAndEntryUrl = [string, string];

export async function testComputeComponentsFromEntry() {
  const volumeManager = new MockVolumeManager();
  window.webkitResolveLocalFileSystemURL =
      MockVolumeManager.resolveLocalFileSystemUrl.bind(null, volumeManager);
  const driveVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DRIVE);
  if (!driveVolumeInfo) {
    throw new Error('Failed to get the drive volume info');
  }
  await driveVolumeInfo.resolveDisplayRoot();
  let fs = driveVolumeInfo.fileSystem as MockFileSystem;

  async function validate(path: string, components: LabelAndEntryUrl[]) {
    fs.populate([path]);
    const result = PathComponent.computeComponentsFromEntry(
        fs.entries[path]!, volumeManager);
    assertEquals(components.length, result.length);
    for (let i = 0; i < components.length; i++) {
      const c = components[i]!;
      assertEquals(c[0], result[i]?.name);
      const entry = await result[i]?.resolveEntry();
      assertEquals(c[1], entry?.toURL());
    }
  }

  // Drive volume.
  // .files-by-id.
  await validate('/.files-by-id/1234/file', [
    [
      str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
      'fake-entry://drive_shared_with_me',
    ],
    ['file', `filesystem:${fakeDriveVolumeId}/.files-by-id/1234/file`],
  ]);
  await validate('/.files-by-id/1234/a/file', [
    [
      str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
      'fake-entry://drive_shared_with_me',
    ],
    ['a', `filesystem:${fakeDriveVolumeId}/.files-by-id/1234/a`],
    ['file', `filesystem:${fakeDriveVolumeId}/.files-by-id/1234/a/file`],
  ]);
  // .shortcut-targets-by-id.
  await validate('/.shortcut-targets-by-id/1-abc-xyz/file', [
    [
      str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
      'fake-entry://drive_shared_with_me',
    ],
    [
      'file',
      `filesystem:${fakeDriveVolumeId}/.shortcut-targets-by-id/1-abc-xyz/file`,
    ],
  ]);
  await validate('/.shortcut-targets-by-id/1-abc-xyz/a/file', [
    [
      str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
      'fake-entry://drive_shared_with_me',
    ],
    [
      'a',
      `filesystem:${fakeDriveVolumeId}/.shortcut-targets-by-id/1-abc-xyz/a`,
    ],
    [
      'file',
      `filesystem:${
          fakeDriveVolumeId}/.shortcut-targets-by-id/1-abc-xyz/a/file`,
    ],
  ]);
  // Computers.
  await validate('/Computers/C1/file', [
    [str('DRIVE_COMPUTERS_LABEL'), `filesystem:${fakeDriveVolumeId}/Computers`],
    ['C1', `filesystem:${fakeDriveVolumeId}/Computers/C1`],
    ['file', `filesystem:${fakeDriveVolumeId}/Computers/C1/file`],
  ]);
  await validate('/Computers/C1/a/file', [
    [str('DRIVE_COMPUTERS_LABEL'), `filesystem:${fakeDriveVolumeId}/Computers`],
    ['C1', `filesystem:${fakeDriveVolumeId}/Computers/C1`],
    ['a', `filesystem:${fakeDriveVolumeId}/Computers/C1/a`],
    ['file', `filesystem:${fakeDriveVolumeId}/Computers/C1/a/file`],
  ]);
  // root.
  await validate('/root/file', [
    [str('DRIVE_MY_DRIVE_LABEL'), `filesystem:${fakeDriveVolumeId}/root`],
    ['file', `filesystem:${fakeDriveVolumeId}/root/file`],
  ]);
  await validate('/root/a/file', [
    [str('DRIVE_MY_DRIVE_LABEL'), `filesystem:${fakeDriveVolumeId}/root`],
    ['a', `filesystem:${fakeDriveVolumeId}/root/a`],
    ['file', `filesystem:${fakeDriveVolumeId}/root/a/file`],
  ]);
  // team_drives.
  await validate('/team_drives/S1/file', [
    [
      str('DRIVE_SHARED_DRIVES_LABEL'),
      `filesystem:${fakeDriveVolumeId}/team_drives`,
    ],
    ['S1', `filesystem:${fakeDriveVolumeId}/team_drives/S1`],
    ['file', `filesystem:${fakeDriveVolumeId}/team_drives/S1/file`],
  ]);
  await validate('/team_drives/S1/a/file', [
    [
      str('DRIVE_SHARED_DRIVES_LABEL'),
      `filesystem:${fakeDriveVolumeId}/team_drives`,
    ],
    ['S1', `filesystem:${fakeDriveVolumeId}/team_drives/S1`],
    ['a', `filesystem:${fakeDriveVolumeId}/team_drives/S1/a`],
    ['file', `filesystem:${fakeDriveVolumeId}/team_drives/S1/a/file`],
  ]);

  const downloadsVolumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DOWNLOADS);
  if (!downloadsVolumeInfo) {
    throw new Error('Failed to get the drive volume info');
  }
  fs = downloadsVolumeInfo.fileSystem as MockFileSystem;

  // Downloads.
  await validate('/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), `filesystem:${fakeMyFilesVolumeId}/`],
    ['file', `filesystem:${fakeMyFilesVolumeId}/file`],
  ]);
  await validate('/a/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), `filesystem:${fakeMyFilesVolumeId}/`],
    ['a', `filesystem:${fakeMyFilesVolumeId}/a`],
    ['file', `filesystem:${fakeMyFilesVolumeId}/a/file`],
  ]);

  // Special labels for '/Downloads', '/PvmDefault', '/Camera'.
  await validate('/Downloads/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), `filesystem:${fakeMyFilesVolumeId}/`],
    [
      str('DOWNLOADS_DIRECTORY_LABEL'),
      `filesystem:${fakeMyFilesVolumeId}/Downloads`,
    ],
    ['file', `filesystem:${fakeMyFilesVolumeId}/Downloads/file`],
  ]);
  await validate('/PvmDefault/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), `filesystem:${fakeMyFilesVolumeId}/`],
    [
      str('PLUGIN_VM_DIRECTORY_LABEL'),
      `filesystem:${fakeMyFilesVolumeId}/PvmDefault`,
    ],
    ['file', `filesystem:${fakeMyFilesVolumeId}/PvmDefault/file`],
  ]);
  await validate('/Camera/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), `filesystem:${fakeMyFilesVolumeId}/`],
    [str('CAMERA_DIRECTORY_LABEL'), `filesystem:${fakeMyFilesVolumeId}/Camera`],
    ['file', `filesystem:${fakeMyFilesVolumeId}/Camera/file`],
  ]);
}
