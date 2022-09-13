// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './app_windows.js';

import {openWindow} from '../../common/js/api.js';
import {AsyncUtil} from '../../common/js/async_util.js';
import {FilesAppState} from '../../common/js/files_app_state.js';

/**
 * Wrapper for an app window.
 *
 * Expects the following from the app scripts:
 * 1. The page load handler should initialize the app using |window.appState|
 *    and call |appUtil.saveAppState|.
 * 2. Every time the app state changes the app should update |window.appState|
 *    and call |appUtil.saveAppState| .
 * 3. The app may have |unload| function to persist the app state that does not
 *    fit into |window.appState|.
 */
export class AppWindowWrapper {
  /**
   * @param {string} url App window content url.
   * @param {string} id App window id.
   * @param {Object} options Options object to create it.
   */
  constructor(url, id, options) {
    this.url_ = url;
    this.id_ = id;
    // Do deep copy for the template of options to assign customized params
    // later.
    this.options_ = /** @type {!chrome.app.window.CreateWindowOptions} */ (
        JSON.parse(JSON.stringify(options)));
    this.window_ = null;
    /** @private {?FilesAppState} */
    this.appState_ = null;
    this.openingOrOpened_ = false;

    /** @protected {AsyncUtil.Queue} */
    this.queue_ = new AsyncUtil.Queue();
  }

  /**
   * @return {chrome.app.window.AppWindow} Wrapped application window.
   */
  get rawAppWindow() {
    return this.window_;
  }

  /**
   * Sets the icon of the window.
   * @param {string} iconPath Path of the icon.
   */
  setIcon(iconPath) {
    this.window_.setIcon(iconPath);
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

    return this.launchSWA_();
  }

  /**
   * Opens a new window for the SWA.
   *
   * @return {Promise} Resolved when the window is launched.
   * @private
   */
  async launchSWA_() {
    const unlock = await this.getLaunchLock();
    try {
      await this.createWindowSWA_();
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
  async createWindowSWA_() {
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

/**
 * Key for getting and storing the last window state (maximized or not).
 * @const
 * @private
 */
AppWindowWrapper.MAXIMIZED_KEY_ = 'isMaximized';

/**
 * Make a key of window geometry preferences for the given initial URL.
 * @param {string} url Initialize URL that the window has.
 * @return {string} Key of window geometry preferences.
 */
AppWindowWrapper.makeGeometryKey = url => {
  return 'windowGeometry:' + url;
};

/**
 * Shift distance to avoid overlapping windows.
 * @type {number}
 * @const
 */
AppWindowWrapper.SHIFT_DISTANCE = 40;
