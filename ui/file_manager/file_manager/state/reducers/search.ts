// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from '../../externs/ts/state.js';
import {SearchAction} from '../actions.js';

export function search(state: State, action: SearchAction): State {
  const search = {
    query: action.payload.query,
    status: action.payload.status,
  };
  return {...state, search};
}
