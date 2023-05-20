// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertNotEquals, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {createChild} from '../../common/js/dom_utils.js';
import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {str, util} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {PropStatus} from '../../externs/ts/state.js';
import {VolumeInfo} from '../../externs/volume_info.js';
import {constants} from '../../foreground/js/constants.js';
import {clearSearch, updateSearch} from '../../state/actions/search.js';
import {getEmptyState, getStore} from '../../state/store.js';

import {DirectoryModel} from './directory_model.js';
import {EmptyFolderController} from './empty_folder_controller.js';
import {FileListModel} from './file_list_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';
import {ProvidersModel} from './providers_model.js';

/**
 * @type {!HTMLElement}
 */
let element;

/**
 * @type {!DirectoryModel}
 */
let directoryModel;

/**
 * @type {!ProvidersModel}
 */
let providersModel;

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
  directoryModel.isSearching = () => false;
  providersModel = new ProvidersModel(new MockVolumeManager());
  recentEntry = new FakeEntryImpl(
      'Recent', VolumeManagerCommon.RootType.RECENT,
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE,
      chrome.fileManagerPrivate.FileCategory.ALL);
  emptyFolderController = new EmptyFolderController(
      element, directoryModel, providersModel, recentEntry);
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
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.AUDIO;
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_AUDIO_FOLDER'), emptyFolderController.label_.innerText);
  // For document filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.DOCUMENT;
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_DOCUMENTS_FOLDER'),
      emptyFolderController.label_.innerText);
  // For image filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.IMAGE;
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_IMAGES_FOLDER'),
      emptyFolderController.label_.innerText);
  // For video filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.VIDEO;
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
 * Tests that no files message will be hidden if the volume is ODFS
 * but the scan finished with no error.
 * @suppress {accessControls} access private method in test.
 */
export function testHiddenForODFS() {
  // Set ODFS as the volume.
  directoryModel.getCurrentVolumeInfo = function() {
    return /** @type {!VolumeInfo} */ ({
      providerId: constants.ODFS_EXTENSION_ID,
    });
  };

  emptyFolderController.onScanFinished_();
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

/**
 * Tests that the reauthentication required image shows up when the volume is
 * ODFS and the scan failed from a NO_MODIFICATION_ALLOWED_ERR.
 * @suppress {accessControls} access private method in test.
 */
export function testShownForODFS() {
  // Set ODFS as the volume.
  directoryModel.getCurrentVolumeInfo = function() {
    return /** @type {!VolumeInfo} */ ({
      providerId: constants.ODFS_EXTENSION_ID,
    });
  };

  // Pass a NO_MODIFICATION_ALLOWED_ERR error (implies reauthentication
  // required).
  const event = new Event('scan-failed');
  event.error = {name: util.FileError.NO_MODIFICATION_ALLOWED_ERR};
  emptyFolderController.onScanFailed_(event);

  assertFalse(element.hidden);
  const signInLink = emptyFolderController.label_.querySelector('.sign-in');
  assertNotEquals(signInLink, null);
}

/**
 * Tests that the empty state image shows up when search is active.
 * @suppress {accessControls} access private method in test.
 */
export function testShowNoSearchResult() {
  util.isSearchV2Enabled = () => true;
  const store = getStore();
  store.init(getEmptyState());
  // Test 1: Store indicates we are not searching. No matter if the directory is
  // empty or not, we must not show "No matching search results" panel.
  emptyFolderController.updateUI_();
  assertTrue(element.hidden);

  // Test 2: Dispatch search update so that the store indicates we are
  // searchhing. Expect "No matching search results" panel.
  store.dispatch(updateSearch({
    query: 'any-string-will-do',
    status: PropStatus.STARTED,
    options: undefined,
  }));

  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  const text = emptyFolderController.label_.innerText;
  assertTrue(text.includes(str('SEARCH_NO_MATCHING_RESULTS_TITLE')));

  // Clean up the store.
  store.dispatch(clearSearch());
}
