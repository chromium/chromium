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
import {RootType, VolumeType} from '../../common/js/volume_manager_types.js';
import {FakeEntry} from '../../externs/files_app_entry_interfaces.js';
import {PropStatus} from '../../externs/ts/state.js';
import {constants} from '../../foreground/js/constants.js';
import {clearSearch, updateSearch} from '../../state/ducks/search.js';
import {convertVolumeInfoAndMetadataToVolume} from '../../state/ducks/volumes.js';
import {createFakeVolumeMetadata, setUpFileManagerOnWindow, setupStore} from '../../state/for_tests.js';
import {getEmptyState, getStore} from '../../state/store.js';

import {DirectoryModel} from './directory_model.js';
import {EmptyFolderController, ScanFailedEvent} from './empty_folder_controller.js';
import {FileListModel} from './file_list_model.js';
import {MetadataModel} from './metadata/metadata_model.js';
import {MockMetadataModel} from './metadata/mock_metadata.js';
import {createFakeDirectoryModel} from './mock_directory_model.js';
import {ProvidersModel} from './providers_model.js';

// Test class to enable accessing protected methods on the
// `EmptyFolderController`.
class TestEmptyFolderController extends EmptyFolderController {
  get isScanning() {
    return this.isScanning_;
  }

  set isScanning(isScanning: boolean) {
    this.isScanning_ = isScanning;
  }

  get label() {
    return this.label_;
  }

  onScanFailed(event: ScanFailedEvent) {
    return this.onScanFailed_(event);
  }

  updateUi() {
    this.updateUi_();
  }
}

let element: HTMLElement;
let directoryModel: DirectoryModel;
let providersModel: ProvidersModel;
let fileListModel: FileListModel;
let recentEntry: FakeEntry;
let emptyFolderController: TestEmptyFolderController;

export function setUp() {
  // Create EmptyFolderController instance with dependencies.
  element = document.createElement('div');
  createChild(element, 'label', 'span');

  // Setup the image, nested svg and nested use elements.
  const image = createChild(element, 'image');
  const svg = createChild(image, undefined, 'svg');
  createChild(svg, undefined, 'use');

  directoryModel = createFakeDirectoryModel();
  fileListModel =
      new FileListModel(new MockMetadataModel({}) as unknown as MetadataModel);
  directoryModel.getFileList = () => fileListModel;
  directoryModel.isSearching = () => false;
  providersModel = new ProvidersModel(new MockVolumeManager());
  recentEntry = new FakeEntryImpl(
      'Recent', RootType.RECENT,
      chrome.fileManagerPrivate.SourceRestriction.ANY_SOURCE,
      chrome.fileManagerPrivate.FileCategory.ALL);
  emptyFolderController = new TestEmptyFolderController(
      element, directoryModel, providersModel, recentEntry);

  // Mock fileManagerPrivate.getCustomActions for |testShownForODFS|.
  const mockChrome = {
    fileManagerPrivate: {
      getCustomActions: function(
          _: Entry[],
          callback: (customActions: chrome.fileManagerPrivate
                         .FileSystemProviderAction[]) => void) {
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
 */
export function testNoFilesMessage() {
  // Mock current directory to Recent.
  directoryModel.getCurrentRootType = () => RootType.RECENT;

  // For all filter.
  emptyFolderController.updateUi();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_FOLDER'), emptyFolderController.label.innerText);
  // For audio filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.AUDIO;
  emptyFolderController.updateUi();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_AUDIO_FOLDER'), emptyFolderController.label.innerText);
  // For document filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.DOCUMENT;
  emptyFolderController.updateUi();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_DOCUMENTS_FOLDER'),
      emptyFolderController.label.innerText);
  // For image filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.IMAGE;
  emptyFolderController.updateUi();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_IMAGES_FOLDER'), emptyFolderController.label.innerText);
  // For video filter.
  recentEntry.fileCategory = chrome.fileManagerPrivate.FileCategory.VIDEO;
  emptyFolderController.updateUi();
  assertFalse(element.hidden);
  assertEquals(
      str('RECENT_EMPTY_VIDEOS_FOLDER'), emptyFolderController.label.innerText);
}

/**
 * Tests that no files message will be hidden for non-Recent entries.
 */
export function testHiddenForNonRecent() {
  // Mock current directory to Downloads.
  directoryModel.getCurrentRootType = () => RootType.DOWNLOADS;

  emptyFolderController.updateUi();
  assertTrue(element.hidden);
  assertEquals('', emptyFolderController.label.innerText);
}

/**
 * Tests that no files message will be hidden if scanning is in progress.
 */
export function testHiddenForScanning() {
  // Mock current directory to Recent.
  directoryModel.getCurrentRootType = () => RootType.RECENT;
  // Mock scanning.
  emptyFolderController.isScanning = true;

  emptyFolderController.updateUi();
  assertTrue(element.hidden);
  assertEquals('', emptyFolderController.label.innerText);
}

/**
 * Tests that no files message will be hidden if there are files in the list.
 */
export function testHiddenForFiles() {
  // Mock current directory to Recent.
  directoryModel.getCurrentRootType = () => RootType.RECENT;
  // Current file list has 1 item.
  fileListModel.push({name: 'a.txt', isDirectory: false, toURL: () => 'a.txt'});

  emptyFolderController.updateUi();
  assertTrue(element.hidden);
  assertEquals('', emptyFolderController.label.innerText);
}

/**
 * Tests that the empty folder element is hidden if the volume is ODFS
 * and the scan finished with no error. Add ODFS to the store so that the
 * |isInteractive| state of the volume can be read.
 */
export function testHiddenForODFS() {
  // Add ODFS volume to the store.
  setUpFileManagerOnWindow();
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  const odfsVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.PROVIDED, 'odfs', 'odfs', 'odfs', constants.ODFS_EXTENSION_ID);
  volumeManager.volumeInfoList.add(odfsVolumeInfo);
  const volume = convertVolumeInfoAndMetadataToVolume(
      odfsVolumeInfo, createFakeVolumeMetadata(odfsVolumeInfo));
  initialState.volumes[volume.volumeId] = volume;

  setupStore(initialState);

  // Set ODFS as the volume.
  directoryModel.getCurrentVolumeInfo = function() {
    return odfsVolumeInfo;
  };

  // Complete the scan with no error.
  emptyFolderController.onScanFinished();

  // Expect that the empty-folder element is hidden.
  assertTrue(element.hidden);
  assertEquals('', emptyFolderController.label.innerText);
}

/**
 * Tests that the empty state image shows up when root type is Trash.
 */
export function testShownForTrash() {
  directoryModel.getCurrentRootType = () => RootType.TRASH;
  emptyFolderController.updateUi();
  assertFalse(element.hidden);
  const text = emptyFolderController.label.innerText;
  assertTrue(text.includes(str('EMPTY_TRASH_FOLDER_TITLE')));
}

/**
 * Tests that the reauthentication required image shows up when the volume is
 * ODFS and the scan failed from a NO_MODIFICATION_ALLOWED_ERR (access denied).
 * Add ODFS to the store so that the |isInteractive| state of the volume can be
 * set and read.
 */
export async function testShownForODFS(done: VoidCallback) {
  // Add ODFS volume to the store.
  setUpFileManagerOnWindow();
  const initialState = getEmptyState();
  const {volumeManager} = window.fileManager;
  const odfsVolumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeType.PROVIDED, 'odfs', 'odfs', 'odfs', constants.ODFS_EXTENSION_ID);
  volumeManager.volumeInfoList.add(odfsVolumeInfo);
  const volume = convertVolumeInfoAndMetadataToVolume(
      odfsVolumeInfo, createFakeVolumeMetadata(odfsVolumeInfo));
  initialState.volumes[volume.volumeId] = volume;

  setupStore(initialState);

  // Set ODFS as the volume.
  directoryModel.getCurrentVolumeInfo = function() {
    return odfsVolumeInfo;
  };

  // Pass a NO_MODIFICATION_ALLOWED_ERR error (triggers a call to
  // getCustomActions to which is stubbed out to return reauthentication
  // required as true).
  const event = new CustomEvent('scan-failed', {
    detail: {
      error: {
        name: FileErrorToDomError.NO_MODIFICATION_ALLOWED_ERR,
        message: '',
      },
    },
  });
  emptyFolderController.onScanFailed(event);

  // Expect that the empty-folder element is shown and the sign in link is
  // present. Need to wait for |updateUi| to be called as the check for
  // reauthentication is required is asynchronous.
  await waitUntil(() => !element.hidden);
  await waitUntil(
      () => emptyFolderController.label.querySelector('.sign-in') !== null);
  done();
}

/**
 * Tests that the empty state image shows up when search is active.
 */
export function testShowNoSearchResult() {
  const store = getStore();
  store.init(getEmptyState());
  // Test 1: Store indicates we are not searching. No matter if the directory is
  // empty or not, we must not show "No matching search results" panel.
  emptyFolderController.updateUi();
  assertTrue(element.hidden);

  // Test 2: Dispatch search update so that the store indicates we are
  // searchhing. Expect "No matching search results" panel.
  store.dispatch(updateSearch({
    query: 'any-string-will-do',
    status: PropStatus.STARTED,
    options: undefined,
  }));

  emptyFolderController.updateUi();
  assertFalse(element.hidden);
  const text = emptyFolderController.label.innerText;
  assertTrue(text.includes(str('SEARCH_NO_MATCHING_RESULTS_TITLE')));

  // Clean up the store.
  store.dispatch(clearSearch());
}
