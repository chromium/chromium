// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {installMockChrome, MockCommandLinePrivate} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {assertRejected, reportPromise, waitUntil} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';

import {VolumeInfoImpl} from './volume_info_impl.js';
import {volumeManagerFactory} from './volume_manager_factory.js';
import {VolumeManagerImpl} from './volume_manager_impl.js';
import {volumeManagerUtil} from './volume_manager_util.js';

// @ts-ignore: error TS7034: Variable 'mockChrome' implicitly has type 'any' in
// some locations where its type cannot be determined.
let mockChrome;
// @ts-ignore: error TS7034: Variable 'mockData' implicitly has type 'any' in
// some locations where its type cannot be determined.
let mockData;
// @ts-ignore: error TS7034: Variable 'createVolumeInfoOriginal' implicitly has
// type 'any' in some locations where its type cannot be determined.
let createVolumeInfoOriginal;
// @ts-ignore: error TS7034: Variable 'webkitResolveLocalFileSystemURLOriginal'
// implicitly has type 'any' in some locations where its type cannot be
// determined.
let webkitResolveLocalFileSystemURLOriginal;

export function setUp() {
  mockData = {
    mountSourcePath_: null,
    onMountCompletedListeners_: [],
    onDriveConnectionStatusChangedListeners_: [],
    driveConnectionState_: 'ONLINE',
    volumeMetadataList_: [],
    password: undefined,
    // @ts-ignore: error TS7006: Parameter 'state' implicitly has an 'any' type.
    setDriveConnectionState(state) {
      // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
      // type.
      mockData.driveConnectionState_ = state;
      /** @type {!EventTarget} */ (
          chrome.fileManagerPrivate.onDriveConnectionStatusChanged)
          .dispatchEvent(new Event('anything'));
    },
  };

  // Set up mock of chrome.fileManagerPrivate APIs.
  mockChrome = {
    fileManagerPrivate: {
      // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
      // type.
      addMount: function(fileUrl, password, callback) {
        // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
        // type.
        mockData.password = password;
        // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
        // type.
        callback(mockData.mountSourcePath_);
      },
      // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
      // type.
      getVolumeRoot: function(options, callback) {
        // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
        // type.
        if (!(options.volumeId in mockData.fileSystemMap_)) {
          // @ts-ignore: error TS2339: Property 'runtime' does not exist on type
          // 'typeof chrome'.
          chrome.runtime.lastError = {message: 'Not found.'};
        }
        // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
        // type.
        callback(mockData.fileSystemMap_[options.volumeId].root);
      },
      // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
      // type.
      removeMount: function(volumeId, callback) {
        const event = {
          eventType: 'unmount',
          status: 'success',
          volumeMetadata: {volumeId: volumeId},
        };
        // @ts-ignore: error TS7005: Variable 'mockChrome' implicitly has an
        // 'any' type.
        mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent(event);
        callback();
      },
      onDriveConnectionStatusChanged: {
        // @ts-ignore: error TS7006: Parameter 'listener' implicitly has an
        // 'any' type.
        addListener: function(listener) {
          // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an
          // 'any' type.
          mockData.onDriveConnectionStatusChangedListeners_.push(listener);
        },
        // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any'
        // type.
        dispatchEvent: function(event) {
          // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an
          // 'any' type.
          mockData.onDriveConnectionStatusChangedListeners_.forEach(
              // @ts-ignore: error TS7006: Parameter 'listener' implicitly has
              // an 'any' type.
              listener => {
                listener(event);
              });
        },
      },
      onMountCompleted: {
        // @ts-ignore: error TS7006: Parameter 'listener' implicitly has an
        // 'any' type.
        addListener: function(listener) {
          // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an
          // 'any' type.
          mockData.onMountCompletedListeners_.push(listener);
        },
        // @ts-ignore: error TS7006: Parameter 'event' implicitly has an 'any'
        // type.
        dispatchEvent: function(event) {
          // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an
          // 'any' type.
          mockData.onMountCompletedListeners_.forEach(
              // @ts-ignore: error TS7006: Parameter 'listener' implicitly has
              // an 'any' type.
              listener => listener(event));
        },
      },
      // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
      // type.
      getDriveConnectionState: function(callback) {
        // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
        // type.
        callback(mockData.driveConnectionState_);
      },
      // @ts-ignore: error TS7006: Parameter 'callback' implicitly has an 'any'
      // type.
      getVolumeMetadataList: function(callback) {
        // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
        // type.
        callback(mockData.volumeMetadataList_);
      },
    },
  };

  installMockChrome(mockChrome);
  new MockCommandLinePrivate();
  mockData.volumeMetadataList_ = [
    // @ts-ignore: error TS2322: Type '{ volumeId: string; volumeLabel: string;
    // volumeType: string; isReadOnly: false; profile: { displayName: string;
    // isCurrentProfile: boolean; profileId: string; }; configurable: false;
    // watchable: true; source: string; }' is not assignable to type 'never'.
    {
      volumeId: 'download:Downloads',
      volumeLabel: '',
      volumeType: VolumeManagerCommon.VolumeType.DOWNLOADS,
      isReadOnly: false,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: VolumeManagerCommon.Source.SYSTEM,
    },
    // @ts-ignore: error TS2322: Type '{ volumeId: string; volumeLabel: string;
    // volumeType: string; isReadOnly: false; profile: { displayName: string;
    // isCurrentProfile: boolean; profileId: string; }; configurable: false;
    // watchable: true; source: string; }' is not assignable to type 'never'.
    {
      volumeId: 'drive:drive-foobar%40chromium.org-hash',
      volumeLabel: '',
      volumeType: VolumeManagerCommon.VolumeType.DRIVE,
      isReadOnly: false,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: VolumeManagerCommon.Source.NETWORK,
    },
    // @ts-ignore: error TS2322: Type '{ volumeId: string; volumeLabel: string;
    // volumeType: string; isReadOnly: false; profile: { displayName: string;
    // isCurrentProfile: boolean; profileId: string; }; configurable: false;
    // watchable: true; source: string; }' is not assignable to type 'never'.
    {
      volumeId: 'android_files:0',
      volumeLabel: '',
      volumeType: VolumeManagerCommon.VolumeType.ANDROID_FILES,
      isReadOnly: false,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: VolumeManagerCommon.Source.SYSTEM,
    },
  ];
  // @ts-ignore: error TS2339: Property 'fileSystemMap_' does not exist on type
  // '{ mountSourcePath_: null; onMountCompletedListeners_: never[];
  // onDriveConnectionStatusChangedListeners_: never[]; driveConnectionState_:
  // string; volumeMetadataList_: never[]; password: undefined;
  // setDriveConnectionState(state: any): void; }'.
  mockData.fileSystemMap_ = {
    'download:Downloads': new MockFileSystem('download:Downloads'),
    'drive:drive-foobar%40chromium.org-hash':
        new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
    'android_files:0': new MockFileSystem('android_files:0'),
  };

  const driveFs =
      // @ts-ignore: error TS2339: Property 'fileSystemMap_' does not exist on
      // type '{ mountSourcePath_: null; onMountCompletedListeners_: never[];
      // onDriveConnectionStatusChangedListeners_: never[];
      // driveConnectionState_: string; volumeMetadataList_: never[]; password:
      // undefined; setDriveConnectionState(state: any): void; }'.
      mockData.fileSystemMap_['drive:drive-foobar%40chromium.org-hash'];
  driveFs.populate(['/root/', '/team_drives/', '/Computers/']);

  // Mock window.webkitResolveLocalFileSystemURL to return entries for DriveFS.
  webkitResolveLocalFileSystemURLOriginal =
      window.webkitResolveLocalFileSystemURL;
  window.webkitResolveLocalFileSystemURL = (url, success) => {
    const match = url.match(/^filesystem:drive:.*(\/.*)/);
    if (match) {
      const path = match[1];
      const entry = driveFs.entries[path];
      if (entry) {
        return setTimeout(success, 0, entry);
      }
    }
    throw new DOMException('Unknown drive url: ' + url, 'NotFoundError');
  };

  createVolumeInfoOriginal = volumeManagerUtil.createVolumeInfo;
}

export function tearDown() {
  volumeManagerFactory.revokeInstanceForTesting();
  // To avoid a closure warning assigning to |chrome|, tearDown() does not
  // balance the call to installMockChrome() here.

  // Restore the createVolumeInfo() function.
  // @ts-ignore: error TS7005: Variable 'createVolumeInfoOriginal' implicitly
  // has an 'any' type.
  volumeManagerUtil.createVolumeInfo = createVolumeInfoOriginal;

  // Restore window.webkitResolveLocalFileSystemURL.
  window.webkitResolveLocalFileSystemURL =
      // @ts-ignore: error TS7005: Variable
      // 'webkitResolveLocalFileSystemURLOriginal' implicitly has an 'any' type.
      webkitResolveLocalFileSystemURLOriginal;
}

// @ts-ignore: error TS7006: Parameter 'volumeManager' implicitly has an 'any'
// type.
async function waitAllVolumes(volumeManager) {
  // Drive + Downloads + Android:
  await waitUntil(() => volumeManager.volumeInfoList.length === 3);
}

/**
 * Returns a mock profile.
 *
 * @return {{displayName:string, isCurrentProfile:boolean, profileId:string}}
 *     Mock profile
 */
function getMockProfile() {
  return {
    displayName: 'foobar@chromium.org',
    isCurrentProfile: true,
    profileId: '',
  };
}

/** @param {()=>void} done */
export async function testGetVolumeInfo(done) {
  const volumeManager = await volumeManagerFactory.getInstance();

  const entry = MockFileEntry.create(
      new MockFileSystem('download:Downloads'), '/foo/bar/bla.zip');

  await waitAllVolumes(volumeManager);

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  // @ts-ignore: error TS18047: 'volumeInfo' is possibly 'null'.
  assertEquals('download:Downloads', volumeInfo.volumeId);
  // @ts-ignore: error TS18047: 'volumeInfo' is possibly 'null'.
  assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS, volumeInfo.volumeType);

  done();
}

/**
 * Tests that an unresponsive volume doesn't lock the whole Volume Manager
 * initialization.
 * @param {()=>void} done
 */
export async function testUnresponsiveVolumeStartUp(done) {
  let unblock;
  const fileManagerPrivate = chrome.fileManagerPrivate;

  // Replace chrome.fileManagerPrivate.getVolumeRoot() to emulate 1
  // volume not resolving.
  const origGetVolumeRoot = fileManagerPrivate.getVolumeRoot;

  fileManagerPrivate.getVolumeRoot = (options, callback) => {
    if (options.volumeId === 'download:Downloads') {
      console.log(`blocking the resolve for ${options.volumeId}`);
      unblock = () => origGetVolumeRoot(options, callback);
      return;
    }
    return origGetVolumeRoot(options, callback);
  };

  // getInstance() calls and waits for initialize(), which shouldn't get stuck
  // waiting for the all volumes to resolve.
  const volumeManager = await volumeManagerFactory.getInstance();

  // Wait 2 out 3 volumes to be ready.
  await waitUntil(() => volumeManager.volumeInfoList.length === 2);

  // Unblock the unresponsive volume and check if it gets available:
  // @ts-ignore: error TS2722: Cannot invoke an object which is possibly
  // 'undefined'.
  unblock();
  await waitUntil(() => volumeManager.volumeInfoList.length === 3);

  done();
}

/** @param {()=>void} callback */
export function testGetDriveConnectionState(callback) {
  reportPromise(
      volumeManagerFactory.getInstance().then(volumeManager => {
        // Default connection state is online
        assertEquals(
            chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
            volumeManager.getDriveConnectionState());

        // Sets it to offline.
        // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
        // type.
        mockData.setDriveConnectionState(
            chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE);
        assertEquals(
            chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
            volumeManager.getDriveConnectionState());

        // Sets it back to online
        // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
        // type.
        mockData.setDriveConnectionState(
            chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE);
        assertEquals(
            chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
            volumeManager.getDriveConnectionState());
      }),
      callback);
}

/** @param {()=>void} callback */
export function testMountArchiveAndUnmount(callback) {
  const test = async () => {
    // Set states of mock fileManagerPrivate APIs.
    const mountSourcePath = '/usr/local/home/test/Downloads/foobar.zip';
    // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
    // type.
    mockData.mountSourcePath_ = mountSourcePath;
    // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
    // type.
    mockData.fileSystemMap_['archive:foobar.zip'] =
        new MockFileSystem('archive:foobar.zip');

    const volumeManager = await volumeManagerFactory.getInstance();

    // Drive + Downloads + Android.
    const numberOfVolumes = 3;
    await waitAllVolumes(volumeManager);

    // Mount an archive
    const password = 'My Password';
    const mounted = volumeManager.mountArchive(
        'filesystem:chrome-extension://extensionid/external/' +
            'Downloads-test/foobar.zip',
        password);

    // @ts-ignore: error TS7005: Variable 'mockChrome' implicitly has an 'any'
    // type.
    mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
      eventType: 'mount',
      status: 'success',
      volumeMetadata: {
        volumeId: 'archive:foobar.zip',
        volumeLabel: 'foobar.zip',
        volumeType: VolumeManagerCommon.VolumeType.ARCHIVE,
        isReadOnly: true,
        sourcePath: mountSourcePath,
        profile: getMockProfile(),
        configurable: false,
        watchable: true,
        source: VolumeManagerCommon.Source.FILE,
      },
    });

    await mounted;

    assertEquals(numberOfVolumes + 1, volumeManager.volumeInfoList.length);
    // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
    // type.
    assertEquals(password, mockData.password);

    // Unmount the mounted archive
    const entry = MockFileEntry.create(
        new MockFileSystem('archive:foobar.zip'), '/foo.txt');
    const volumeInfo = volumeManager.getVolumeInfo(entry);
    // @ts-ignore: error TS2345: Argument of type 'VolumeInfo | null' is not
    // assignable to parameter of type 'VolumeInfo'.
    await volumeManager.unmount(volumeInfo);

    await waitUntil(
        () => volumeManager.volumeInfoList.length === numberOfVolumes);
    assertEquals(numberOfVolumes, volumeManager.volumeInfoList.length);
  };

  reportPromise(test(), callback);
}

/** @param {()=>void} callback */
export function testCancelMountingArchive(callback) {
  const test = async () => {
    // Set states of mock fileManagerPrivate APIs.
    const mountSourcePath = '/usr/local/home/test/Downloads/foobar.zip';
    // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
    // type.
    mockData.mountSourcePath_ = mountSourcePath;
    // @ts-ignore: error TS7005: Variable 'mockData' implicitly has an 'any'
    // type.
    mockData.fileSystemMap_['archive:foobar.zip'] =
        new MockFileSystem('archive:foobar.zip');

    const volumeManager = await volumeManagerFactory.getInstance();

    // Drive + Downloads + Android.
    const numberOfVolumes = 3;
    await waitAllVolumes(volumeManager);

    setTimeout(
        // @ts-ignore: error TS7005: Variable 'mockChrome' implicitly has an
        // 'any' type.
        () => mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
          eventType: 'mount',
          status: VolumeManagerCommon.VolumeError.CANCELLED,
          volumeMetadata: {
            volumeId: null,
            volumeLabel: null,
            volumeType: null,
            isReadOnly: null,
            sourcePath: mountSourcePath,
            profile: null,
            configurable: null,
            watchable: null,
            source: null,
          },
        }));

    try {
      await volumeManager.mountArchive(
          'filesystem:chrome-extension://extensionid/external/' +
              'Downloads-test/foobar.zip',
          'My Password');
    } catch (error) {
      assertEquals(error, VolumeManagerCommon.VolumeError.CANCELLED);
    }

    assertEquals(numberOfVolumes, volumeManager.volumeInfoList.length);
  };

  reportPromise(test(), callback);
}

// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testGetCurrentProfileVolumeInfo(done) {
  const volumeManager = await volumeManagerFactory.getInstance();
  await waitAllVolumes(volumeManager);

  const volumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);

  // @ts-ignore: error TS18047: 'volumeInfo' is possibly 'null'.
  assertEquals('drive:drive-foobar%40chromium.org-hash', volumeInfo.volumeId);
  // @ts-ignore: error TS18047: 'volumeInfo' is possibly 'null'.
  assertEquals(VolumeManagerCommon.VolumeType.DRIVE, volumeInfo.volumeType);

  done();
}

// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testGetLocationInfo(done) {
  const volumeManager = await volumeManagerFactory.getInstance();
  await waitAllVolumes(volumeManager);

  const downloadEntry = MockFileEntry.create(
      new MockFileSystem('download:Downloads'), '/foo/bar/bla.zip');
  const downloadLocationInfo = volumeManager.getLocationInfo(downloadEntry);
  assertEquals(
      // @ts-ignore: error TS18047: 'downloadLocationInfo' is possibly 'null'.
      VolumeManagerCommon.RootType.DOWNLOADS, downloadLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'downloadLocationInfo' is possibly 'null'.
  assertFalse(downloadLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'downloadLocationInfo' is possibly 'null'.
  assertFalse(downloadLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'downloadLocationInfo' is possibly 'null'.
  assertFalse(downloadLocationInfo.isRootEntry);

  const driveEntry = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'), '/root');
  const driveLocationInfo = volumeManager.getLocationInfo(driveEntry);
  // @ts-ignore: error TS18047: 'driveLocationInfo' is possibly 'null'.
  assertEquals(VolumeManagerCommon.RootType.DRIVE, driveLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'driveLocationInfo' is possibly 'null'.
  assertTrue(driveLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'driveLocationInfo' is possibly 'null'.
  assertFalse(driveLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'driveLocationInfo' is possibly 'null'.
  assertTrue(driveLocationInfo.isRootEntry);

  const teamDrivesGrandRoot = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/team_drives');
  const teamDrivesGrandRootLocationInfo =
      volumeManager.getLocationInfo(teamDrivesGrandRoot);
  assertEquals(
      VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT,
      // @ts-ignore: error TS18047: 'teamDrivesGrandRootLocationInfo' is
      // possibly 'null'.
      teamDrivesGrandRootLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'teamDrivesGrandRootLocationInfo' is possibly
  // 'null'.
  assertTrue(teamDrivesGrandRootLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'teamDrivesGrandRootLocationInfo' is possibly
  // 'null'.
  assertTrue(teamDrivesGrandRootLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'teamDrivesGrandRootLocationInfo' is possibly
  // 'null'.
  assertTrue(teamDrivesGrandRootLocationInfo.isRootEntry);

  const teamDrive = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/team_drives/MyTeamDrive');
  const teamDriveLocationInfo = volumeManager.getLocationInfo(teamDrive);
  assertEquals(
      VolumeManagerCommon.RootType.SHARED_DRIVE,
      // @ts-ignore: error TS18047: 'teamDriveLocationInfo' is possibly 'null'.
      teamDriveLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'teamDriveLocationInfo' is possibly 'null'.
  assertFalse(teamDriveLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'teamDriveLocationInfo' is possibly 'null'.
  assertFalse(teamDriveLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'teamDriveLocationInfo' is possibly 'null'.
  assertTrue(teamDriveLocationInfo.isRootEntry);

  const driveFilesByIdDirectoryEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.files-by-id/123');
  const driveFilesByIdDirectoryLocationInfo =
      volumeManager.getLocationInfo(driveFilesByIdDirectoryEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      // @ts-ignore: error TS18047: 'driveFilesByIdDirectoryLocationInfo' is
      // possibly 'null'.
      driveFilesByIdDirectoryLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'driveFilesByIdDirectoryLocationInfo' is
  // possibly 'null'.
  assertFalse(driveFilesByIdDirectoryLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'driveFilesByIdDirectoryLocationInfo' is
  // possibly 'null'.
  assertTrue(driveFilesByIdDirectoryLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'driveFilesByIdDirectoryLocationInfo' is
  // possibly 'null'.
  assertFalse(driveFilesByIdDirectoryLocationInfo.isRootEntry);

  const driveFilesByIdEntry = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.files-by-id/123/foo.txt');
  const driveFilesByIdLocationInfo =
      volumeManager.getLocationInfo(driveFilesByIdEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      // @ts-ignore: error TS18047: 'driveFilesByIdLocationInfo' is possibly
      // 'null'.
      driveFilesByIdLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'driveFilesByIdLocationInfo' is possibly 'null'.
  assertFalse(driveFilesByIdLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'driveFilesByIdLocationInfo' is possibly 'null'.
  assertFalse(driveFilesByIdLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'driveFilesByIdLocationInfo' is possibly 'null'.
  assertFalse(driveFilesByIdLocationInfo.isRootEntry);

  const driveShortcutTargetsByIdDirectoryEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.shortcut-targets-by-id/abcdef');
  const driveShortcutTargetsByIdDirectoryLocationInfo =
      volumeManager.getLocationInfo(driveShortcutTargetsByIdDirectoryEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      // @ts-ignore: error TS18047:
      // 'driveShortcutTargetsByIdDirectoryLocationInfo' is possibly 'null'.
      driveShortcutTargetsByIdDirectoryLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'driveShortcutTargetsByIdDirectoryLocationInfo'
  // is possibly 'null'.
  assertFalse(driveShortcutTargetsByIdDirectoryLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'driveShortcutTargetsByIdDirectoryLocationInfo'
  // is possibly 'null'.
  assertTrue(driveShortcutTargetsByIdDirectoryLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'driveShortcutTargetsByIdDirectoryLocationInfo'
  // is possibly 'null'.
  assertFalse(driveShortcutTargetsByIdDirectoryLocationInfo.isRootEntry);

  const driveShortcutTargetsByIdEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.shortcut-targets-by-id/abcdef/foo');
  const driveShortcutTargetsByIdLocationInfo =
      volumeManager.getLocationInfo(driveShortcutTargetsByIdEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      // @ts-ignore: error TS18047: 'driveShortcutTargetsByIdLocationInfo' is
      // possibly 'null'.
      driveShortcutTargetsByIdLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'driveShortcutTargetsByIdLocationInfo' is
  // possibly 'null'.
  assertFalse(driveShortcutTargetsByIdLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'driveShortcutTargetsByIdLocationInfo' is
  // possibly 'null'.
  assertFalse(driveShortcutTargetsByIdLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'driveShortcutTargetsByIdLocationInfo' is
  // possibly 'null'.
  assertFalse(driveShortcutTargetsByIdLocationInfo.isRootEntry);

  const androidRoot =
      MockFileEntry.create(new MockFileSystem('android_files:0'), '/');
  const androidRootLocationInfo = volumeManager.getLocationInfo(androidRoot);
  // @ts-ignore: error TS18047: 'androidRootLocationInfo' is possibly 'null'.
  assertTrue(androidRootLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'androidRootLocationInfo' is possibly 'null'.
  assertTrue(androidRootLocationInfo.isRootEntry);

  const androidSubFolder =
      MockFileEntry.create(new MockFileSystem('android_files:0'), '/Pictures');
  const androidSubFolderLocationInfo =
      volumeManager.getLocationInfo(androidSubFolder);
  // @ts-ignore: error TS18047: 'androidSubFolderLocationInfo' is possibly
  // 'null'.
  assertFalse(androidSubFolderLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'androidSubFolderLocationInfo' is possibly
  // 'null'.
  assertFalse(androidSubFolderLocationInfo.isRootEntry);

  const computersGrandRoot = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/Computers');
  const computersGrandRootLocationInfo =
      volumeManager.getLocationInfo(computersGrandRoot);
  assertEquals(
      VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT,
      // @ts-ignore: error TS18047: 'computersGrandRootLocationInfo' is possibly
      // 'null'.
      computersGrandRootLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'computersGrandRootLocationInfo' is possibly
  // 'null'.
  assertTrue(computersGrandRootLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'computersGrandRootLocationInfo' is possibly
  // 'null'.
  assertTrue(computersGrandRootLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'computersGrandRootLocationInfo' is possibly
  // 'null'.
  assertTrue(computersGrandRootLocationInfo.isRootEntry);

  const computer = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/Computers/MyComputer');
  const computerLocationInfo = volumeManager.getLocationInfo(computer);
  assertEquals(
      // @ts-ignore: error TS18047: 'computerLocationInfo' is possibly 'null'.
      VolumeManagerCommon.RootType.COMPUTER, computerLocationInfo.rootType);
  // @ts-ignore: error TS18047: 'computerLocationInfo' is possibly 'null'.
  assertFalse(computerLocationInfo.hasFixedLabel);
  // @ts-ignore: error TS18047: 'computerLocationInfo' is possibly 'null'.
  assertTrue(computerLocationInfo.isReadOnly);
  // @ts-ignore: error TS18047: 'computerLocationInfo' is possibly 'null'.
  assertTrue(computerLocationInfo.isRootEntry);

  done();
}

/** @param {()=>void} callback */
export function testWhenReady(callback) {
  volumeManagerFactory.getInstance().then((volumeManager) => {
    const promiseBeforeAdd = volumeManager.whenVolumeInfoReady('volumeId');
    const volumeInfo = new VolumeInfoImpl(
        /* volumeType */ VolumeManagerCommon.VolumeType.MY_FILES,
        /* volumeId */ 'volumeId',
        // @ts-ignore: error TS2345: Argument of type 'null' is not assignable
        // to parameter of type 'FileSystem'.
        /* fileSystem */ null,
        /* error */ undefined,
        /* deviceType */ undefined,
        /* devicePath */ undefined,
        /* isReadOnly */ false,
        /* isReadOnlyRemovableDevice */ false,
        /* profile */ {displayName: '', isCurrentProfile: true},
        /* label */ 'testLabel',
        /* extensionid */ undefined,
        /* hasMedia */ false,
        /* configurable */ false,
        /* watchable */ true,
        /* source */ VolumeManagerCommon.Source.FILE,
        /* diskFileSystemType */ VolumeManagerCommon.FileSystemType.UNKNOWN,
        /* iconSet */ {},
        /* driveLabel */ 'TEST_DRIVE_LABEL',
        /* remoteMountPath*/ '',
        /* vmType*/ undefined);
    volumeManager.volumeInfoList.add(volumeInfo);
    const promiseAfterAdd = volumeManager.whenVolumeInfoReady('volumeId');
    reportPromise(
        Promise.all([promiseBeforeAdd, promiseAfterAdd]).then((volumes) => {
          assertEquals(volumeInfo, volumes[0]);
          assertEquals(volumeInfo, volumes[1]);
        }),
        callback);
  });
}

/** @param {()=>void} callback */
export function testDriveMountedDuringInitialization(callback) {
  const test = async () => {
    const sendVolumeMetadataListPromise = new Promise(resolve => {
      chrome.fileManagerPrivate.getVolumeMetadataList = resolve;
    });

    // Start volume manager initialization.
    const volumeManagerPromise = volumeManagerFactory.getInstance();

    // Drive is mounted during initialization.
    // @ts-ignore: error TS7005: Variable 'mockChrome' implicitly has an 'any'
    // type.
    mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent({
      eventType: 'mount',
      status: 'success',
      volumeMetadata: {
        volumeId: 'drive:drive-foobar%40chromium.org-hash',
        volumeType: VolumeManagerCommon.VolumeType.DRIVE,
        sourcePath: '/drive',
        profile: getMockProfile(),
      },
    });

    // Wait until volume manager initialization calls getVolumeMetadataList().
    const sendVolumeMetadataList = await sendVolumeMetadataListPromise;

    // Inject the callback value for getVolumeMetadataList(), making the
    // initialization continue and finish.
    sendVolumeMetadataList([]);

    // Wait for volume manager to finish initializing.
    const volumeManager = await volumeManagerPromise;

    await waitUntil(() => volumeManager.volumeInfoList.length === 1);

    // Check volume manager.
    assertTrue(!!volumeManager.getCurrentProfileVolumeInfo(
        VolumeManagerCommon.VolumeType.DRIVE));
  };

  reportPromise(test(), callback);
}

// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export function testErrorPropagatedDuringInitialization(done) {
  chrome.fileManagerPrivate.getVolumeMetadataList = () => {
    throw new Error('Dummy error for test purpose');
  };

  // @ts-ignore: error TS2345: Argument of type 'Promise<VolumeManager>' is not
  // assignable to parameter of type 'Promise<void>'.
  reportPromise(assertRejected(volumeManagerFactory.getInstance()), done);
}

/**
 * Tests that an error initializing one volume doesn't stop other volumes to be
 * initialized. crbug.com/1041340
 */
// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testErrorInitializingVolume(done) {
  // Confirm that a Drive volume is on faked getVolumeMetadataList().
  // @ts-ignore: error TS7006: Parameter 'volumeMetadata' implicitly has an
  // 'any' type.
  assertTrue(mockData.volumeMetadataList_.some(volumeMetadata => {
    return volumeMetadata.volumeType === VolumeManagerCommon.VolumeType.DRIVE;
  }));

  // Replace createVolumeInfo() to fail to create Drive volume.
  // @ts-ignore: error TS7006: Parameter 'volumeMetadata' implicitly has an
  // 'any' type.
  const createVolumeInfoFake = (volumeMetadata) => {
    if (volumeMetadata.volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
      throw new Error('Fake security error');
    }

    // For any other volume return normal value.
    // @ts-ignore: error TS7005: Variable 'createVolumeInfoOriginal' implicitly
    // has an 'any' type.
    return createVolumeInfoOriginal(volumeMetadata);
  };
  volumeManagerUtil.createVolumeInfo = createVolumeInfoFake;

  // Wait for initialization to populate volumeInfoList.
  const volumeManager = new VolumeManagerImpl();
  await volumeManager.initialize();

  // VolumeInfoList should contain only Android and MyFiles.
  await waitUntil(() => volumeManager.volumeInfoList.length === 2);

  assertEquals(2, volumeManager.volumeInfoList.length);
  assertEquals(
      VolumeManagerCommon.VolumeType.DOWNLOADS,
      volumeManager.volumeInfoList.item(0).volumeType);
  assertEquals(
      VolumeManagerCommon.VolumeType.ANDROID_FILES,
      volumeManager.volumeInfoList.item(1).volumeType);

  done();
}

/**
 * Tests VolumeInfoImpl doesn't raise exception if null is passed for
 * filesystem. crbug.com/1041340
 */
// @ts-ignore: error TS7006: Parameter 'done' implicitly has an 'any' type.
export async function testDriveWithNullFilesystem(done) {
  // Get Drive volume metadata from faked getVolumeMetadataList().
  const driveVolumeMetadata =
      // @ts-ignore: error TS7006: Parameter 'volumeMetadata' implicitly has an
      // 'any' type.
      mockData.volumeMetadataList_.find(volumeMetadata => {
        return volumeMetadata.volumeType ===
            VolumeManagerCommon.VolumeType.DRIVE;
      });
  assertTrue(!!driveVolumeMetadata);

  const localizedLabel = 'DRIVE LABEL';
  const expectedError = 'EXPECTED ERROR DESCRIPTION';

  // Create a VolumeInfo with null filesystem, in the same way that happens on
  // volumeManagerUtil.createVolumeInfo().
  const volumeInfo = new VolumeInfoImpl(
      /** @type {VolumeManagerCommon.VolumeType} */
      (driveVolumeMetadata.volumeType), driveVolumeMetadata.volumeId,
      // @ts-ignore: error TS2345: Argument of type 'null' is not assignable to
      // parameter of type 'FileSystem'.
      null,  // File system is not found.
      expectedError, driveVolumeMetadata.deviceType,
      driveVolumeMetadata.devicePath, driveVolumeMetadata.isReadOnly,
      driveVolumeMetadata.isReadOnlyRemovableDevice,
      driveVolumeMetadata.profile, localizedLabel,
      driveVolumeMetadata.providerId, driveVolumeMetadata.hasMedia,
      driveVolumeMetadata.configurable, driveVolumeMetadata.watchable,
      /** @type {VolumeManagerCommon.Source} */
      (driveVolumeMetadata.source),
      /** @type {VolumeManagerCommon.FileSystemType} */
      (driveVolumeMetadata.diskFileSystemType), driveVolumeMetadata.iconSet,
      driveVolumeMetadata.driveLabel, driveVolumeMetadata.remoteMountPath,
      driveVolumeMetadata.vmType);

  // Wait for trying to resolve display root, it should fail with
  // |expectedError| if not re-throw to make the test fail.
  await volumeInfo.resolveDisplayRoot().catch(error => {
    if (error !== expectedError) {
      throw error;
    }
  });

  done();
}
