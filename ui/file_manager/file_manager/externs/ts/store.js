// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from './state.js';

/**
 * @fileoverview Simplified interfaces of the Store used by Files app. Used to
 * be able to type check the JS files using Closure compiler.
 * See lib/base_store.ts and state/store.ts for the implementation.
 */

/** @record */
export class Store {
  /** @param {!Object} action */
  dispatch(action) {}

  /**
   *  @param {!StoreObserver} observer
   *  @returns {function()} the function to unsubscribe.
   */
  subscribe(observer) {}

  /** @param {!StoreObserver} observer */
  unsubscribe(observer) {}

  /** @return {!State} */
  getState() {}

  /** @param {!State} initialState */
  init(initialState) {}
}

/**
 * Interface marked as `record` so it doesn't have to mark implements
 * explicitly.
 * @record
 */
class StoreObserver {
  /** @param {!State} newState */
  onStateChanged(newState) {}
}
