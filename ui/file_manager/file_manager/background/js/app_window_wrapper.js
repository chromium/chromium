// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {openWindow} from '../../common/js/api.js';
import {AsyncQueue} from '../../common/js/async_util.js';
import {FilesAppState} from '../../common/js/files_app_state.js';

/** Coordinates the creation of new windows for Files app.  */
export class AppWindowWrapper {
  /**
   * @param {string} url App window content url.
   */
  constructor(url) {
    this.url_ = url;
    /** @private {?FilesAppState} */
    this.appState_ = null;
    this.openingOrOpened_ = false;

    /** @protected {!AsyncQueue} */
    this.queue_ = new AsyncQueue();
  }

  /**
   * Gets the launch lock, used to synchronize the asynchronous initialization
   * steps.
   *
   * @return {Promise<function()>} A function to be called to release the lock.
   */
  async getLaunchLock() {
    return this.queue_.lock();
  }

  /**
   * Opens the window.
   *
   * @param {!FilesAppState} appState App state.
   * @param {boolean} reopen True if the launching is triggered automatically.
   *     False otherwise.
   * @return {Promise} Resolved when the window is launched.
   */
  async launch(appState, reopen) {
    // Check if the window is opened or not.
    if (this.openingOrOpened_) {
      console.warn('The window is already opened.');
      return Promise.resolve();
    }
    this.openingOrOpened_ = true;

    // Save application state.
    this.appState_ = appState;

    return this.launch_();
  }

  /**
   * Opens a new window for the SWA.
   *
   * @return {Promise} Resolved when the window is launched.
   * @private
   */
  async launch_() {
    const unlock = await this.getLaunchLock();
    try {
      await this.createWindow_();
    } catch (error) {
      console.error(error);
    } finally {
      unlock();
    }
  }

  /**
   * @return {Promise} Resolved when the new window is opened.
   * @private
   */
  async createWindow_() {
    const url = this.appState_.currentDirectoryURL || '';
    const result = await openWindow({
      currentDirectoryURL: url,
      selectionURL: this.appState_.selectionURL,
    });

    if (!result) {
      throw new Error(`Failed to create window for ${url}`);
    }
  }
}
