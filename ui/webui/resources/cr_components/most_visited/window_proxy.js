// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Abstracts built-in JS functions in order to mock in tests.
 */
export class MostVisitedWindowProxy {
  /**
   * @param {string} query
   * @return {!MediaQueryList}
   */
  matchMedia(query) {
    return window.matchMedia(query);
  }

  /** @return {number} */
  now() {
    return Date.now();
  }

  /** @return {!MostVisitedWindowProxy} */
  static getInstance() {
    return instance || (instance = new MostVisitedWindowProxy());
  }

  /** @param {!MostVisitedWindowProxy} obj */
  static setInstance(obj) {
    instance = obj;
  }
}

/** @type {?MostVisitedWindowProxy} */
let instance = null;
