// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {util} from '../../common/js/util.js';
import {State, Volume} from '../../externs/ts/state.js';
import {Slice} from '../../lib/base_store.js';
import {updateFileData} from '../ducks/all_entries.js';
import {getEntry} from '../store.js';

import {updateVolume} from './volumes.js';

/**
 * @fileoverview Device slice of the store.
 * @suppress {checkTypes}
 */

const slice = new Slice<State>('device');
export {slice as deviceSlice};

export const updateDeviceConnectionState = slice.addReducer(
    'set-connection-state', updateDeviceConnectionStateReducer);

function updateDeviceConnectionStateReducer(currentState: State, payload: {
  connection: chrome.fileManagerPrivate.DeviceConnectionState,
}): State {
  let device: State['device']|undefined;
  let volumes: State['volumes']|undefined;

  // Device connection.
  if (payload.connection !== currentState.device.connection) {
    device = {
      ...currentState.device,
      connection: payload.connection,
    };
  }

  // Find ODFS volume(s) and disable it (or them) if offline.
  const disableODFS = payload.connection ===
      chrome.fileManagerPrivate.DeviceConnectionState.OFFLINE;
  for (const volume of Object.values<Volume>(currentState.volumes)) {
    if (!util.isOneDriveId(volume.providerId) ||
        volume.isDisabled === disableODFS) {
      continue;
    }
    const updatedVolume =
        updateVolume(currentState, volume.volumeId, {isDisabled: disableODFS});
    if (updatedVolume) {
      if (!volumes) {
        volumes = {
          ...currentState.volumes,
          [volume.volumeId]: updatedVolume,
        };
      } else {
        volumes[volume.volumeId] = updatedVolume;
      }
    }
    // Make the ODFS FileData/VolumeEntry consistent with its volume in the
    // store.
    updateFileData(currentState, volume.rootKey!, {disabled: disableODFS});
    const odfsVolumeEntry =
        getEntry(currentState, volume.rootKey!) as VolumeEntry;
    if (odfsVolumeEntry) {
      odfsVolumeEntry.disabled = disableODFS;
    }
  }

  if (!device && !volumes) {
    return currentState;
  }

  const newState = {
    ...currentState,
  };

  if (device) {
    newState.device = device;
  }

  if (volumes) {
    newState.volumes = volumes;
  }

  return newState;
}
