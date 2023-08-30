// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeEntry} from '../../common/js/files_app_entry_types.js';
import {util} from '../../common/js/util.js';
import {State, Volume} from '../../externs/ts/state.js';
import {addReducer, BaseAction, Reducer, ReducersMap} from '../../lib/base_store.js';
import {Action, ActionType} from '../actions.js';
import {updateFileData} from '../ducks/all_entries.js';
import {getEntry} from '../store.js';

import {updateVolume} from './volumes.js';

/** Map of actions to reducers for the device slice. */
export const deviceReducersMap: ReducersMap<State, Action> = new Map();

/** Action to update the device connection state in the store. */
export interface UpdateDeviceConnectionStateAction extends BaseAction {
  type: ActionType.UPDATE_DEVICE_CONNECTION_STATE;
  payload: {
    connection: chrome.fileManagerPrivate.DeviceConnectionState,
  };
}

function updateDeviceConnectionStateReducer(
    currentState: State,
    payload: UpdateDeviceConnectionStateAction['payload']): State {
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

export const updateDeviceConnectionState = addReducer(
    ActionType.UPDATE_DEVICE_CONNECTION_STATE,
    updateDeviceConnectionStateReducer as Reducer<State, Action>,
    deviceReducersMap);
