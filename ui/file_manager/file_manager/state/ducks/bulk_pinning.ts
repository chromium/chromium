// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {Slice} from '../../lib/base_store.js';
import type {State} from '../../state/state.js';

/**
 * @fileoverview Bulk pinning slice of the store.
 *
 * BulkPinProgress is the current state of files that are being pinned when the
 * BulkPinning feature is enabled. During bulk pinning, all the users items in
 * My drive are pinned and kept available offline. This tracks the progress of
 * both the initial operation and any subsequent updates along with any error
 * states that may occur.
 */

const slice = new Slice<State, State['bulkPinning']>('bulkPinning');
export {slice as bulkPinningSlice};

/** Create action to update the bulk pin progress. */
export const updateBulkPinProgress = slice.addReducer(
    'set-progress',
    (state, bulkPinning: chrome.fileManagerPrivate.BulkPinProgress) => ({
      ...state,
      bulkPinning,
    }));
