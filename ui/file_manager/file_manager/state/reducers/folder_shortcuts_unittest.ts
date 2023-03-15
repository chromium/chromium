// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chai_assert.js';

import {MockFileSystem} from '../../common/js/mock_entry.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {EntryType} from '../../externs/ts/state.js';
import {addFolderShortcut as addFolderShortcutAction, refreshFolderShortcut as refreshFolderShortcutAction, removeFolderShortcut as removeFolderShortcutAction} from '../actions/folder_shortcuts.js';
import {createFakeFileData} from '../for_tests.js';
import {getEmptyState} from '../store.js';

import {addFolderShortcut, refreshFolderShortcut, removeFolderShortcut} from './folder_shortcuts.js';

/** Generate a fake file system with fake file entries. */
function setupFileSystem(): MockFileSystem {
  const fileSystem = new MockFileSystem('fake-fs');
  fileSystem.populate([
    '/shortcut-1/',
    '/shortcut-2/',
    '/shortcut-3/',
    '/shortcut-4/',
  ]);
  return fileSystem;
}


/** Tests folder shortcuts can be refreshed correctly. */
export function testRefreshFolderShortcuts() {
  const currentState = getEmptyState();
  // Add shortcut-1 to the store.
  const fileSystem = setupFileSystem();
  const shortcutEntry1: DirectoryEntry = fileSystem.entries['/shortcut-1'];
  const shortcutEntry2: DirectoryEntry = fileSystem.entries['/shortcut-2'];
  const shortcutEntry3: DirectoryEntry = fileSystem.entries['/shortcut-3'];
  currentState.allEntries[shortcutEntry1.toURL()] = createFakeFileData({
    entry: shortcutEntry1,
    volumeType: VolumeManagerCommon.VolumeType.DRIVE,
    label: 'shortcut 1',
    type: EntryType.FS_API,
  });
  currentState.folderShortcuts.push(shortcutEntry1.toURL());

  const newState = refreshFolderShortcut(
      currentState,
      refreshFolderShortcutAction({entries: [shortcutEntry2, shortcutEntry3]}));
  assertEquals(2, newState.folderShortcuts.length);
  assertEquals(shortcutEntry2.toURL(), newState.folderShortcuts[0]);
  assertEquals(shortcutEntry3.toURL(), newState.folderShortcuts[1]);
}

/** Tests folder shortcut can be added correctly. */
export function testAddFolderShortcut() {
  const currentState = getEmptyState();
  // Add shortcut-1 and shortcut-3 to the store.
  const fileSystem = setupFileSystem();
  const shortcutEntry1: DirectoryEntry = fileSystem.entries['/shortcut-1'];
  const shortcutEntry2: DirectoryEntry = fileSystem.entries['/shortcut-2'];
  const shortcutEntry3: DirectoryEntry = fileSystem.entries['/shortcut-3'];
  const shortcutEntry4: DirectoryEntry = fileSystem.entries['/shortcut-4'];
  currentState.allEntries[shortcutEntry1.toURL()] = createFakeFileData({
    entry: shortcutEntry1,
    volumeType: VolumeManagerCommon.VolumeType.DRIVE,
    label: 'shortcut 1',
    type: EntryType.FS_API,
  });
  currentState.allEntries[shortcutEntry3.toURL()] = createFakeFileData({
    entry: shortcutEntry3,
    volumeType: VolumeManagerCommon.VolumeType.DRIVE,
    label: 'shortcut 3',
    type: EntryType.FS_API,
  });
  currentState.folderShortcuts.push(shortcutEntry1.toURL());
  currentState.folderShortcuts.push(shortcutEntry3.toURL());

  // Add a new shortcut.
  const newState1 = addFolderShortcut(
      currentState, addFolderShortcutAction({entry: shortcutEntry2}));
  assertEquals(3, newState1.folderShortcuts.length);
  assertEquals(shortcutEntry2.toURL(), newState1.folderShortcuts[1]);
  // Add an already existed shortcut.
  const newState2 = addFolderShortcut(
      currentState, addFolderShortcutAction({entry: shortcutEntry1}));
  assertEquals(newState2.folderShortcuts, currentState.folderShortcuts);
  // Add another entry to check sorting.
  const newState3 = addFolderShortcut(
      currentState, addFolderShortcutAction({entry: shortcutEntry4}));
  assertEquals(3, newState3.folderShortcuts.length);
  assertEquals(shortcutEntry4.toURL(), newState3.folderShortcuts[2]);
}

/** Tests folder shortcut can be removed correctly. */
export function testRemoveFolderShortcut() {
  const currentState = getEmptyState();
  // Add shortcut-1 to the store.
  const fileSystem = setupFileSystem();
  const shortcutEntry1: DirectoryEntry = fileSystem.entries['/shortcut-1'];
  const shortcutEntry2: DirectoryEntry = fileSystem.entries['/shortcut-2'];
  currentState.allEntries[shortcutEntry1.toURL()] = createFakeFileData({
    entry: shortcutEntry1,
    volumeType: VolumeManagerCommon.VolumeType.DRIVE,
    label: 'shortcut 1',
    type: EntryType.FS_API,
  });
  currentState.folderShortcuts.push(shortcutEntry1.toURL());

  // Remove a shortcut.
  const newState1 = removeFolderShortcut(
      currentState, removeFolderShortcutAction({key: shortcutEntry1.toURL()}));
  assertEquals(0, newState1.folderShortcuts.length);
  // Remove a non-exist shortcut.
  const newState2 = removeFolderShortcut(
      currentState, removeFolderShortcutAction({key: shortcutEntry2.toURL()}));
  assertEquals(newState2.folderShortcuts, currentState.folderShortcuts);
}
