// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Reducer for bulk pinning.
 *
 * This file is checked via TS, so we suppress Closure checks.
 * @suppress {checkTypes}
 */

import {State} from '../../externs/ts/state.js';
import {UpdateBulkPinProgressAction} from '../actions/bulk_pinning.js';

export function updateBulkPinning(
    currentState: State, action: UpdateBulkPinProgressAction): State {
  const bulkPinning = action.payload;

  return {
    ...currentState,
    bulkPinning,
  };
}
