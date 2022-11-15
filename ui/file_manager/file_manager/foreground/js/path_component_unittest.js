// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {MockFileSystem} from '../../common/js/mock_entry.js';
import {str} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

import {PathComponent} from './path_component.js';

export async function testComputeComponentsFromEntry() {
  const volumeManager = new MockVolumeManager();
  window.webkitResolveLocalFileSystemURL =
      MockVolumeManager.resolveLocalFileSystemURL.bind(null, volumeManager);
  const driveVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);
  await driveVolumeInfo.resolveDisplayRoot();
  let fs = /** @type {!MockFileSystem} */ (driveVolumeInfo.fileSystem);

  async function validate(path, components) {
    fs.populate([path]);
    const result = PathComponent.computeComponentsFromEntry(
        fs.entries[path], volumeManager);
    assertEquals(components.length, result.length);
    for (let i = 0; i < components.length; i++) {
      const c = components[i];
      assertEquals(c[0], result[i].name);
      const entry = await result[i].resolveEntry();
      assertEquals(c[1], entry.toURL());
    }
  }

  // Drive volume.
  // .files-by-id.
  await validate('/.files-by-id/1234/file', [
    [
      str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
      'fake-entry://drive_shared_with_me',
    ],
    ['file', 'filesystem:drive/.files-by-id/1234/file'],
  ]);
  await validate('/.files-by-id/1234/a/file', [
    [
      str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
      'fake-entry://drive_shared_with_me',
    ],
    ['a', 'filesystem:drive/.files-by-id/1234/a'],
    ['file', 'filesystem:drive/.files-by-id/1234/a/file'],
  ]);
  // .shortcut-targets-by-id.
  await validate('/.shortcut-targets-by-id/1-abc-xyz/file', [
    [
      str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
      'fake-entry://drive_shared_with_me',
    ],
    ['file', 'filesystem:drive/.shortcut-targets-by-id/1-abc-xyz/file'],
  ]);
  await validate('/.shortcut-targets-by-id/1-abc-xyz/a/file', [
    [
      str('DRIVE_SHARED_WITH_ME_COLLECTION_LABEL'),
      'fake-entry://drive_shared_with_me',
    ],
    ['a', 'filesystem:drive/.shortcut-targets-by-id/1-abc-xyz/a'],
    ['file', 'filesystem:drive/.shortcut-targets-by-id/1-abc-xyz/a/file'],
  ]);
  // Computers.
  await validate('/Computers/C1/file', [
    [str('DRIVE_COMPUTERS_LABEL'), 'filesystem:drive/Computers'],
    ['C1', 'filesystem:drive/Computers/C1'],
    ['file', 'filesystem:drive/Computers/C1/file'],
  ]);
  await validate('/Computers/C1/a/file', [
    [str('DRIVE_COMPUTERS_LABEL'), 'filesystem:drive/Computers'],
    ['C1', 'filesystem:drive/Computers/C1'],
    ['a', 'filesystem:drive/Computers/C1/a'],
    ['file', 'filesystem:drive/Computers/C1/a/file'],
  ]);
  // root.
  await validate('/root/file', [
    [str('DRIVE_MY_DRIVE_LABEL'), 'filesystem:drive/root'],
    ['file', 'filesystem:drive/root/file'],
  ]);
  await validate('/root/a/file', [
    [str('DRIVE_MY_DRIVE_LABEL'), 'filesystem:drive/root'],
    ['a', 'filesystem:drive/root/a'],
    ['file', 'filesystem:drive/root/a/file'],
  ]);
  // team_drives.
  await validate('/team_drives/S1/file', [
    [str('DRIVE_SHARED_DRIVES_LABEL'), 'filesystem:drive/team_drives'],
    ['S1', 'filesystem:drive/team_drives/S1'],
    ['file', 'filesystem:drive/team_drives/S1/file'],
  ]);
  await validate('/team_drives/S1/a/file', [
    [str('DRIVE_SHARED_DRIVES_LABEL'), 'filesystem:drive/team_drives'],
    ['S1', 'filesystem:drive/team_drives/S1'],
    ['a', 'filesystem:drive/team_drives/S1/a'],
    ['file', 'filesystem:drive/team_drives/S1/a/file'],
  ]);

  const downloadsVolumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  fs = /** @type {!MockFileSystem} */ (downloadsVolumeInfo.fileSystem);

  // Downloads.
  await validate('/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), 'filesystem:downloads/'],
    ['file', 'filesystem:downloads/file'],
  ]);
  await validate('/a/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), 'filesystem:downloads/'],
    ['a', 'filesystem:downloads/a'],
    ['file', 'filesystem:downloads/a/file'],
  ]);

  // Special labels for '/Downloads', '/PvmDefault', '/Camera'.
  await validate('/Downloads/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), 'filesystem:downloads/'],
    [str('DOWNLOADS_DIRECTORY_LABEL'), 'filesystem:downloads/Downloads'],
    ['file', 'filesystem:downloads/Downloads/file'],
  ]);
  await validate('/PvmDefault/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), 'filesystem:downloads/'],
    [str('PLUGIN_VM_DIRECTORY_LABEL'), 'filesystem:downloads/PvmDefault'],
    ['file', 'filesystem:downloads/PvmDefault/file'],
  ]);
  await validate('/Camera/file', [
    [str('DOWNLOADS_DIRECTORY_LABEL'), 'filesystem:downloads/'],
    [str('CAMERA_DIRECTORY_LABEL'), 'filesystem:downloads/Camera'],
    ['file', 'filesystem:downloads/Camera/file'],
  ]);
}
