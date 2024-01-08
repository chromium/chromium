// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assertDeepEquals} from 'chrome://webui-test/chromeos/chai_assert.js';

import {DialogType, type LaunchParams} from '../../state/state.js';
import {getEmptyState, getStore, type Store} from '../store.js';

import {setLaunchParameters} from './launch_params.js';

let store: Store;

export function setUp() {
  store = getStore();
  store.init(getEmptyState());
}

export function testUpdateLaunchParameters() {
  const firstState = store.getState().launchParams;
  const want: LaunchParams = {
    dialogType: undefined,
  };
  assertDeepEquals(
      want, firstState,
      `1. ${JSON.stringify(want)} !== ${JSON.stringify(firstState)}`);
  // Update dialogType
  store.dispatch(setLaunchParameters({
    dialogType: DialogType.FULL_PAGE,
  }));
  const secondState = store.getState().launchParams;
  want.dialogType = DialogType.FULL_PAGE;
  assertDeepEquals(
      want, secondState,
      `1. ${JSON.stringify(want)} !== ${JSON.stringify(secondState)}`);
}
