// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {State} from './state.js';

/**
 * @fileoverview Simplified interfaces of the Store used by Files app. Used to
 * be able to type check the JS files using Closure compiler.
 * See lib/base_store.ts and state/store.ts for the implementation.
 */

export class Store {
  /** @param {!Object} action */
  // @ts-ignore: error TS6133: 'action' is declared but its value is never read.
  dispatch(action) {}

  /**
   *  @param {!StoreObserver} observer
   *  @returns {function():void} the function to unsubscribe.
   */
  // @ts-ignore: error TS6133: 'observer' is declared but its value is never
  // read.
  subscribe(observer) {
    return () => {};
  }

  /** @param {!StoreObserver} observer */
  // @ts-ignore: error TS6133: 'observer' is declared but its value is never
  // read.
  unsubscribe(observer) {}

  // @ts-ignore: error TS2355: A function whose declared type is neither 'void'
  // nor 'any' must return a value.
  /** @return {!State} */
  getState() {}

  /** @param {!State} initialState */
  // @ts-ignore: error TS6133: 'initialState' is declared but its value is never
  // read.
  init(initialState) {}
}

/**
 * Interface marked as `record` so it doesn't have to mark implements
 * explicitly.
 * @record
 */
class StoreObserver {
  /** @param {!State} newState */
  // @ts-ignore: error TS6133: 'newState' is declared but its value is never
  // read.
  onStateChanged(newState) {}
}
