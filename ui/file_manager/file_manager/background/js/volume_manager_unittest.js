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

let mockChrome;
let mockData;
let createVolumeInfoOriginal;
let webkitResolveLocalFileSystemURLOriginal;

export function setUp() {
  mockData = {
    mountSourcePath_: null,
    onMountCompletedListeners_: [],
    onDriveConnectionStatusChangedListeners_: [],
    driveConnectionState_: 'ONLINE',
    volumeMetadataList_: [],
    password: undefined,
    setDriveConnectionState(state) {
      mockData.driveConnectionState_ = state;
      /** @type {!EventTarget} */ (
          chrome.fileManagerPrivate.onDriveConnectionStatusChanged)
          .dispatchEvent(new Event('anything'));
    },
  };

  // Set up mock of chrome.fileManagerPrivate APIs.
  mockChrome = {
    fileManagerPrivate: {
      addMount: function(fileUrl, password, callback) {
        mockData.password = password;
        callback(mockData.mountSourcePath_);
      },
      getVolumeRoot: function(options, callback) {
        if (!(options.volumeId in mockData.fileSystemMap_)) {
          chrome.runtime.lastError = {message: 'Not found.'};
        }
        callback(mockData.fileSystemMap_[options.volumeId].root);
      },
      removeMount: function(volumeId, callback) {
        const event = {
          eventType: 'unmount',
          status: 'success',
          volumeMetadata: {volumeId: volumeId},
        };
        mockChrome.fileManagerPrivate.onMountCompleted.dispatchEvent(event);
        callback();
      },
      onDriveConnectionStatusChanged: {
        addListener: function(listener) {
          mockData.onDriveConnectionStatusChangedListeners_.push(listener);
        },
        dispatchEvent: function(event) {
          mockData.onDriveConnectionStatusChangedListeners_.forEach(
              listener => {
                listener(event);
              });
        },
      },
      onMountCompleted: {
        addListener: function(listener) {
          mockData.onMountCompletedListeners_.push(listener);
        },
        dispatchEvent: function(event) {
          mockData.onMountCompletedListeners_.forEach(
              listener => listener(event));
        },
      },
      getDriveConnectionState: function(callback) {
        callback(mockData.driveConnectionState_);
      },
      getVolumeMetadataList: function(callback) {
        callback(mockData.volumeMetadataList_);
      },
    },
  };

  installMockChrome(mockChrome);
  new MockCommandLinePrivate();
  mockData.volumeMetadataList_ = [
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
  mockData.fileSystemMap_ = {
    'download:Downloads': new MockFileSystem('download:Downloads'),
    'drive:drive-foobar%40chromium.org-hash':
        new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
    'android_files:0': new MockFileSystem('android_files:0'),
  };

  const driveFs =
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
  volumeManagerUtil.createVolumeInfo = createVolumeInfoOriginal;

  // Restore window.webkitResolveLocalFileSystemURL.
  window.webkitResolveLocalFileSystemURL =
      webkitResolveLocalFileSystemURLOriginal;
}

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

export async function testGetVolumeInfo(done) {
  const volumeManager = await volumeManagerFactory.getInstance();

  const entry = MockFileEntry.create(
      new MockFileSystem('download:Downloads'), '/foo/bar/bla.zip');

  await waitAllVolumes(volumeManager);

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  assertEquals('download:Downloads', volumeInfo.volumeId);
  assertEquals(VolumeManagerCommon.VolumeType.DOWNLOADS, volumeInfo.volumeType);

  done();
}

/**
 * Tests that an unresponsive volume doesn't lock the whole Volume Manager
 * initialization.
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
  unblock();
  await waitUntil(() => volumeManager.volumeInfoList.length === 3);

  done();
}

export function testGetDriveConnectionState(callback) {
  reportPromise(
      volumeManagerFactory.getInstance().then(volumeManager => {
        // Default connection state is online
        assertEquals(
            chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
            volumeManager.getDriveConnectionState());

        // Sets it to offline.
        mockData.setDriveConnectionState(
            chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE);
        assertEquals(
            chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
            volumeManager.getDriveConnectionState());

        // Sets it back to online
        mockData.setDriveConnectionState(
            chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE);
        assertEquals(
            chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
            volumeManager.getDriveConnectionState());
      }),
      callback);
}

export function testMountArchiveAndUnmount(callback) {
  const test = async () => {
    // Set states of mock fileManagerPrivate APIs.
    const mountSourcePath = '/usr/local/home/test/Downloads/foobar.zip';
    mockData.mountSourcePath_ = mountSourcePath;
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
    assertEquals(password, mockData.password);

    // Unmount the mounted archive
    const entry = MockFileEntry.create(
        new MockFileSystem('archive:foobar.zip'), '/foo.txt');
    const volumeInfo = volumeManager.getVolumeInfo(entry);
    await volumeManager.unmount(volumeInfo);

    await waitUntil(
        () => volumeManager.volumeInfoList.length === numberOfVolumes);
    assertEquals(numberOfVolumes, volumeManager.volumeInfoList.length);
  };

  reportPromise(test(), callback);
}

export function testCancelMountingArchive(callback) {
  const test = async () => {
    // Set states of mock fileManagerPrivate APIs.
    const mountSourcePath = '/usr/local/home/test/Downloads/foobar.zip';
    mockData.mountSourcePath_ = mountSourcePath;
    mockData.fileSystemMap_['archive:foobar.zip'] =
        new MockFileSystem('archive:foobar.zip');

    const volumeManager = await volumeManagerFactory.getInstance();

    // Drive + Downloads + Android.
    const numberOfVolumes = 3;
    await waitAllVolumes(volumeManager);

    setTimeout(
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

export async function testGetCurrentProfileVolumeInfo(done) {
  const volumeManager = await volumeManagerFactory.getInstance();
  await waitAllVolumes(volumeManager);

  const volumeInfo = volumeManager.getCurrentProfileVolumeInfo(
      VolumeManagerCommon.VolumeType.DRIVE);

  assertEquals('drive:drive-foobar%40chromium.org-hash', volumeInfo.volumeId);
  assertEquals(VolumeManagerCommon.VolumeType.DRIVE, volumeInfo.volumeType);

  done();
}

export async function testGetLocationInfo(done) {
  const volumeManager = await volumeManagerFactory.getInstance();
  await waitAllVolumes(volumeManager);

  const downloadEntry = MockFileEntry.create(
      new MockFileSystem('download:Downloads'), '/foo/bar/bla.zip');
  const downloadLocationInfo = volumeManager.getLocationInfo(downloadEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DOWNLOADS, downloadLocationInfo.rootType);
  assertFalse(downloadLocationInfo.hasFixedLabel);
  assertFalse(downloadLocationInfo.isReadOnly);
  assertFalse(downloadLocationInfo.isRootEntry);

  const driveEntry = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'), '/root');
  const driveLocationInfo = volumeManager.getLocationInfo(driveEntry);
  assertEquals(VolumeManagerCommon.RootType.DRIVE, driveLocationInfo.rootType);
  assertTrue(driveLocationInfo.hasFixedLabel);
  assertFalse(driveLocationInfo.isReadOnly);
  assertTrue(driveLocationInfo.isRootEntry);

  const teamDrivesGrandRoot = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/team_drives');
  const teamDrivesGrandRootLocationInfo =
      volumeManager.getLocationInfo(teamDrivesGrandRoot);
  assertEquals(
      VolumeManagerCommon.RootType.SHARED_DRIVES_GRAND_ROOT,
      teamDrivesGrandRootLocationInfo.rootType);
  assertTrue(teamDrivesGrandRootLocationInfo.hasFixedLabel);
  assertTrue(teamDrivesGrandRootLocationInfo.isReadOnly);
  assertTrue(teamDrivesGrandRootLocationInfo.isRootEntry);

  const teamDrive = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/team_drives/MyTeamDrive');
  const teamDriveLocationInfo = volumeManager.getLocationInfo(teamDrive);
  assertEquals(
      VolumeManagerCommon.RootType.SHARED_DRIVE,
      teamDriveLocationInfo.rootType);
  assertFalse(teamDriveLocationInfo.hasFixedLabel);
  assertFalse(teamDriveLocationInfo.isReadOnly);
  assertTrue(teamDriveLocationInfo.isRootEntry);

  const driveFilesByIdDirectoryEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.files-by-id/123');
  const driveFilesByIdDirectoryLocationInfo =
      volumeManager.getLocationInfo(driveFilesByIdDirectoryEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      driveFilesByIdDirectoryLocationInfo.rootType);
  assertFalse(driveFilesByIdDirectoryLocationInfo.hasFixedLabel);
  assertTrue(driveFilesByIdDirectoryLocationInfo.isReadOnly);
  assertFalse(driveFilesByIdDirectoryLocationInfo.isRootEntry);

  const driveFilesByIdEntry = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.files-by-id/123/foo.txt');
  const driveFilesByIdLocationInfo =
      volumeManager.getLocationInfo(driveFilesByIdEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      driveFilesByIdLocationInfo.rootType);
  assertFalse(driveFilesByIdLocationInfo.hasFixedLabel);
  assertFalse(driveFilesByIdLocationInfo.isReadOnly);
  assertFalse(driveFilesByIdLocationInfo.isRootEntry);

  const driveShortcutTargetsByIdDirectoryEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.shortcut-targets-by-id/abcdef');
  const driveShortcutTargetsByIdDirectoryLocationInfo =
      volumeManager.getLocationInfo(driveShortcutTargetsByIdDirectoryEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      driveShortcutTargetsByIdDirectoryLocationInfo.rootType);
  assertFalse(driveShortcutTargetsByIdDirectoryLocationInfo.hasFixedLabel);
  assertTrue(driveShortcutTargetsByIdDirectoryLocationInfo.isReadOnly);
  assertFalse(driveShortcutTargetsByIdDirectoryLocationInfo.isRootEntry);

  const driveShortcutTargetsByIdEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.shortcut-targets-by-id/abcdef/foo');
  const driveShortcutTargetsByIdLocationInfo =
      volumeManager.getLocationInfo(driveShortcutTargetsByIdEntry);
  assertEquals(
      VolumeManagerCommon.RootType.DRIVE_SHARED_WITH_ME,
      driveShortcutTargetsByIdLocationInfo.rootType);
  assertFalse(driveShortcutTargetsByIdLocationInfo.hasFixedLabel);
  assertFalse(driveShortcutTargetsByIdLocationInfo.isReadOnly);
  assertFalse(driveShortcutTargetsByIdLocationInfo.isRootEntry);

  const androidRoot =
      MockFileEntry.create(new MockFileSystem('android_files:0'), '/');
  const androidRootLocationInfo = volumeManager.getLocationInfo(androidRoot);
  assertTrue(androidRootLocationInfo.isReadOnly);
  assertTrue(androidRootLocationInfo.isRootEntry);

  const androidSubFolder =
      MockFileEntry.create(new MockFileSystem('android_files:0'), '/Pictures');
  const androidSubFolderLocationInfo =
      volumeManager.getLocationInfo(androidSubFolder);
  assertFalse(androidSubFolderLocationInfo.isReadOnly);
  assertFalse(androidSubFolderLocationInfo.isRootEntry);

  const computersGrandRoot = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/Computers');
  const computersGrandRootLocationInfo =
      volumeManager.getLocationInfo(computersGrandRoot);
  assertEquals(
      VolumeManagerCommon.RootType.COMPUTERS_GRAND_ROOT,
      computersGrandRootLocationInfo.rootType);
  assertTrue(computersGrandRootLocationInfo.hasFixedLabel);
  assertTrue(computersGrandRootLocationInfo.isReadOnly);
  assertTrue(computersGrandRootLocationInfo.isRootEntry);

  const computer = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/Computers/MyComputer');
  const computerLocationInfo = volumeManager.getLocationInfo(computer);
  assertEquals(
      VolumeManagerCommon.RootType.COMPUTER, computerLocationInfo.rootType);
  assertFalse(computerLocationInfo.hasFixedLabel);
  assertTrue(computerLocationInfo.isReadOnly);
  assertTrue(computerLocationInfo.isRootEntry);

  done();
}

export function testWhenReady(callback) {
  volumeManagerFactory.getInstance().then((volumeManager) => {
    const promiseBeforeAdd = volumeManager.whenVolumeInfoReady('volumeId');
    const volumeInfo = new VolumeInfoImpl(
        /* volumeType */ VolumeManagerCommon.VolumeType.MY_FILES,
        /* volumeId */ 'volumeId',
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

export function testDriveMountedDuringInitialization(callback) {
  const test = async () => {
    const sendVolumeMetadataListPromise = new Promise(resolve => {
      chrome.fileManagerPrivate.getVolumeMetadataList = resolve;
    });

    // Start volume manager initialization.
    const volumeManagerPromise = volumeManagerFactory.getInstance();

    // Drive is mounted during initialization.
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

export function testErrorPropagatedDuringInitialization(done) {
  chrome.fileManagerPrivate.getVolumeMetadataList = () => {
    throw new Error('Dummy error for test purpose');
  };

  reportPromise(assertRejected(volumeManagerFactory.getInstance()), done);
}

/**
 * Tests that an error initializing one volume doesn't stop other volumes to be
 * initialized. crbug.com/1041340
 */
export async function testErrorInitializingVolume(done) {
  // Confirm that a Drive volume is on faked getVolumeMetadataList().
  assertTrue(mockData.volumeMetadataList_.some(volumeMetadata => {
    return volumeMetadata.volumeType === VolumeManagerCommon.VolumeType.DRIVE;
  }));

  // Replace createVolumeInfo() to fail to create Drive volume.
  const createVolumeInfoFake = (volumeMetadata) => {
    if (volumeMetadata.volumeType === VolumeManagerCommon.VolumeType.DRIVE) {
      throw new Error('Fake security error');
    }

    // For any other volume return normal value.
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
export async function testDriveWithNullFilesystem(done) {
  // Get Drive volume metadata from faked getVolumeMetadataList().
  const driveVolumeMetadata =
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
