// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {VolumeInfo} from '../../background/js/volume_info.js';
import type {VolumeManager} from '../../background/js/volume_manager.js';
import {installMockChrome} from '../../common/js/mock_chrome.js';
import {MockDirectoryEntry, MockFileSystem} from '../../common/js/mock_entry.js';
import {FileSystemType, Source, VolumeType} from '../../common/js/volume_manager_types.js';

import {ProvidersModel} from './providers_model.js';

/**
 * Providing extension which has a mounted file system and doesn't support
 * multiple mounts.
 */
const MOUNTED_SINGLE_PROVIDING_EXTENSION = {
  name: 'mounted-single-extension-name',
  source: chrome.fileManagerPrivate.ProviderSource.NETWORK,

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
 */
const NOT_MOUNTED_SINGLE_PROVIDING_EXTENSION = {
  name: 'not-mounted-single-extension-name',
  source: chrome.fileManagerPrivate.ProviderSource.NETWORK,

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
 */
const MOUNTED_MULTIPLE_PROVIDING_EXTENSION = {
  name: 'mounted-multiple-extension-name',
  source: chrome.fileManagerPrivate.ProviderSource.NETWORK,

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
 */
const NOT_MOUNTED_FILE_PROVIDING_EXTENSION = {
  name: 'file-extension-name',
  source: chrome.fileManagerPrivate.ProviderSource.FILE,

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
 */
const NOT_MOUNTED_DEVICE_PROVIDING_EXTENSION = {
  name: 'device-extension-name',
  source: chrome.fileManagerPrivate.ProviderSource.DEVICE,

  providerId: 'device-provider-id',
  iconSet: {
    icon16x16Url: 'chrome://device-extension-id-16.jpg',
    icon32x32Url: 'chrome://device-extension-id-32.jpg',
  },

  configurable: false,
  watchable: true,
  multipleMounts: true,
};

let volumeManager: VolumeManager;

function addProvidedVolume(
    volumeManager: VolumeManager, providerId: string, volumeId: string) {
  const fileSystem = new MockFileSystem(volumeId, 'filesystem:' + volumeId);
  fileSystem.entries['/'] = MockDirectoryEntry.create(fileSystem, '');

  const volumeInfo = new VolumeInfo(
      VolumeType.PROVIDED, volumeId, fileSystem,
      '',                                         // error
      '',                                         // deviceType
      '',                                         // devicePath
      false,                                      // isReadonly
      false,                                      // isReadonlyRemovableDevice
      {isCurrentProfile: true, displayName: ''},  // profile
      '',                                         // label
      providerId,                                 // providerId
      false,                                      // configurable
      false,                                      // watchable
      Source.NETWORK,                             // source
      FileSystemType.UNKNOWN,                     // diskFileSystemType
      {icon16x16Url: '', icon32x32Url: ''},       // iconSet
      '',                                         // driveLabel
      '',                                         // remoteMountPath
      undefined);                                 // vmType

  volumeManager.volumeInfoList.add(volumeInfo);
}

export function setUp() {
  const mockChrome = {
    runtime: {},
    fileManagerPrivate: {
      getProviders: function(
          callback: (providers: chrome.fileManagerPrivate.Provider[]) => void) {
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

  // Install mock volume manager.
  volumeManager = new MockVolumeManager();

  // Add provided test volumes.
  const single = MOUNTED_SINGLE_PROVIDING_EXTENSION.providerId;
  addProvidedVolume(volumeManager, single, 'volume-1');
  const multiple = MOUNTED_MULTIPLE_PROVIDING_EXTENSION.providerId;
  addProvidedVolume(volumeManager, multiple, 'volume-2');
}

export async function testGetInstalledProviders() {
  const model = new ProvidersModel(volumeManager);
  const providers = await model.getInstalledProviders();
  assertEquals(5, providers.length);
  assertEquals(
      MOUNTED_SINGLE_PROVIDING_EXTENSION.providerId, providers[0]?.providerId);
  assertEquals(
      MOUNTED_SINGLE_PROVIDING_EXTENSION.iconSet, providers[0]?.iconSet);
  assertEquals(MOUNTED_SINGLE_PROVIDING_EXTENSION.name, providers[0]?.name);
  assertEquals(
      MOUNTED_SINGLE_PROVIDING_EXTENSION.configurable,
      providers[0]?.configurable);
  assertEquals(
      MOUNTED_SINGLE_PROVIDING_EXTENSION.watchable, providers[0]?.watchable);
  assertEquals(
      MOUNTED_SINGLE_PROVIDING_EXTENSION.multipleMounts,
      providers[0]?.multipleMounts);
  assertEquals(MOUNTED_SINGLE_PROVIDING_EXTENSION.source, providers[0]?.source);

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
      NOT_MOUNTED_SINGLE_PROVIDING_EXTENSION.iconSet, providers[1]?.iconSet);
  assertEquals(
      MOUNTED_MULTIPLE_PROVIDING_EXTENSION.iconSet, providers[2]?.iconSet);
  assertEquals(
      NOT_MOUNTED_FILE_PROVIDING_EXTENSION.iconSet, providers[3]?.iconSet);
  assertEquals(
      NOT_MOUNTED_DEVICE_PROVIDING_EXTENSION.iconSet, providers[4]?.iconSet);
}

export async function testGetMountableProviders() {
  const model = new ProvidersModel(volumeManager);
  const providers = await model.getMountableProviders();
  assertEquals(2, providers.length);
  assertEquals(
      NOT_MOUNTED_SINGLE_PROVIDING_EXTENSION.providerId,
      providers[0]?.providerId);
  assertEquals(
      MOUNTED_MULTIPLE_PROVIDING_EXTENSION.providerId,
      providers[1]?.providerId);
}
