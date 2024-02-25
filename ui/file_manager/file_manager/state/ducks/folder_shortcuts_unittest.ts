// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {MockFileSystem} from '../../common/js/mock_entry.js';
import type {State} from '../../state/state.js';
import {convertEntryToFileData} from '../ducks/all_entries.js';
import {setUpFileManagerOnWindow, setupStore, waitDeepEquals} from '../for_tests.js';
import {getEmptyState} from '../store.js';

import {addFolderShortcut, refreshFolderShortcut, removeFolderShortcut} from './folder_shortcuts.js';

export function setUp() {
  setUpFileManagerOnWindow();
}

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
export async function testRefreshFolderShortcuts(done: () => void) {
  const initialState = getEmptyState();
  // Add shortcut-1 to the store.
  const fileSystem = setupFileSystem();
  const shortcutEntry1 = fileSystem.entries['/shortcut-1'] as DirectoryEntry;
  initialState.allEntries[shortcutEntry1.toURL()] =
      convertEntryToFileData(shortcutEntry1);
  initialState.folderShortcuts.push(shortcutEntry1.toURL());

  const store = setupStore(initialState);

  // Dispatch a refresh action with shortcut 2 and 3.
  const shortcutEntry2 = fileSystem.entries['/shortcut-2'] as DirectoryEntry;
  const shortcutEntry3 = fileSystem.entries['/shortcut-3'] as DirectoryEntry;
  store.dispatch(
      refreshFolderShortcut({entries: [shortcutEntry2, shortcutEntry3]}));

  // Expect all shortcut entries are in allEntries, and only shortcut 2 and 3
  // are in the folderShortcuts.
  const want: Partial<State> = {
    allEntries: {
      [shortcutEntry2.toURL()]: convertEntryToFileData(shortcutEntry2),
      [shortcutEntry3.toURL()]: convertEntryToFileData(shortcutEntry3),
    },
    folderShortcuts: [shortcutEntry2.toURL(), shortcutEntry3.toURL()],
  };
  await waitDeepEquals(store, want, (state) => ({
                                      allEntries: state.allEntries,
                                      folderShortcuts: state.folderShortcuts,
                                    }));

  done();
}

/** Tests folder shortcut can be added correctly. */
export async function testAddFolderShortcut(done: () => void) {
  const initialState = getEmptyState();
  // Add shortcut-1 and shortcut-3 to the store.
  const fileSystem = setupFileSystem();
  const shortcutEntry1 = fileSystem.entries['/shortcut-1'] as DirectoryEntry;
  const shortcutEntry3 = fileSystem.entries['/shortcut-3'] as DirectoryEntry;
  initialState.allEntries[shortcutEntry1.toURL()] =
      convertEntryToFileData(shortcutEntry1);
  initialState.allEntries[shortcutEntry3.toURL()] =
      convertEntryToFileData(shortcutEntry3);
  initialState.folderShortcuts.push(shortcutEntry1.toURL());
  initialState.folderShortcuts.push(shortcutEntry3.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to add shortcut 4.
  const shortcutEntry4 = fileSystem.entries['/shortcut-4'] as DirectoryEntry;
  store.dispatch(addFolderShortcut({entry: shortcutEntry4}));

  // Expect the newly added shortcut 4 is in the store.
  const want1: Partial<State> = {
    allEntries: {
      [shortcutEntry1.toURL()]: convertEntryToFileData(shortcutEntry1),
      [shortcutEntry3.toURL()]: convertEntryToFileData(shortcutEntry3),
      [shortcutEntry4.toURL()]: convertEntryToFileData(shortcutEntry4),
    },
    folderShortcuts: [
      shortcutEntry1.toURL(),
      shortcutEntry3.toURL(),
      shortcutEntry4.toURL(),
    ],
  };
  await waitDeepEquals(store, want1, (state) => ({
                                       allEntries: state.allEntries,
                                       folderShortcuts: state.folderShortcuts,
                                     }));

  // Dispatch another action to add already existed shortcut 1.
  store.dispatch(addFolderShortcut({entry: shortcutEntry1}));

  // Expect no changes in the store.
  await waitDeepEquals(store, want1, (state) => ({
                                       allEntries: state.allEntries,
                                       folderShortcuts: state.folderShortcuts,
                                     }));

  // Dispatch another action to add shortcut 2 to check sorting.
  const shortcutEntry2 = fileSystem.entries['/shortcut-2'] as DirectoryEntry;
  store.dispatch(addFolderShortcut({entry: shortcutEntry2}));

  // Expect shortcut 2 will be inserted in the middle.
  const want2: Partial<State> = {
    allEntries: {
      ...want1.allEntries,
      [shortcutEntry2.toURL()]: convertEntryToFileData(shortcutEntry2),
    },
    folderShortcuts: [
      shortcutEntry1.toURL(),
      shortcutEntry2.toURL(),
      shortcutEntry3.toURL(),
      shortcutEntry4.toURL(),
    ],
  };
  await waitDeepEquals(store, want2, (state) => ({
                                       allEntries: state.allEntries,
                                       folderShortcuts: state.folderShortcuts,
                                     }));

  done();
}

/** Tests folder shortcut can be removed correctly. */
export async function testRemoveFolderShortcut(done: () => void) {
  const initialState = getEmptyState();
  // Add shortcut-1 to the store.
  const fileSystem = setupFileSystem();
  const shortcutEntry1 = fileSystem.entries['/shortcut-1'] as DirectoryEntry;
  initialState.allEntries[shortcutEntry1.toURL()] =
      convertEntryToFileData(shortcutEntry1);
  initialState.folderShortcuts.push(shortcutEntry1.toURL());

  const store = setupStore(initialState);

  // Dispatch an action to remove shortcut 1.
  store.dispatch(removeFolderShortcut({key: shortcutEntry1.toURL()}));

  // Expect shortcut 1 is removed from the store.
  await waitDeepEquals(store, [], (state) => state.folderShortcuts);

  // Dispatch another action to remove non-existed shortcut 2.
  const shortcutEntry2 = fileSystem.entries['/shortcut-2'] as DirectoryEntry;
  store.dispatch(removeFolderShortcut({key: shortcutEntry2.toURL()}));

  // Expect no changes in the store.
  await waitDeepEquals(store, [], (state) => state.folderShortcuts);

  done();
}
