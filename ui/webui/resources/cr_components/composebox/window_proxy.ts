// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Abstracts built-in JS functions in order to mock in tests.
 */
let instance: WindowProxy|null = null;

export class WindowProxy {
  static getInstance(): WindowProxy {
    return instance || (instance = new WindowProxy());
  }

  static setInstance(obj: WindowProxy) {
    instance = obj;
  }

  setTimeout(callback: () => void, duration: number): number {
    return window.setTimeout(callback, duration);
  }

  clearTimeout(id: number|null) {
    window.clearTimeout(id !== null ? id : undefined);
  }
}
