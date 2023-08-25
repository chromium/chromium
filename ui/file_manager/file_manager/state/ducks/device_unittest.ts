// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertEquals, assertFalse} from 'chrome://webui-test/chromeos/chai_assert.js';

import {MockVolumeManager} from '../../background/js/mock_volume_manager.js';
import {VolumeManagerCommon} from '../../common/js/volume_manager_types.js';
import {State} from '../../externs/ts/state.js';
import {constants} from '../../foreground/js/constants.js';
import {createFakeVolumeMetadata, setupStore, waitDeepEquals} from '../for_tests.js';
import {getEmptyState, getStore, Store} from '../store.js';

import {updateDeviceConnectionState} from './device.js';
import {convertVolumeInfoAndMetadataToVolume} from './volumes.js';

let store: Store;

export function setUp() {
  store = getStore();
  store.init(getEmptyState());
}

export async function testUpdateDeviceConnection(done: () => void) {
  const volumeInfo = MockVolumeManager.createMockVolumeInfo(
      VolumeManagerCommon.VolumeType.PROVIDED, 'odfs', 'OneDrive', '',
      constants.ODFS_EXTENSION_ID, '');
  const ODFSVolume = convertVolumeInfoAndMetadataToVolume(
      volumeInfo, createFakeVolumeMetadata(volumeInfo));

  const initialState = getEmptyState();
  initialState.volumes[ODFSVolume.volumeId] = ODFSVolume;
  const store = setupStore(initialState);

  assertEquals(
      chrome.fileManagerPrivate.DeviceConnectionState.ONLINE,
      store.getState().device.connection);
  assertFalse(store.getState().volumes[ODFSVolume.volumeId].isDisabled);

  // Dispatch an action to set |connection| for the device to OFFLINE.
  store.dispatch(updateDeviceConnectionState({
    connection: chrome.fileManagerPrivate.DeviceConnectionState.OFFLINE,
  }));

  // Expect the volume to be set to disabled.
  const want: Partial<State> = {
    volumes: {
      [ODFSVolume.volumeId]: {
        ...ODFSVolume,
        isDisabled: true,
      },
    },
  };
  await waitDeepEquals(store, want, (state) => ({
                                      volumes: state.volumes,
                                    }));
  assertEquals(
      chrome.fileManagerPrivate.DeviceConnectionState.OFFLINE,
      store.getState().device.connection);

  done();
}
