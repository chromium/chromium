// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Slice} from '../../lib/base_store.js';
import type {LaunchParams, State} from '../../state/state.js';

const slice = new Slice<State, State['launchParams']>('launchParams');
export {slice as launchParamsSlice};

function launchParamsReducer(state: State, launchParams: LaunchParams): State {
  const storedLaunchParams: LaunchParams = state.launchParams || {};
  if (launchParams.dialogType !== storedLaunchParams.dialogType) {
    return {...state, launchParams};
  }
  return state;
}

/**
 * Updates the stored launch parameters in the store based on the supplied data.
 */
export const setLaunchParameters = slice.addReducer('set', launchParamsReducer);
