// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {util} from '../../common/js/util.js';
import {State, Volume} from '../../externs/ts/state.js';
import {addReducer, BaseAction, Reducer, ReducersMap} from '../../lib/base_store.js';
import {Action, ActionType} from '../actions.js';

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
    if (!volumes) {
      volumes = {
        ...currentState.volumes,
      };
    }
    volumes[volume.volumeId] = {
      ...volumes[volume.volumeId],
      isDisabled: disableODFS,
    };
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
