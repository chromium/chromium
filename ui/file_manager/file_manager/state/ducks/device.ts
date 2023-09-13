// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {Slice} from '../../lib/base_store.js';

/**
 * @fileoverview Device slice of the store.
 * @suppress {checkTypes}
 */

const slice = new Slice<State, State['device']>('device');
export {slice as deviceSlice};

export const updateDeviceConnectionState = slice.addReducer(
    'set-connection-state', updateDeviceConnectionStateReducer);

function updateDeviceConnectionStateReducer(currentState: State, payload: {
  connection: chrome.fileManagerPrivate.DeviceConnectionState,
}): State {
  let device: State['device']|undefined;

  // Device connection.
  if (payload.connection !== currentState.device.connection) {
    device = {
      ...currentState.device,
      connection: payload.connection,
    };
  }

  return device ? {...currentState, device} : currentState;
}
