// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {createChild} from '../../common/js/dom_utils.js';
import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {str} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';

import {DirectoryModel} from './directory_model.js';
import {EmptyFolderController} from './empty_folder_controller.js';
import {FileListModel} from './file_list_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';

/**
 * @type {!HTMLElement}
 */
let element;

/**
 * @type {!DirectoryModel}
 */
let directoryModel;

/**
 * @type {!FileListModel}
 */
let fileListModel;

/**
 * @type {!FakeEntry}
 */
let recentEntry;

/**
 * @type {!EmptyFolderController}
 */
let emptyFolderController;

export function setUp() {
  // Create EmptyFolderController instance with dependencies.
  element = /** @type {!HTMLElement} */ (document.createElement('div'));
  createChild(element, 'label', 'span');

  // Setup the image, nested svg and nested use elements.
  const image = createChild(element, 'image');
  const svg = createChild(image, undefined, 'svg');
  createChild(svg, undefined, 'use');

  directoryModel = createFakeDirectoryModel();
  fileListModel = new FileListModel(new MockMetadataModel({}));
  directoryModel.getFileList = () => fileListModel;
  recentEntry = new FakeEntryImpl(
      'Recent', VolumeManagerCommon.RootType.RECENT,
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE,
      chrome.fileManagerPrivate.RecentFileType.ALL);
  emptyFolderController =
      new EmptyFolderController(element, directoryModel, recentEntry);
}

/**
 * Tests that no files message will be rendered for each filter type.
 * @suppress {accessControls} access private method/property in test.
 */
export function testNoFilesMessage() {
  // Mock current directory to Recent.
  directoryModel.getCurrentRootType = () => VolumeManagerCommon.RootType.RECENT;

  // For all filter.
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_FOLDER'), emptyFolderController.label_.innerText);
  // For audio filter.
  recentEntry.recentFileType = chrome.fileManagerPrivate.RecentFileType.AUDIO;
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_AUDIO_FOLDER'), emptyFolderController.label_.innerText);
  // For document filter.
  recentEntry.recentFileType =
      chrome.fileManagerPrivate.RecentFileType.DOCUMENT;
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_DOCUMENTS_FOLDER'),
      emptyFolderController.label_.innerText);
  // For image filter.
  recentEntry.recentFileType = chrome.fileManagerPrivate.RecentFileType.IMAGE;
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_IMAGES_FOLDER'),
      emptyFolderController.label_.innerText);
  // For video filter.
  recentEntry.recentFileType = chrome.fileManagerPrivate.RecentFileType.VIDEO;
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_VIDEOS_FOLDER'),
      emptyFolderController.label_.innerText);
}

/**
 * Tests that no files message will be hidden for non-Recent entries.
 * @suppress {accessControls} access private method in test.
 */
export function testHiddenForNonRecent() {
  // Mock current directory to Downloads.
  directoryModel.getCurrentRootType = () =>
      VolumeManagerCommon.RootType.DOWNLOADS;

  emptyFolderController.updateUI_();
  assertTrue(element.hidden);
  assertEquals('', emptyFolderController.label_.innerText);
}

/**
 * Tests that no files message will be hidden if scanning is in progress.
 * @suppress {accessControls} access private method in test.
 */
export function testHiddenForScanning() {
  // Mock current directory to Recent.
  directoryModel.getCurrentRootType = () => VolumeManagerCommon.RootType.RECENT;
  // Mock scanning.
  emptyFolderController.isScanning_ = true;

  emptyFolderController.updateUI_();
  assertTrue(element.hidden);
  assertEquals('', emptyFolderController.label_.innerText);
}

/**
 * Tests that no files message will be hidden if there are files in the list.
 * @suppress {accessControls} access private method in test.
 */
export function testHiddenForFiles() {
  // Mock current directory to Recent.
  directoryModel.getCurrentRootType = () => VolumeManagerCommon.RootType.RECENT;
  // Current file list has 1 item.
  fileListModel.push({name: 'a.txt', isDirectory: false, toURL: () => 'a.txt'});

  emptyFolderController.updateUI_();
  assertTrue(element.hidden);
  assertEquals('', emptyFolderController.label_.innerText);
}

/**
 * Tests that the empty state image shows up when root type is Trash.
 * @suppress {accessControls} access private method in test.
 */
export function testShownForTrash() {
  directoryModel.getCurrentRootType = () => VolumeManagerCommon.RootType.TRASH;
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  const text = emptyFolderController.label_.innerText;
  assertTrue(text.includes(str('EMPTY_TRASH_FOLDER_TITLE')));
}
