// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * BackgroundComponents.
 */
class BackgroundComponents {
  constructor() {
    /**
     * @type {!Object<string>}
     */
    this.stringData;

    /**
     * @type {!VolumeManager}
     */
    this.volumeManager;
  }
}

/**
 * A global flag which indicates whether it is in tests or not. This is set in
 * gallery/js/background.js.
 *
 * @type {boolean}
 */
Window.prototype.IN_TEST;

/**
 * @type {string}
 */
Entry.prototype.cachedUrl;
