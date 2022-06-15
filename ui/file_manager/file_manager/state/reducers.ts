// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {BaseAction} from '../lib/base_store.js';

import {State} from './state.js';

/** TODO(lucmult): Document this. */
export function rootReducer(currentState: State, action: BaseAction): State {
  if (action.type) {
    return Object.assign(currentState, {});
  }

  console.error('invalid action');
  return currentState;
}
