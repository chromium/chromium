// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {createChild} from '../../common/js/dom_utils.js';
import {FakeEntryImpl} from '../../common/js/files_app_entry_types.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {waitUntil} from '../../common/js/test_error_reporting.js';
import {str} from '../../common/js/translations.js';
import {FileErrorToDomError} from '../../common/js/util.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {PropStatus} from '../../externs/ts/state.js';
import {constants} from '../../foreground/js/constants.js';
import {clearSearch, updateSearch} from '../../state/ducks/search.js';
import {convertVolumeInfoAndMetadataToVolume} from '../../state/ducks/volumes.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore} from '../../state/for_tests.js';
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
  // @ts-ignore: error TS2345: Argument of type 'MockMetadataModel' is not
  // assignable to parameter of type 'MetadataModel'.
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

  // Mock fileManagerPrivate.getCustomActions for |testShownForODFS|.
  const mockChrome = {
    fileManagerPrivate: {
      // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
      // type.
      getCustomActions: function(entries, callback) {
        // This is called for the test when Reauthentication Required state is
        // true.
        const actions = [{
          id: constants.FSP_ACTION_HIDDEN_ONEDRIVE_REAUTHENTICATION_REQUIRED,
          title: 'true',
        }];
        callback(actions);
      },
    },
  };
  installMockChrome(mockChrome);
}

/**
 * Tests that no files message will be rendered for each filter type.
 * @suppress {accessControls} access private method/property in test.
 */
export function testNoFilesMessage() {
  // Mock current directory to Recent.
  directoryModel.getCurrentRootType = () => VolumeManagerCommon.RootType.RECENT;

  // For all filter.
  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      // @ts-ignore: error TS2341: Property 'label_' is private and only
      // accessible within class 'EmptyFolderController'.
      str('RECENT_EMPTY_FOLDER'), emptyFolderController.label_.innerText);
  // For audio filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.AUDIO;
  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      // @ts-ignore: error TS2341: Property 'label_' is private and only
      // accessible within class 'EmptyFolderController'.
      str('RECENT_EMPTY_AUDIO_FOLDER'), emptyFolderController.label_.innerText);
  // For document filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.DOCUMENT;
  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_DOCUMENTS_FOLDER'),
      // @ts-ignore: error TS2341: Property 'label_' is private and only
      // accessible within class 'EmptyFolderController'.
      emptyFolderController.label_.innerText);
  // For image filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.IMAGE;
  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_IMAGES_FOLDER'),
      // @ts-ignore: error TS2341: Property 'label_' is private and only
      // accessible within class 'EmptyFolderController'.
      emptyFolderController.label_.innerText);
  // For video filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.VIDEO;
  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_VIDEOS_FOLDER'),
      // @ts-ignore: error TS2341: Property 'label_' is private and only
      // accessible within class 'EmptyFolderController'.
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

  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertTrue(element.hidden);
  // @ts-ignore: error TS2341: Property 'label_' is private and only accessible
  // within class 'EmptyFolderController'.
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
  // @ts-ignore: error TS2341: Property 'isScanning_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.isScanning_ = true;

  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertTrue(element.hidden);
  // @ts-ignore: error TS2341: Property 'label_' is private and only accessible
  // within class 'EmptyFolderController'.
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

  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertTrue(element.hidden);
  // @ts-ignore: error TS2341: Property 'label_' is private and only accessible
  // within class 'EmptyFolderController'.
  assertEquals('', emptyFolderController.label_.innerText);
}

/**
 * Tests that the empty folder element is hidden if the volume is ODFS
 * and the scan finished with no error. Add ODFS to the store so that the
 * |isInteractive| state of the volume can be read.
 * @suppress {accessControls} access private method in test.
 */
export function testHiddenForODFS() {
  // Add ODFS volume to the store.
  setUpFileManagerOnWindow();
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  const odfsVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'odfs', 'odfs', 'odfs',
      constants.ODFS_EXTENSION_ID);
  volumeManager.volumeInfoList.add(odfsVolumeInfo);
  const volume = convertVolumeInfoAndMetadataToVolume(
      odfsVolumeInfo, createFakeVolumeMetadata(odfsVolumeInfo));
  initialState.volumes[volume.volumeId] = volume;

  setupStore(initialState);

  // Set ODFS as the volume.
  directoryModel.getCurrentVolumeInfo = function() {
    return /** @type {!import("../../externs/volume_info.js").VolumeInfo} */ (
        odfsVolumeInfo);
  };

  // Complete the scan with no error.
  // @ts-ignore: error TS2341: Property 'onScanFinished_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.onScanFinished_();

  // Expect that the empty-folder element is hidden.
  assertTrue(element.hidden);
  // @ts-ignore: error TS2341: Property 'label_' is private and only accessible
  // within class 'EmptyFolderController'.
  assertEquals('', emptyFolderController.label_.innerText);
}

/**
 * Tests that the empty state image shows up when root type is Trash.
 * @suppress {accessControls} access private method in test.
 */
export function testShownForTrash() {
  directoryModel.getCurrentRootType = () => VolumeManagerCommon.RootType.TRASH;
  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  // @ts-ignore: error TS2341: Property 'label_' is private and only accessible
  // within class 'EmptyFolderController'.
  const text = emptyFolderController.label_.innerText;
  assertTrue(text.includes(str('EMPTY_TRASH_FOLDER_TITLE')));
}

/**
 * Tests that the reauthentication required image shows up when the volume is
 * ODFS and the scan failed from a NO_MODIFICATION_ALLOWED_ERR (access denied).
 * Add ODFS to the store so that the |isInteractive| state of the volume can be
 * set and read.
 * @suppress {accessControls} access private method in test.
 * @param {()=>void} done
 */
export async function testShownForODFS(done) {
  // Add ODFS volume to the store.
  setUpFileManagerOnWindow();
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  const odfsVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'odfs', 'odfs', 'odfs',
      constants.ODFS_EXTENSION_ID);
  volumeManager.volumeInfoList.add(odfsVolumeInfo);
  const volume = convertVolumeInfoAndMetadataToVolume(
      odfsVolumeInfo, createFakeVolumeMetadata(odfsVolumeInfo));
  initialState.volumes[volume.volumeId] = volume;

  setupStore(initialState);

  // Set ODFS as the volume.
  directoryModel.getCurrentVolumeInfo = function() {
    return /** @type {!import("../../externs/volume_info.js").VolumeInfo} */ (
        odfsVolumeInfo);
  };

  // Pass a NO_MODIFICATION_ALLOWED_ERR error (triggers a call to
  // getCustomActions to which is stubbed out to return reauthentication
  // required as true).
  const event = new Event('scan-failed');
  // @ts-ignore: error TS2339: Property 'error' does not exist on type 'Event'.
  event.error = {name: FileErrorToDomError.NO_MODIFICATION_ALLOWED_ERR};
  // @ts-ignore: error TS2341: Property 'onScanFailed_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.onScanFailed_(event);

  // Expect that the empty-folder element is shown and the sign in link is
  // present. Need to wait for |updateUI_| to be called as the check for
  // reauthentication is required is asynchronous.
  await waitUntil(() => !element.hidden);
  // @ts-ignore: error TS2341: Property 'label_' is private and only accessible
  // within class 'EmptyFolderController'.
  await waitUntil(() => emptyFolderController.label_.querySelector('.sign-in'));
  done();
}

/**
 * Tests that the empty state image shows up when search is active.
 * @suppress {accessControls} access private method in test.
 */
export function testShowNoSearchResult() {
  const store = getStore();
  store.init(getEmptyState());
  // Test 1: Store indicates we are not searching. No matter if the directory is
  // empty or not, we must not show "No matching search results" panel.
  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertTrue(element.hidden);

  // Test 2: Dispatch search update so that the store indicates we are
  // searchhing. Expect "No matching search results" panel.
  store.dispatch(updateSearch({
    query: 'any-string-will-do',
    status: PropStatus.STARTED,
    options: undefined,
  }));

  // @ts-ignore: error TS2341: Property 'updateUI_' is private and only
  // accessible within class 'EmptyFolderController'.
  emptyFolderController.updateUI_();
  assertFalse(element.hidden);
  // @ts-ignore: error TS2341: Property 'label_' is private and only accessible
  // within class 'EmptyFolderController'.
  const text = emptyFolderController.label_.innerText;
  assertTrue(text.includes(str('SEARCH_NO_MATCHING_RESULTS_TITLE')));

  // Clean up the store.
  store.dispatch(clearSearch());
}
