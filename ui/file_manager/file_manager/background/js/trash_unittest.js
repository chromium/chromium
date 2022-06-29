// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {MockDirectoryEntry, MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {TrashDirs} from '../../common/js/trash.js';
import {util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

import {MockVolumeManager} from './mock_volume_manager.js';
import {Trash} from './trash.js';

/** @type {!MockVolumeManager} */
let volumeManager;

/**
 * Boolean indicating whether Trash is enabled.
 * @type {boolean}
 * */
let trashEnabled = true;

// Set up the test components.
export function setUp() {
  loadTimeData.getString = id => id;

  util.isTrashEnabled = () => trashEnabled;

  volumeManager = new MockVolumeManager();
}

/**
 * Call removeFileOrDirectory with the supplied settings and validate that
 * we correctly either permanently delete, or move to trash.
 *
 * @suppress {accessControls} Access private functions
 * permanentlyDeleteFileOrDirectory_() and trashFileOrDirectory_().
 */
function checkRemoveFileOrDirectory(
    filesTrashEnabled, rootType, path, deletePermanently,
    expectPermanentlyDelete) {
  trashEnabled = filesTrashEnabled;
  const volumeInfo =
      volumeManager.createVolumeInfo(rootType, 'volumeId', 'label');
  const f = MockFileEntry.create(volumeInfo.fileSystem, path);

  const trash = new Trash();
  // Detect whether permanentlyDelete..., or trash... is called.
  let permanentlyDeleteCalled = false;
  let trashCalled = false;
  trash.permanentlyDeleteFileOrDirectory_ = () => {
    permanentlyDeleteCalled = true;
    return Promise.resolve();
  };
  trash.trashFileOrDirectory_ = (volumeManager, entry) => {
    trashCalled = true;
    return Promise.resolve();
  };

  trash.removeFileOrDirectory(volumeManager, f, deletePermanently);
  assertEquals(expectPermanentlyDelete, permanentlyDeleteCalled);
  assertEquals(!expectPermanentlyDelete, trashCalled);
}

/**
 * Test that removeFileOrDirectory() correctly moves to trash, or permanently
 * deletes.
 */
export function testRemoveFileOrDirectory() {
  // Only use trash if flag is enabled, entry is in 'downloads' volume, but not
  // in /.Trash.

  // enabled, rootType, path, deletePermanently, expectPermanentlyDelete.
  checkRemoveFileOrDirectory(false, 'removable', '/f', false, true);
  checkRemoveFileOrDirectory(false, 'removable', '/f', true, true);
  checkRemoveFileOrDirectory(false, 'downloads', '/f', false, true);
  checkRemoveFileOrDirectory(false, 'downloads', '/f', true, true);
  checkRemoveFileOrDirectory(true, 'removable', '/f', false, true);
  checkRemoveFileOrDirectory(true, 'removable', '/f', true, true);
  checkRemoveFileOrDirectory(true, 'downloads', '/f', false, false);
  checkRemoveFileOrDirectory(true, 'downloads', '/.Trash/f', false, true);
  checkRemoveFileOrDirectory(true, 'downloads', '/f', true, true);
}

/**
 * Test permanentlyDeleteFileOrDirectory_().
 *
 * @suppress {accessControls} Access permanentlyDeleteFileOrDirectory_().
 */
export async function testPermanentlyDeleteFileOrDirectory(done) {
  const trash = new Trash();
  const fs = new MockFileSystem('volumeId');
  const dir = MockDirectoryEntry.create(fs, '/dir');
  const file1 = MockFileEntry.create(fs, '/dir/file1');
  MockFileEntry.create(fs, '/dir/file2');
  MockFileEntry.create(fs, '/dir/file3');

  // Deleted file should be removed and no new files in FileSystem.
  assertEquals(5, Object.keys(fs.entries).length);
  assertTrue(!!fs.entries['/dir/file1']);
  await trash.permanentlyDeleteFileOrDirectory_(file1);
  assertFalse(!!fs.entries['/dir/file1']);
  assertEquals(4, Object.keys(fs.entries).length);

  // Deleted dir should also delete all children.
  assertTrue(!!fs.entries['/dir']);
  await trash.permanentlyDeleteFileOrDirectory_(dir);
  assertFalse(!!fs.entries['/dir']);
  assertFalse(!!fs.entries['/dir/file2']);
  assertFalse(!!fs.entries['/dir/file3']);
  assertEquals(1, Object.keys(fs.entries).length);

  done();
}

/**
 * Test trash in MyFiles.
 */
export async function testMyFilesTrash(done) {
  const trash = new Trash();
  const deletePermanently = false;
  const downloads = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  const fs = downloads.fileSystem;

  const dir = MockDirectoryEntry.create(fs, '/dir');
  const file1 = MockFileEntry.create(fs, '/dir/file1', null, new Blob(['f1']));
  const file2 = MockFileEntry.create(fs, '/dir/file2', null, new Blob(['f2']));
  const file3 = MockFileEntry.create(fs, '/dir/file3', null, new Blob(['f3']));

  // Trashed file should be moved to /.Trash/files and new file added in
  // /.Trash/info.
  assertEquals(5, Object.keys(fs.entries).length);
  assertTrue(!!fs.entries['/dir/file1']);
  await trash.removeFileOrDirectory(volumeManager, file1, deletePermanently);
  assertFalse(!!fs.entries['/dir/file1']);
  assertTrue(fs.entries['/.Trash/files'].isDirectory);
  assertTrue(fs.entries['/.Trash/info'].isDirectory);
  assertTrue(fs.entries['/.Trash/files/file1'].isFile);
  assertTrue(fs.entries['/.Trash/info/file1.trashinfo'].isFile);
  let text = await fs.entries['/.Trash/files/file1'].content.text();
  assertEquals('f1', text);
  text = await fs.entries['/.Trash/info/file1.trashinfo'].content.text();
  assertTrue(text.startsWith('[Trash Info]\nPath=/dir/file1\nDeletionDate='));
  assertEquals(9, Object.keys(fs.entries).length);

  // Trashed dir should also move children files into /.Trash/files.
  assertTrue(!!fs.entries['/dir']);
  await trash.removeFileOrDirectory(volumeManager, dir, deletePermanently);
  assertFalse(!!fs.entries['/dir']);
  assertFalse(!!fs.entries['/dir/file2']);
  assertFalse(!!fs.entries['/dir/file3']);
  assertTrue(fs.entries['/.Trash/files'].isDirectory);
  assertTrue(fs.entries['/.Trash/info'].isDirectory);
  assertTrue(fs.entries['/.Trash/files/dir'].isDirectory);
  assertTrue(fs.entries['/.Trash/files/dir/file2'].isFile);
  assertTrue(fs.entries['/.Trash/files/dir/file3'].isFile);
  text = await fs.entries['/.Trash/files/dir/file2'].content.text();
  assertEquals('f2', text);
  text = await fs.entries['/.Trash/files/dir/file3'].content.text();
  assertEquals('f3', text);
  assertTrue(fs.entries['/.Trash/info/dir.trashinfo'].isFile);
  text = await fs.entries['/.Trash/info/dir.trashinfo'].content.text();
  assertTrue(text.startsWith('[Trash Info]\nPath=/dir\nDeletionDate='));
  assertEquals(10, Object.keys(fs.entries).length);

  done();
}

/**
 * Test that Downloads has its own /Downloads/.Trash since it is a separate
 * mount on a device and we don't want move to trash to be a copy operation.
 */
export async function testDownloadsHasOwnTrash(done) {
  const trash = new Trash();
  const deletePermanently = false;
  const downloads = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  const fs = downloads.fileSystem;
  const file1 = MockFileEntry.create(fs, '/file1', null, new Blob(['f1']));
  const dir2 = MockDirectoryEntry.create(fs, '/Downloads');
  const file2 =
      MockFileEntry.create(fs, '/Downloads/file2', null, new Blob(['f2']));
  const file3 =
      MockFileEntry.create(fs, '/Downloads/file3', null, new Blob(['f3']));
  assertEquals(5, Object.keys(fs.entries).length);

  // Move /file1 to trash.
  await trash.removeFileOrDirectory(volumeManager, file1, deletePermanently);
  assertTrue(fs.entries['/.Trash'].isDirectory);
  assertTrue(fs.entries['/.Trash/files'].isDirectory);
  assertTrue(fs.entries['/.Trash/info'].isDirectory);
  assertTrue(fs.entries['/.Trash/files/file1'].isFile);
  assertTrue(fs.entries['/.Trash/info/file1.trashinfo'].isFile);
  assertEquals(9, Object.keys(fs.entries).length);

  // Move /Downloads/file2 to trash.
  await trash.removeFileOrDirectory(volumeManager, file2, deletePermanently);
  assertTrue(fs.entries['/Downloads/.Trash'].isDirectory);
  assertTrue(fs.entries['/Downloads/.Trash/files'].isDirectory);
  assertTrue(fs.entries['/Downloads/.Trash/info'].isDirectory);
  assertTrue(fs.entries['/Downloads/.Trash/files/file2'].isFile);
  assertTrue(fs.entries['/Downloads/.Trash/info/file2.trashinfo'].isFile);
  assertEquals(13, Object.keys(fs.entries).length);

  // Delete /Downloads/.Trash/files/file2.
  const file2Trashed = fs.entries['/Downloads/.Trash/files/file2'];
  assertFalse(!!trash.shouldMoveToTrash(volumeManager, file2Trashed));
  await trash.removeFileOrDirectory(
      volumeManager, file2Trashed, deletePermanently);
  assertEquals(12, Object.keys(fs.entries).length);

  // Delete /Downloads/.Trash.
  const downloadsTrash = fs.entries['/Downloads/.Trash'];
  assertFalse(!!trash.shouldMoveToTrash(volumeManager, downloadsTrash));
  await trash.removeFileOrDirectory(
      volumeManager, downloadsTrash, deletePermanently);
  assertFalse(!!fs.entries['/Downloads/.Trash']);
  assertEquals(8, Object.keys(fs.entries).length);

  // Move /Downloads/file3 to trash, should recreate /Downloads/.Trash.
  await trash.removeFileOrDirectory(volumeManager, file3, deletePermanently);
  assertTrue(fs.entries['/Downloads/.Trash'].isDirectory);
  assertTrue(fs.entries['/Downloads/.Trash/files'].isDirectory);
  assertTrue(fs.entries['/Downloads/.Trash/info'].isDirectory);
  assertTrue(fs.entries['/Downloads/.Trash/files/file3'].isFile);
  assertTrue(fs.entries['/Downloads/.Trash/info/file3.trashinfo'].isFile);
  assertEquals(12, Object.keys(fs.entries).length);
  done();
}

/**
 * Test crostini trash in .local/share/Trash.
 */
export async function testCrostiniTrash(done) {
  const trash = new Trash();
  const deletePermanently = false;
  const crostini = volumeManager.createVolumeInfo(
      VolumeManagerCommon.VolumeType.CROSTINI, 'crostini', 'Linux files', '',
      '/home/testuser');
  const fs = crostini.fileSystem;
  const file1 = MockFileEntry.create(fs, '/file1', null, new Blob(['f1']));
  const file2 = MockFileEntry.create(fs, '/file2', null, new Blob(['f1']));
  assertEquals(3, Object.keys(fs.entries).length);

  // Move /file1 to trash.
  const file1TrashEntry = await trash.removeFileOrDirectory(
      volumeManager, file1, deletePermanently);
  assertFalse(!!fs.entries['/file1']);
  assertTrue(fs.entries['/.local/share/Trash'].isDirectory);
  assertTrue(fs.entries['/.local/share/Trash/files'].isDirectory);
  assertTrue(fs.entries['/.local/share/Trash/info'].isDirectory);
  assertTrue(fs.entries['/.local/share/Trash/files/file1'].isFile);
  assertTrue(fs.entries['/.local/share/Trash/info/file1.trashinfo'].isFile);
  const text = await fs.entries['/.local/share/Trash/info/file1.trashinfo']
                   .content.text();
  assertTrue(
      text.startsWith('[Trash Info]\nPath=/home/testuser/file1\nDeletionDate='),
      `${text} must have Path=/home/test/user/file1`);
  assertEquals(9, Object.keys(fs.entries).length);

  // Restore /file1
  await trash.restore(volumeManager, assert(file1TrashEntry));
  assertEquals(8, Object.keys(fs.entries).length);
  assertTrue(!!fs.entries['/file1']);

  // Move /file2 to trash, then delete /.local/share/Trash/files/file2.
  await trash.removeFileOrDirectory(volumeManager, file2, deletePermanently);
  const file2Trashed = fs.entries['/.local/share/Trash/files/file2'];
  assertFalse(!!trash.shouldMoveToTrash(volumeManager, file2Trashed));
  await trash.removeFileOrDirectory(
      volumeManager, file2Trashed, deletePermanently);
  assertEquals(8, Object.keys(fs.entries).length);

  // Delete /.local/share/Trash.
  const crostiniTrash = fs.entries['/.local/share/Trash'];
  assertFalse(!!trash.shouldMoveToTrash(volumeManager, crostiniTrash));
  await trash.removeFileOrDirectory(
      volumeManager, crostiniTrash, deletePermanently);
  assertEquals(4, Object.keys(fs.entries).length);

  done();
}

/**
 * Test restore().
 */
export async function testRestore(done) {
  const trash = new Trash();
  const deletePermanently = false;
  const downloads = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  const fs = downloads.fileSystem;

  const dir = MockDirectoryEntry.create(fs, '/dir');
  const file1 = MockFileEntry.create(fs, '/dir/file1', null, new Blob(['f1']));
  const file2 = MockFileEntry.create(fs, '/dir/file2', null, new Blob(['f2']));
  const file3 = MockFileEntry.create(fs, '/dir/file3', null, new Blob(['f3']));

  // Move /dir/file1 to trash.
  const file1TrashEntry = await trash.removeFileOrDirectory(
      volumeManager, file1, deletePermanently);
  assertEquals(9, Object.keys(fs.entries).length);
  assertFalse(!!fs.entries['/dir/file1']);
  assertEquals('file1', file1TrashEntry.name);
  assertEquals(fs.entries['/.Trash/files/file1'], file1TrashEntry.filesEntry);
  assertEquals(
      fs.entries['/.Trash/info/file1.trashinfo'], file1TrashEntry.infoEntry);

  // Restore it.
  await trash.restore(volumeManager, assert(file1TrashEntry));
  assertEquals(8, Object.keys(fs.entries).length);
  assertTrue(!!fs.entries['/dir/file1']);

  // Move /dir/file2 to trash, recreate a new /dir/file2,
  // original should restore to '/dir/file2 (1)'.
  const file2TrashEntry = await trash.removeFileOrDirectory(
      volumeManager, file2, deletePermanently);
  assertFalse(!!fs.entries['/dir/file2']);
  assertEquals(9, Object.keys(fs.entries).length);
  MockFileEntry.create(fs, '/dir/file2', null, new Blob(['f2v2']));
  assertEquals(10, Object.keys(fs.entries).length);
  await trash.restore(volumeManager, assert(file2TrashEntry));
  assertEquals(9, Object.keys(fs.entries).length);
  assertTrue(!!fs.entries['/dir/file2 (1)']);
  let text = await fs.entries['/dir/file2'].content.text();
  assertEquals('f2v2', text);
  text = await fs.entries['/dir/file2 (1)'].content.text();
  assertEquals('f2', text);

  done();
}

/**
 * Test removeOldEntries_().
 *
 * @suppress {accessControls} Access removeOldItems_() and inProgress_.
 */
export async function testRemoveOldItems_(done) {
  const trash = new Trash();
  const deletePermanently = false;
  const downloads = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DOWNLOADS);
  const fs = downloads.fileSystem;

  const dir = MockDirectoryEntry.create(fs, '/dir');
  const file1 = MockFileEntry.create(fs, '/dir/file1', null, new Blob(['f1']));
  const file2 = MockFileEntry.create(fs, '/dir/file2', null, new Blob(['f2']));
  const file3 = MockFileEntry.create(fs, '/dir/file3', null, new Blob(['f3']));
  const file4 = MockFileEntry.create(fs, '/dir/file4', null, new Blob(['f4']));
  const file5 = MockFileEntry.create(fs, '/dir/file5', null, new Blob(['f5']));
  const file6 = MockFileEntry.create(fs, '/dir/file6', null, new Blob(['f6']));

  // Get TrashConfig.
  const config = trash.shouldMoveToTrash(volumeManager, fs.root);
  assert(config);

  // Move files to trash.
  for (const f of [file1, file2, file3, file4, file5, file6]) {
    await trash.removeFileOrDirectory(volumeManager, f, deletePermanently);
  }
  assertEquals(17, Object.keys(fs.entries).length);
  const now = Date.now();

  // Directories inside info should be deleted.
  MockDirectoryEntry.create(fs, '/.Trash/info/baddir.trashinfo');
  // Files that do not end with .trashinfo should be deleted.
  MockFileEntry.create(fs, '/.Trash/info/f', null, new Blob(['f']));
  // Files that are write-in-progress with no DeletionDate should be ignored.
  fs.entries['/.Trash/info/file1.trashinfo'].content =
      new Blob(['no-deletion-date']);
  trash.inProgress_.set('downloads-/', new Set(['file1.trashinfo']));
  delete fs.entries['/.Trash/files/file1'];
  // Files without a matching file in .Trash/files should be deleted.
  delete fs.entries['/.Trash/files/file2'];
  // Files with no DeletionDate should be deleted.
  fs.entries['/.Trash/info/file3.trashinfo'].content =
      new Blob(['no-deletion-date']);
  // Files with DeletionDate which cannot be parsed should be deleted.
  fs.entries['/.Trash/info/file4.trashinfo'].content =
      new Blob(['DeletionDate=abc']);
  // Files with no matching trashinfo should be deleted.
  delete fs.entries['/.Trash/info/file5.trashinfo'];

  const trashDirs =
      new TrashDirs(fs.entries['/.Trash/files'], fs.entries['/.Trash/info']);
  await trash.removeOldItems_(trashDirs, config, now);
  assertTrue(!!fs.entries['/']);
  assertTrue(!!fs.entries['/.Trash']);
  assertTrue(!!fs.entries['/.Trash/files']);
  assertTrue(!!fs.entries['/.Trash/files/file6']);
  assertTrue(!!fs.entries['/.Trash/info']);
  assertTrue(!!fs.entries['/.Trash/info/file1.trashinfo']);
  assertTrue(!!fs.entries['/.Trash/info/file6.trashinfo']);
  assertTrue(!!fs.entries['/dir']);
  assertEquals(8, Object.keys(fs.entries).length);

  // Items older than 30d should be deleted.
  const daysAgo29 = now + (29 * 24 * 60 * 60 * 1000);
  const daysAgo31 = now + (31 * 24 * 60 * 60 * 1000);
  await trash.removeOldItems_(trashDirs, config, daysAgo29);
  assertEquals(8, Object.keys(fs.entries).length);

  await trash.removeOldItems_(trashDirs, config, daysAgo31);
  assertTrue(!!fs.entries['/.Trash/info/file1.trashinfo']);
  assertFalse(!!fs.entries['/.Trash/info/file5.trashinfo']);
  assertFalse(!!fs.entries['/.Trash/files/file5']);
  assertEquals(6, Object.keys(fs.entries).length);

  // trashinfo with no matching file, and not in-progress should be deleted.
  trash.inProgress_.get('downloads-/').delete('file1.trashinfo');
  await trash.removeOldItems_(trashDirs, config, daysAgo31);
  assertFalse(!!fs.entries['/.Trash/info/file1.trashinfo']);
  assertEquals(5, Object.keys(fs.entries).length);

  done();
}
