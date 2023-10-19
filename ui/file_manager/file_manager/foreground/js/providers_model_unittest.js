// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {VolumeInfoImpl} from '../../background/js/volume_info_impl.js';
import {installMockChrome, MockCommandLinePrivate} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {reportPromise} from '../../common/js/test_error_reporting.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {VolumeManager} from '../../externs/volume_manager.js';

import {ProvidersModel} from './providers_model.js';

/**
 * Providing extension which has a mounted file system and doesn't support
 * multiple mounts.
 * @type {Partial<chrome.fileManagerPrivate.Provider>}
 */
const MOUNTED_SINGLE_PROVIDING_EXTENSION = {
  name: 'mounted-single-extension-name',
  source: VolumeManagerCommon.Source.NETWORK,

  providerId: 'mounted-single-provider-id',
  iconSet: {
    icon16x16Url: 'chrome://mounted-single-extension-id-16.jpg',
    icon32x32Url: 'chrome://mounted-single-extension-id-32.jpg',
  },

  configurable: false,
  watchable: true,
  multipleMounts: false,
};

/**
 * Providing extension which has a not-mounted file system and doesn't support
 * multiple mounts.
 * @type {Partial<chrome.fileManagerPrivate.Provider>}
 */
const NOT_MOUNTED_SINGLE_PROVIDING_EXTENSION = {
  name: 'not-mounted-single-extension-name',
  source: VolumeManagerCommon.Source.NETWORK,

  providerId: 'not-mounted-single-provider-id',
  iconSet: {
    icon16x16Url: 'chrome://not-mounted-single-extension-id-16.jpg',
    icon32x32Url: 'chrome://not-mounted-single-extension-id-32.jpg',
  },

  configurable: false,
  watchable: true,
  multipleMounts: false,
};

/**
 * Providing extension which has a mounted file system and supports multiple
 * mounts.
 * @type {Partial<chrome.fileManagerPrivate.Provider>}
 */
const MOUNTED_MULTIPLE_PROVIDING_EXTENSION = {
  name: 'mounted-multiple-extension-name',
  source: VolumeManagerCommon.Source.NETWORK,

  providerId: 'mounted-multiple-provider-id',
  iconSet: {
    icon16x16Url: 'chrome://mounted-multiple-extension-id-16.jpg',
    icon32x32Url: 'chrome://mounted-multiple-extension-id-32.jpg',
  },

  configurable: true,
  watchable: false,
  multipleMounts: true,
};

/**
 * Providing extension which has a not-mounted file system of FILE source.
 * Such providers are mounted via file handlers.
 * @type {Partial<chrome.fileManagerPrivate.Provider>}
 */
const NOT_MOUNTED_FILE_PROVIDING_EXTENSION = {
  name: 'file-extension-name',
  source: VolumeManagerCommon.Source.FILE,

  providerId: 'file-provider-id',
  iconSet: {
    icon16x16Url: 'chrome://file-extension-id-16.jpg',
    icon32x32Url: 'chrome://file-extension-id-32.jpg',
  },

  configurable: false,
  watchable: true,
  multipleMounts: true,
};

/**
 * Providing extension which has a not-mounted file system of DEVICE source.
 * Such providers are not mounted by users: they automatically mount when the
 * DEVICE is attached.
 * @type {Partial<chrome.fileManagerPrivate.Provider>}
 */
const NOT_MOUNTED_DEVICE_PROVIDING_EXTENSION = {
  name: 'device-extension-name',
  source: VolumeManagerCommon.Source.DEVICE,

  providerId: 'device-provider-id',
  iconSet: {
    icon16x16Url: 'chrome://device-extension-id-16.jpg',
    icon32x32Url: 'chrome://device-extension-id-32.jpg',
  },

  configurable: false,
  watchable: true,
  multipleMounts: true,
};

/** @type {!VolumeManager} */
let volumeManager;

/**
 * @param {VolumeManager} volumeManager
 * @param {string} providerId
 * @param {string} volumeId
 */
function addProvidedVolume(volumeManager, providerId, volumeId) {
  const fileSystem = new MockFileSystem(volumeId, 'filesystem:' + volumeId);
  fileSystem.entries['/'] = MockDirectoryEntry.create(fileSystem, '');

  const volumeInfo = new VolumeInfoImpl(
      VolumeManagerCommon.VolumeType.PROVIDED, volumeId, fileSystem,
      '',                                          // error
      '',                                          // deviceType
      '',                                          // devicePath
      false,                                       // isReadonly
      false,                                       // isReadonlyRemovableDevice
      {isCurrentProfile: true, displayName: ''},   // profile
      '',                                          // label
      providerId,                                  // providerId
      false,                                       // hasMedia
      false,                                       // configurable
      false,                                       // watchable
      VolumeManagerCommon.Source.NETWORK,          // source
      VolumeManagerCommon.FileSystemType.UNKNOWN,  // diskFileSystemType
      // @ts-ignore: error TS2345: Argument of type '{}' is not assignable to
      // parameter of type 'IconSet'.
      {},          // iconSet
      '',          // driveLabel
      '',          // remoteMountPath
      undefined);  // vmType

  volumeManager.volumeInfoList.add(volumeInfo);
}

export function setUp() {
  const mockChrome = {
    runtime: {},
    fileManagerPrivate: {
      /** @param {(_: any[])=>void} callback */
      getProviders: function(callback) {
        callback([
          MOUNTED_SINGLE_PROVIDING_EXTENSION,
          NOT_MOUNTED_SINGLE_PROVIDING_EXTENSION,
          MOUNTED_MULTIPLE_PROVIDING_EXTENSION,
          NOT_MOUNTED_FILE_PROVIDING_EXTENSION,
          NOT_MOUNTED_DEVICE_PROVIDING_EXTENSION,
        ]);
      },
    },
  };

  installMockChrome(mockChrome);
  new MockCommandLinePrivate();

  // Install mock volume manager.
  volumeManager = new MockVolumeManager();

  // Add provided test volumes.
  const single = MOUNTED_SINGLE_PROVIDING_EXTENSION.providerId;
  // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
  // assignable to parameter of type 'string'.
  addProvidedVolume(volumeManager, single, 'volume-1');
  const multiple = MOUNTED_MULTIPLE_PROVIDING_EXTENSION.providerId;
  // @ts-ignore: error TS2345: Argument of type 'string | undefined' is not
  // assignable to parameter of type 'string'.
  addProvidedVolume(volumeManager, multiple, 'volume-2');
}

/** @param {()=>void} callback */
export function testGetInstalledProviders(callback) {
  const model = new ProvidersModel(volumeManager);
  reportPromise(
      model.getInstalledProviders().then(providers => {
        assertEquals(5, providers.length);
        assertEquals(
            MOUNTED_SINGLE_PROVIDING_EXTENSION.providerId,
            providers[0]?.providerId);
        assertEquals(
            MOUNTED_SINGLE_PROVIDING_EXTENSION.iconSet, providers[0]?.iconSet);
        assertEquals(
            MOUNTED_SINGLE_PROVIDING_EXTENSION.name, providers[0]?.name);
        assertEquals(
            MOUNTED_SINGLE_PROVIDING_EXTENSION.configurable,
            providers[0]?.configurable);
        assertEquals(
            MOUNTED_SINGLE_PROVIDING_EXTENSION.watchable,
            providers[0]?.watchable);
        assertEquals(
            MOUNTED_SINGLE_PROVIDING_EXTENSION.multipleMounts,
            providers[0]?.multipleMounts);
        assertEquals(
            MOUNTED_SINGLE_PROVIDING_EXTENSION.source, providers[0]?.source);

        assertEquals(
            NOT_MOUNTED_SINGLE_PROVIDING_EXTENSION.providerId,
            providers[1]?.providerId);
        assertEquals(
            MOUNTED_MULTIPLE_PROVIDING_EXTENSION.providerId,
            providers[2]?.providerId);
        assertEquals(
            NOT_MOUNTED_FILE_PROVIDING_EXTENSION.providerId,
            providers[3]?.providerId);
        assertEquals(
            NOT_MOUNTED_DEVICE_PROVIDING_EXTENSION.providerId,
            providers[4]?.providerId);

        assertEquals(
            NOT_MOUNTED_SINGLE_PROVIDING_EXTENSION.iconSet,
            providers[1]?.iconSet);
        assertEquals(
            MOUNTED_MULTIPLE_PROVIDING_EXTENSION.iconSet,
            providers[2]?.iconSet);
        assertEquals(
            NOT_MOUNTED_FILE_PROVIDING_EXTENSION.iconSet,
            providers[3]?.iconSet);
        assertEquals(
            NOT_MOUNTED_DEVICE_PROVIDING_EXTENSION.iconSet,
            providers[4]?.iconSet);
      }),
      callback);
}

/** @param {()=>void} callback */
export function testGetMountableProviders(callback) {
  const model = new ProvidersModel(volumeManager);
  reportPromise(
      model.getMountableProviders().then(providers => {
        assertEquals(2, providers.length);
        assertEquals(
            NOT_MOUNTED_SINGLE_PROVIDING_EXTENSION.providerId,
            providers[0]?.providerId);
        assertEquals(
            MOUNTED_MULTIPLE_PROVIDING_EXTENSION.providerId,
            providers[1]?.providerId);
      }),
      callback);
}
