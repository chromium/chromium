// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Abstracts built-in JS functions in order to mock in tests.
 */
export class MostVisitedWindowProxy {
  matchMedia(query: string): MediaQueryList {
    return window.matchMedia(query);
  }

  /**
   * Returns milliseconds elapsed since navigation start (`performance.now()`).
   */
  now(): number {
    return performance.now();
  }

  static getInstance(): MostVisitedWindowProxy {
    return instance || (instance = new MostVisitedWindowProxy());
  }

  static setInstance(obj: MostVisitedWindowProxy) {
    instance = obj;
  }
}

let instance: MostVisitedWindowProxy|null = null;
