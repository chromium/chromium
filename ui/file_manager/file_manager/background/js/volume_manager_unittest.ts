// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.js';
import {assertEquals, assertFalse, assertTrue} from 'chrome://webui-test/chromeos/chai_assert.js';

import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockFileEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {assertRejected, waitUntil} from '../../common/js/test_error_reporting.js';
import {FileSystemType, RootType, Source, VolumeError, VolumeType} from '../../common/js/volume_manager_types.js';

import {VolumeInfo} from './volume_info.js';
import {createVolumeInfo, VolumeManager} from './volume_manager.js';
import {volumeManagerFactory} from './volume_manager_factory.js';

let mockChrome: {
  fileManagerPrivate: Partial<typeof chrome.fileManagerPrivate>,
};
let mockData: {
  mountSourcePath_: string|null,
  onMountCompletedListeners_:
      Array<(event: chrome.fileManagerPrivate.MountCompletedEvent) => void>,
  onDriveConnectionStatusChangedListeners_: Array<(event: Event) => void>,
  driveConnectionState_: chrome.fileManagerPrivate.DriveConnectionState,
  volumeMetadataList_: chrome.fileManagerPrivate.VolumeMetadata[],
  password: string|undefined,
  setDriveConnectionState:
      (state: chrome.fileManagerPrivate.DriveConnectionStateType) => void,
  fileSystemMap_: {
    [volumeId: string]: MockFileSystem,
  },
};
let webkitResolveLocalFileSystemURLOriginal:
    typeof window.webkitResolveLocalFileSystemURL;

export function setUp() {
  mockData = {
    mountSourcePath_: null,
    onMountCompletedListeners_: [],
    onDriveConnectionStatusChangedListeners_: [],
    driveConnectionState_:
        {type: chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE},
    volumeMetadataList_: [],
    password: undefined,
    setDriveConnectionState(state) {
      mockData.driveConnectionState_ = {type: state};
      chrome.fileManagerPrivate.onDriveConnectionStatusChanged.dispatchEvent(
          new Event('anything'));
    },
    fileSystemMap_: {},
  };

  // Set up mock of chrome.fileManagerPrivate APIs.
  mockChrome = {
    fileManagerPrivate: {
      addMount: function(_, password, callback) {
        mockData.password = password!;
        callback(mockData.mountSourcePath_!);
      },
      getVolumeRoot: function(options, callback) {
        if (!(options.volumeId in mockData.fileSystemMap_)) {
          chrome.runtime.lastError = {message: 'Not found.'};
          callback(undefined as unknown as DirectoryEntry);
          chrome.runtime.lastError = undefined;
          return;
        }
        callback(mockData.fileSystemMap_[options.volumeId]!.root);
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
        addListener: function(listener: (event: Event) => void) {
          mockData.onDriveConnectionStatusChangedListeners_.push(listener);
        },
        dispatchEvent: function(event: Event) {
          mockData.onDriveConnectionStatusChangedListeners_.forEach(
              listener => {
                listener(event);
              });
        },
      },
      onMountCompleted: {
        addListener: function(
            listener: (event: chrome.fileManagerPrivate.MountCompletedEvent) =>
                void) {
          mockData.onMountCompletedListeners_.push(listener);
        },
        dispatchEvent: function(
            event: chrome.fileManagerPrivate.MountCompletedEvent) {
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
  mockData.volumeMetadataList_ = [
    {
      volumeId: 'download:Downloads',
      volumeLabel: '',
      volumeType: VolumeType.DOWNLOADS,
      isReadOnly: false,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: Source.SYSTEM,
    } as chrome.fileManagerPrivate.VolumeMetadata,
    {
      volumeId: 'drive:drive-foobar%40chromium.org-hash',
      volumeLabel: '',
      volumeType: VolumeType.DRIVE,
      isReadOnly: false,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: Source.NETWORK,
    } as chrome.fileManagerPrivate.VolumeMetadata,
    {
      volumeId: 'android_files:0',
      volumeLabel: '',
      volumeType: VolumeType.ANDROID_FILES,
      isReadOnly: false,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: Source.SYSTEM,
    } as chrome.fileManagerPrivate.VolumeMetadata,
  ];
  mockData.fileSystemMap_ = {
    'download:Downloads': new MockFileSystem('download:Downloads'),
    'drive:drive-foobar%40chromium.org-hash':
        new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
    'android_files:0': new MockFileSystem('android_files:0'),
  };

  const driveFs =
      mockData.fileSystemMap_['drive:drive-foobar%40chromium.org-hash']!;
  driveFs.populate(['/root/', '/team_drives/', '/Computers/']);

  // Mock window.webkitResolveLocalFileSystemURL to return entries for DriveFS.
  webkitResolveLocalFileSystemURLOriginal =
      window.webkitResolveLocalFileSystemURL;
  window.webkitResolveLocalFileSystemURL = (url, success) => {
    const match = url.match(/^filesystem:drive:.*(\/.*)/);
    if (match) {
      const path = match[1];
      const entry = driveFs.entries[path!];
      if (entry) {
        return setTimeout(success, 0, entry);
      }
    }
    throw new DOMException('Unknown drive url: ' + url, 'NotFoundError');
  };
}

export function tearDown() {
  volumeManagerFactory.revokeInstanceForTesting();
  // To avoid a closure warning assigning to |chrome|, tearDown() does not
  // balance the call to installMockChrome() here.

  // Restore window.webkitResolveLocalFileSystemURL.
  window.webkitResolveLocalFileSystemURL =
      webkitResolveLocalFileSystemURLOriginal;
}

async function waitAllVolumes(volumeManager: VolumeManager) {
  // Drive + Downloads + Android:
  await waitUntil(() => volumeManager.volumeInfoList.length === 3);
}

/**
 * Returns a mock profile.
 */
function getMockProfile():
    {displayName: string, isCurrentProfile: boolean, profileId: string} {
  return {
    displayName: 'foobar@chromium.org',
    isCurrentProfile: true,
    profileId: '',
  };
}

export async function testGetVolumeInfo(done: VoidCallback) {
  const volumeManager = await volumeManagerFactory.getInstance();

  const entry = MockFileEntry.create(
      new MockFileSystem('download:Downloads'), '/foo/bar/bla.zip');

  await waitAllVolumes(volumeManager);

  const volumeInfo = volumeManager.getVolumeInfo(entry);
  assert(volumeInfo);
  assertEquals('download:Downloads', volumeInfo.volumeId);
  assertEquals(VolumeType.DOWNLOADS, volumeInfo.volumeType);

  done();
}

/**
 * Tests that an unresponsive volume doesn't lock the whole Volume Manager
 * initialization.
 */
export async function testUnresponsiveVolumeStartUp(done: VoidCallback) {
  let unblock: VoidCallback;
  const fileManagerPrivate = chrome.fileManagerPrivate;

  // Replace chrome.fileManagerPrivate.getVolumeRoot() to emulate 1
  // volume not resolving.
  const origGetVolumeRoot = fileManagerPrivate.getVolumeRoot;

  fileManagerPrivate.getVolumeRoot =
      (options: chrome.fileManagerPrivate.GetVolumeRootOptions,
       callback: DirectoryEntryCallback) => {
        if (options.volumeId === 'download:Downloads') {
          console.info(`blocking the resolve for ${options.volumeId}`);
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
  unblock!();
  await waitUntil(() => volumeManager.volumeInfoList.length === 3);

  done();
}

/**
 * Tests that failing to get the root doesn't add a volume to the
 * `volumeInfoList`.
 */
export async function testFailureToGetRootStartUp(done: VoidCallback) {
  const fileManagerPrivate = chrome.fileManagerPrivate;

  // Replace chrome.fileManagerPrivate.getVolumeRoot() to emulate failure to get
  // 1 volume root.
  const origGetVolumeRoot = fileManagerPrivate.getVolumeRoot;
  fileManagerPrivate.getVolumeRoot =
      (options: chrome.fileManagerPrivate.GetVolumeRootOptions,
       callback: DirectoryEntryCallback) => {
        if (options.volumeId === 'download:Downloads') {
          chrome.runtime.lastError = {message: 'Not found.'};
          callback(undefined as unknown as DirectoryEntry);
          chrome.runtime.lastError = undefined;
          return;
        }
        return origGetVolumeRoot(options, callback);
      };

  // getInstance() calls and waits for initialize(), which shouldn't add
  // 'download:Downloads' to the volumeInfoList when failing to get the root.
  const volumeManager = await volumeManagerFactory.getInstance();

  await waitUntil(() => volumeManager.volumeInfoList.length === 2);

  done();
}

export async function testGetDriveConnectionState(done: VoidCallback) {
  const volumeManager = await volumeManagerFactory.getInstance();
  // Default connection state is online
  assertEquals(
      chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
      volumeManager.getDriveConnectionState().type);

  // Sets it to offline.
  mockData.setDriveConnectionState(
      chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE);
  assertEquals(
      chrome.fileManagerPrivate.DriveConnectionStateType.OFFLINE,
      volumeManager.getDriveConnectionState().type);

  // Sets it back to online
  mockData.setDriveConnectionState(
      chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE);
  assertEquals(
      chrome.fileManagerPrivate.DriveConnectionStateType.ONLINE,
      volumeManager.getDriveConnectionState().type);

  done();
}

export async function testMountArchiveAndUnmount(done: VoidCallback) {
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
      volumeType: VolumeType.ARCHIVE,
      isReadOnly: true,
      sourcePath: mountSourcePath,
      profile: getMockProfile(),
      configurable: false,
      watchable: true,
      source: Source.FILE,
    },
  });

  await mounted;

  assertEquals(numberOfVolumes + 1, volumeManager.volumeInfoList.length);
  assertEquals(password, mockData.password);

  // Unmount the mounted archive
  const entry = MockFileEntry.create(
      new MockFileSystem('archive:foobar.zip'), '/foo.txt');
  const volumeInfo = volumeManager.getVolumeInfo(entry);
  assert(volumeInfo);
  await volumeManager.unmount(volumeInfo);

  await waitUntil(
      () => volumeManager.volumeInfoList.length === numberOfVolumes);
  assertEquals(numberOfVolumes, volumeManager.volumeInfoList.length);

  done();
}

export async function testCancelMountingArchive(done: VoidCallback) {
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
        status: VolumeError.CANCELLED,
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
    assertEquals(error, VolumeError.CANCELLED);
  }

  assertEquals(numberOfVolumes, volumeManager.volumeInfoList.length);

  done();
}

export async function testGetCurrentProfileVolumeInfo(done: VoidCallback) {
  const volumeManager = await volumeManagerFactory.getInstance();
  await waitAllVolumes(volumeManager);

  const volumeInfo =
      volumeManager.getCurrentProfileVolumeInfo(VolumeType.DRIVE);
  assert(volumeInfo);

  assertEquals('drive:drive-foobar%40chromium.org-hash', volumeInfo.volumeId);
  assertEquals(VolumeType.DRIVE, volumeInfo.volumeType);

  done();
}

export async function testGetLocationInfo(done: VoidCallback) {
  const volumeManager = await volumeManagerFactory.getInstance();
  await waitAllVolumes(volumeManager);

  const downloadEntry = MockFileEntry.create(
      new MockFileSystem('download:Downloads'), '/foo/bar/bla.zip');
  const downloadLocationInfo = volumeManager.getLocationInfo(downloadEntry);
  assert(downloadLocationInfo);
  assertEquals(RootType.DOWNLOADS, downloadLocationInfo.rootType);
  assertFalse(downloadLocationInfo.hasFixedLabel);
  assertFalse(downloadLocationInfo.isReadOnly);
  assertFalse(downloadLocationInfo.isRootEntry);

  const driveEntry = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'), '/root');
  const driveLocationInfo = volumeManager.getLocationInfo(driveEntry);
  assert(driveLocationInfo);
  assertEquals(RootType.DRIVE, driveLocationInfo.rootType);
  assertTrue(driveLocationInfo.hasFixedLabel);
  assertFalse(driveLocationInfo.isReadOnly);
  assertTrue(driveLocationInfo.isRootEntry);

  const teamDrivesGrandRoot = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/team_drives');
  const teamDrivesGrandRootLocationInfo =
      volumeManager.getLocationInfo(teamDrivesGrandRoot);
  assert(teamDrivesGrandRootLocationInfo);
  assertEquals(
      RootType.SHARED_DRIVES_GRAND_ROOT,
      teamDrivesGrandRootLocationInfo.rootType);
  assertTrue(teamDrivesGrandRootLocationInfo.hasFixedLabel);
  assertTrue(teamDrivesGrandRootLocationInfo.isReadOnly);
  assertTrue(teamDrivesGrandRootLocationInfo.isRootEntry);

  const teamDrive = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/team_drives/MyTeamDrive');
  const teamDriveLocationInfo = volumeManager.getLocationInfo(teamDrive);
  assert(teamDriveLocationInfo);
  assertEquals(RootType.SHARED_DRIVE, teamDriveLocationInfo.rootType);
  assertFalse(teamDriveLocationInfo.hasFixedLabel);
  assertFalse(teamDriveLocationInfo.isReadOnly);
  assertTrue(teamDriveLocationInfo.isRootEntry);

  const driveFilesByIdDirectoryEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.files-by-id/123');
  const driveFilesByIdDirectoryLocationInfo =
      volumeManager.getLocationInfo(driveFilesByIdDirectoryEntry);
  assert(driveFilesByIdDirectoryLocationInfo);
  assertEquals(
      RootType.DRIVE_SHARED_WITH_ME,
      driveFilesByIdDirectoryLocationInfo.rootType);
  assertFalse(driveFilesByIdDirectoryLocationInfo.hasFixedLabel);
  assertTrue(driveFilesByIdDirectoryLocationInfo.isReadOnly);
  assertFalse(driveFilesByIdDirectoryLocationInfo.isRootEntry);

  const driveFilesByIdEntry = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.files-by-id/123/foo.txt');
  const driveFilesByIdLocationInfo =
      volumeManager.getLocationInfo(driveFilesByIdEntry);
  assert(driveFilesByIdLocationInfo);
  assertEquals(
      RootType.DRIVE_SHARED_WITH_ME, driveFilesByIdLocationInfo.rootType);
  assertFalse(driveFilesByIdLocationInfo.hasFixedLabel);
  assertFalse(driveFilesByIdLocationInfo.isReadOnly);
  assertFalse(driveFilesByIdLocationInfo.isRootEntry);

  const driveShortcutTargetsByIdDirectoryEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.shortcut-targets-by-id/abcdef');
  const driveShortcutTargetsByIdDirectoryLocationInfo =
      volumeManager.getLocationInfo(driveShortcutTargetsByIdDirectoryEntry);
  assert(driveShortcutTargetsByIdDirectoryLocationInfo);
  assertEquals(
      RootType.DRIVE_SHARED_WITH_ME,
      driveShortcutTargetsByIdDirectoryLocationInfo.rootType);
  assertFalse(driveShortcutTargetsByIdDirectoryLocationInfo.hasFixedLabel);
  assertTrue(driveShortcutTargetsByIdDirectoryLocationInfo.isReadOnly);
  assertFalse(driveShortcutTargetsByIdDirectoryLocationInfo.isRootEntry);

  const driveShortcutTargetsByIdEntry = MockDirectoryEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/.shortcut-targets-by-id/abcdef/foo');
  const driveShortcutTargetsByIdLocationInfo =
      volumeManager.getLocationInfo(driveShortcutTargetsByIdEntry);
  assert(driveShortcutTargetsByIdLocationInfo);
  assertEquals(
      RootType.DRIVE_SHARED_WITH_ME,
      driveShortcutTargetsByIdLocationInfo.rootType);
  assertFalse(driveShortcutTargetsByIdLocationInfo.hasFixedLabel);
  assertFalse(driveShortcutTargetsByIdLocationInfo.isReadOnly);
  assertFalse(driveShortcutTargetsByIdLocationInfo.isRootEntry);

  const androidRoot =
      MockFileEntry.create(new MockFileSystem('android_files:0'), '/');
  const androidRootLocationInfo = volumeManager.getLocationInfo(androidRoot);
  assert(androidRootLocationInfo);
  assertTrue(androidRootLocationInfo.isReadOnly);
  assertTrue(androidRootLocationInfo.isRootEntry);

  const androidSubFolder =
      MockFileEntry.create(new MockFileSystem('android_files:0'), '/Pictures');
  const androidSubFolderLocationInfo =
      volumeManager.getLocationInfo(androidSubFolder);
  assert(androidSubFolderLocationInfo);
  assertFalse(androidSubFolderLocationInfo.isReadOnly);
  assertFalse(androidSubFolderLocationInfo.isRootEntry);

  const computersGrandRoot = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/Computers');
  const computersGrandRootLocationInfo =
      volumeManager.getLocationInfo(computersGrandRoot);
  assert(computersGrandRootLocationInfo);
  assertEquals(
      RootType.COMPUTERS_GRAND_ROOT, computersGrandRootLocationInfo.rootType);
  assertTrue(computersGrandRootLocationInfo.hasFixedLabel);
  assertTrue(computersGrandRootLocationInfo.isReadOnly);
  assertTrue(computersGrandRootLocationInfo.isRootEntry);

  const computer = MockFileEntry.create(
      new MockFileSystem('drive:drive-foobar%40chromium.org-hash'),
      '/Computers/MyComputer');
  const computerLocationInfo = volumeManager.getLocationInfo(computer);
  assert(computerLocationInfo);
  assertEquals(RootType.COMPUTER, computerLocationInfo.rootType);
  assertFalse(computerLocationInfo.hasFixedLabel);
  assertTrue(computerLocationInfo.isReadOnly);
  assertTrue(computerLocationInfo.isRootEntry);

  done();
}

export async function testWhenReady(done: VoidCallback) {
  const volumeManager = await volumeManagerFactory.getInstance();
  const promiseBeforeAdd = volumeManager.whenVolumeInfoReady('volumeId');
  const volumeInfo = new VolumeInfo(
      /* volumeType */ VolumeType.MY_FILES,
      /* volumeId */ 'volumeId',
      /* fileSystem */ new MockFileSystem('testName', '/testURL'),
      /* error */ undefined,
      /* deviceType */ undefined,
      /* devicePath */ undefined,
      /* isReadOnly */ false,
      /* isReadOnlyRemovableDevice */ false,
      /* profile */ {displayName: '', isCurrentProfile: true},
      /* label */ 'testLabel',
      /* extensionid */ undefined,
      /* configurable */ false,
      /* watchable */ true,
      /* source */ Source.FILE,
      /* diskFileSystemType */ FileSystemType.UNKNOWN,
      /* iconSet */ {icon16x16Url: '', icon32x32Url: ''},
      /* driveLabel */ 'TEST_DRIVE_LABEL',
      /* remoteMountPath*/ '',
      /* vmType*/ undefined);
  volumeManager.volumeInfoList.add(volumeInfo);
  const promiseAfterAdd = volumeManager.whenVolumeInfoReady('volumeId');
  await Promise.all([promiseBeforeAdd, promiseAfterAdd]).then((volumes) => {
    assertEquals(volumeInfo, volumes[0]);
    assertEquals(volumeInfo, volumes[1]);
  });

  done();
}

export async function testDriveMountedDuringInitialization(done: VoidCallback) {
  const sendVolumeMetadataListPromise = new Promise<
      (callback: chrome.fileManagerPrivate.VolumeMetadata[]) => void>(
      resolve => {
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
      volumeType: VolumeType.DRIVE,
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
  assertTrue(!!volumeManager.getCurrentProfileVolumeInfo(VolumeType.DRIVE));

  done();
}

export async function testErrorPropagatedDuringInitialization(
    done: VoidCallback) {
  chrome.fileManagerPrivate.getVolumeMetadataList = () => {
    throw new Error('Dummy error for test purpose');
  };

  await assertRejected(volumeManagerFactory.getInstance());

  done();
}

/**
 * Tests that an error initializing one volume doesn't stop other volumes to be
 * initialized. crbug.com/1041340
 */
export async function testErrorInitializingVolume(done: VoidCallback) {
  // Confirm that a Drive volume is on faked getVolumeMetadataList().
  assertTrue(mockData.volumeMetadataList_.some(volumeMetadata => {
    return volumeMetadata.volumeType === VolumeType.DRIVE;
  }));

  // Replace createVolumeInfo() to fail to create Drive volume.
  const createVolumeInfoFake: typeof createVolumeInfo = (volumeMetadata) => {
    if (volumeMetadata.volumeType === VolumeType.DRIVE) {
      throw new Error('Fake security error');
    }

    // For any other volume return normal value.
    return createVolumeInfo(volumeMetadata);
  };

  // Wait for initialization to populate volumeInfoList.
  const volumeManager = new VolumeManager(createVolumeInfoFake);
  await volumeManager.initialize();

  // VolumeInfoList should contain only Android and MyFiles.
  await waitUntil(() => volumeManager.volumeInfoList.length === 2);

  assertEquals(2, volumeManager.volumeInfoList.length);
  assertEquals(
      VolumeType.DOWNLOADS, volumeManager.volumeInfoList.item(0).volumeType);
  assertEquals(
      VolumeType.ANDROID_FILES,
      volumeManager.volumeInfoList.item(1).volumeType);

  done();
}
