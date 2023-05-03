// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {loadTimeData} from 'chrome://resources/ash/common/load_time_data.m.js';
import {assertDeepEquals, assertTrue} from 'chrome://webui-test/chai_assert.js';

import {installMockChrome} from '../../common/js/mock_chrome.js';
import {FileOperationManager} from '../../externs/background/file_operation_manager.js';
import {State} from '../../externs/ts/state.js';
import {VolumeManager} from '../../externs/volume_manager.js';
import {setUpFileManagerOnWindow, waitDeepEquals} from '../../state/for_tests.js';
import {getEmptyState, getStore} from '../../state/store.js';

import {DirectoryModel} from './directory_model.js';
import {GuestOsController} from './guest_os_controller.js';
import {MockNavigationListModel} from './mock_navigation_list_model.js';
import {NavigationListModel} from './navigation_list_model.js';
import {DirectoryTree} from './ui/directory_tree.js';

/** Mock chrome APIs.  */
let mockChrome: any;
let directoryModel: DirectoryModel;
let directoryTree: DirectoryTree;
let volumeManager: VolumeManager;

let onMountableGuestsChangedCallback:
    (data: chrome.fileManagerPrivate.MountableGuest[]) => void;

export function setUp() {
  loadTimeData.overrideValues({
    GUEST_OS: true,
  });
  document.body.innerHTML = [
    '<style>',
    '  .hide {',
    '    display: none;',
    '  }',
    '</style>',
    '<div tabindex="0" id="directory-tree">',
    '</div>',
  ].join('');

  mockChrome = {
    fileManagerPrivate: {
      onMountableGuestsChanged: {
        addListener(
            callback: (_: chrome.fileManagerPrivate.MountableGuest[]) => void) {
          onMountableGuestsChangedCallback = callback;
        },
      },
    },
  };
  installMockChrome(mockChrome);

  setUpFileManagerOnWindow();
  directoryModel = window.fileManager.directoryModel;
  volumeManager = window.fileManager.volumeManager;
  directoryTree =
      document.querySelector('#directory-tree')! as unknown as DirectoryTree;
  DirectoryTree.decorate(
      document.querySelector('#directory-tree')!,
      directoryModel,
      volumeManager,
      window.fileManager.metadataModel,
      {
        addEVentListener(_name: string, _callback: () => void) {},
      } as unknown as FileOperationManager,
      /*fakeEntriesVisible=*/ true,

  );
  directoryTree.dataModel = new MockNavigationListModel(volumeManager) as
      unknown as NavigationListModel;


  const store = getStore();
  store.init(getEmptyState());
}

export async function testDuplicatedPlayFiles(done: () => void) {
  const store = getStore();
  assertDeepEquals([], store.getState().uiEntries);

  const controller =
      new GuestOsController(directoryModel, directoryTree, volumeManager);
  assertTrue(!!controller);

  // Emulate the private API event sending 2 PlayFiles instances.
  onMountableGuestsChangedCallback([
    {
      id: 0,
      displayName: 'PlayFiles',
      vmType: chrome.fileManagerPrivate.VmType.ARCVM,
    },
    {
      id: 1,
      displayName: 'PlayFiles',
      vmType: chrome.fileManagerPrivate.VmType.ARCVM,
    },
  ]);

  // Expect only one instance in the store.
  const want: State['uiEntries'] = [
    'entry-list://my_files',
    'fake-entry://guest-os/1',
  ];
  await waitDeepEquals(store, want, (state) => state.uiEntries);

  done();
}
