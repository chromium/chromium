// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {VolumeManager} from '../volume_manager.js';

/** @typedef {function(!Array<string>):!Promise} */
export let LaunchHandler;

/**
 * Interface exposed in window.background in the background page. Used for
 * Audio and Video.
 *
 * Files app uses a larger interface: `FileBrowserBackgroundFull`.
 * Interface exposed in window.background in the background page. Used for
 * Audio and Video.
 *
 * Files app uses a larger interface: `FileBrowserBackgroundFull`.
 *
 * @interface
 */
export class BackgroundBase {
  constructor() {
    /** @type {!Object<!Window>} */
    this.dialogs;
  }

  /**
   * Set a handler which is called when an app is launched.
   * @param {!LaunchHandler} handler Function to be called.
   */
  setLaunchHandler(handler) {}

  /** @return {!Promise<!VolumeManager>} */
  getVolumeManager() {}
}
