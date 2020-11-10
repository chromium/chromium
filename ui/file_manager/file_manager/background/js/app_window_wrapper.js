// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
// #import './app_windows.m.js';
// #import * as wrappedAsyncUtil from '../../common/js/async_util.m.js'; const {AsyncUtil} = wrappedAsyncUtil;
// #import * as wrappedAppUtil from '../../../base/js/app_util.m.js'; const {appUtil} = wrappedAppUtil;
// #import {assertInstanceof} from 'chrome://resources/js/assert.m.js';
// clang-format on

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
/* #export */ class AppWindowWrapper {
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
   * @param {Array<chrome.app.window.AppWindow>} similarWindows
   * @return {!Promise}
   */
  async restoreMaximizedWindow_(similarWindows) {
    return new Promise(resolve => {
      for (let index = 0; index < similarWindows.length; index++) {
        if (similarWindows[index].isMaximized()) {
          const listener = () => {
            similarWindows[index].onRestored.removeListener(listener);
            resolve();
          };
          similarWindows[index].onRestored.addListener(listener);

          // TODO(lucmult): Isn't restore function below synchronous? If so we
          // don't need this event listener.
          similarWindows[index].restore();
          return;
        }
      }

      // If no maximized windows, then create the window immediately.
      resolve();
    });
  }

  /**
   * @return {!Promise}
   */
  async getWindowPreferences_() {
    return new Promise(resolve => {
      const boundsKey = AppWindowWrapper.makeGeometryKey(this.url_);
      const maximizedKey = AppWindowWrapper.MAXIMIZED_KEY_;

      let lastBounds;
      let isMaximized = false;
      chrome.storage.local.get([boundsKey, maximizedKey], preferences => {
        if (!chrome.runtime.lastError) {
          lastBounds = preferences[boundsKey];
          isMaximized = preferences[maximizedKey];
        }
        resolve({lastBounds: lastBounds, isMaximized: isMaximized});
      });
    });
  }

  /**
   * @return {!Promise<?chrome.app.window.AppWindow>}
   */
  async createWindow_(reopen) {
    return await new Promise((resolve, reject) => {
      // Create a window.
      chrome.app.window.create(this.url_, this.options_, appWindow => {
        this.window_ = appWindow;
        if (!appWindow) {
          reject(`Failed to create window for ${this.url_}`);
          return;
        }

        // Save the properties.
        window.appWindows[this.id_] = appWindow;
        const contentWindow = appWindow.contentWindow;
        contentWindow.appID = this.id_;
        contentWindow.appState = this.appState_;
        contentWindow.appReopen = reopen;
        contentWindow.appInitialURL = this.url_;
        if (window.IN_TEST) {
          contentWindow.IN_TEST = true;
        }

        resolve(appWindow);
      });
    });
  }

  positionWindow_(appWindow, similarWindows) {
    // If there is another window in the same position, shift the window.
    const makeBoundsKey = bounds => {
      return bounds.left + '/' + bounds.top;
    };

    const notAvailablePositions = {};
    for (let i = 0; i < similarWindows.length; i++) {
      const key = makeBoundsKey(similarWindows[i].getBounds());
      notAvailablePositions[key] = true;
    }

    const candidateBounds = this.window_.getBounds();
    while (true) {
      const key = makeBoundsKey(candidateBounds);
      if (!notAvailablePositions[key]) {
        break;
      }

      // Make the position available to avoid an infinite loop.
      notAvailablePositions[key] = false;
      const nextLeft = candidateBounds.left + AppWindowWrapper.SHIFT_DISTANCE;
      const nextRight = nextLeft + candidateBounds.width;
      candidateBounds.left = nextRight >= screen.availWidth ?
          nextRight % screen.availWidth :
          nextLeft;
      const nextTop = candidateBounds.top + AppWindowWrapper.SHIFT_DISTANCE;
      const nextBottom = nextTop + candidateBounds.height;
      candidateBounds.top = nextBottom >= screen.availHeight ?
          nextBottom % screen.availHeight :
          nextTop;
    }

    appWindow.moveTo(
        /** @type {number} */ (candidateBounds.left),
        /** @type {number} */ (candidateBounds.top));
  }

  /**
   * Opens the window.
   *
   * @param {Object} appState App state.
   * @param {boolean} reopen True if the launching is triggered automatically.
   *     False otherwise.
   * @return {Promise} Resolved when the window is launched.
   */
  async launch(appState, reopen) {
    // Check if the window is opened or not.
    if (this.openingOrOpened_) {
      console.error('The window is already opened.');
      return Promise.resolve();
    }
    this.openingOrOpened_ = true;

    // Save application state.
    this.appState_ = appState;

    // Get similar windows, it means with the same initial url, eg. different
    // main windows of the Files app.
    const similarWindows = window.getSimilarWindows(this.url_);

    const unlock = await this.getLaunchLock();
    try {
      // Restore maximized windows, to avoid hiding them to tray, which can be
      // confusing for users.
      await this.restoreMaximizedWindow_(similarWindows);

      // Obtains the last geometry and window state (maximized or not).
      const prefs = await this.getWindowPreferences_();
      if (prefs.isMaximized !== undefined) {
        this.options_.state = prefs.isMaximized ? 'maximized' : undefined;
      }
      if (prefs.lastBounds) {
        this.options_.bounds = prefs.lastBounds;
      }

      // Closure creating the window, once all preprocessing tasks are finished.
      const appWindow = await this.createWindow_(reopen);

      // Exit full screen state if it's created as a full screen window.
      if (appWindow.isFullscreen()) {
        appWindow.restore();
      }

      // This is a temporary workaround for crbug.com/452737.
      // {state: 'maximized'} in CreateWindowOptions is ignored when a window is
      // launched with hidden option, so we maximize the window manually here.
      if (this.options_.hidden && this.options_.state === 'maximized') {
        appWindow.maximize();
      }

      this.positionWindow_(appWindow, similarWindows);

      // Register event listeners.
      appWindow.onBoundsChanged.addListener(this.onBoundsChanged_.bind(this));
      appWindow.onClosed.addListener(this.onClosed_.bind(this));
    } catch (error) {
      console.error(error);
    } finally {
      unlock();
    }
  }

  /**
   * Handles the onClosed extension API event.
   * @private
   */
  onClosed_() {
    // Remember the last window state (maximized or normal).
    const preferences = {};
    preferences[AppWindowWrapper.MAXIMIZED_KEY_] = this.window_.isMaximized();
    chrome.storage.local.set(preferences);

    // Unload the window.
    const appWindow = this.window_;
    const contentWindow = this.window_.contentWindow;
    if (contentWindow.unload) {
      contentWindow.unload();
    }
    this.window_ = null;
    this.openingOrOpened_ = false;

    // Updates preferences.
    if (contentWindow.saveOnExit) {
      contentWindow.saveOnExit.forEach(entry => {
        appUtil.AppCache.update(entry.key, entry.value);
      });
    }
    chrome.storage.local.remove(this.id_);  // Forget the persisted state.

    // Remove the window from the set.
    delete window.appWindows[this.id_];
  }

  /**
   * Handles onBoundsChanged extension API event.
   * @private
   */
  onBoundsChanged_() {
    if (!this.window_.isMaximized()) {
      const preferences = {};
      preferences[AppWindowWrapper.makeGeometryKey(this.url_)] =
          this.window_.getBounds();
      chrome.storage.local.set(preferences);
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

/**
 * Wrapper for a singleton app window.
 *
 * In addition to the AppWindowWrapper requirements the app scripts should
 * have |reload| method that re-initializes the app based on a changed
 * |window.appState|.
 */
/* #export */ class SingletonAppWindowWrapper extends AppWindowWrapper {
  /**
   * @param {string} url App window content url.
   * @param {Object|function()} options Options object or a function to return
   *     it.
   */
  constructor(url, options) {
    super(url, url, options);
  }

  /**
   * Open the window.
   *
   * Activates an existing window or creates a new one.
   *
   * @param {Object} appState App state.
   * @param {boolean} reopen True if the launching is triggered automatically.
   *     False otherwise.
   * @return {Promise} Resolved when the window is launched.
   */
  async launch(appState, reopen) {
    // If the window is not opened yet, just call the parent method.
    if (!this.openingOrOpened_) {
      return super.launch(appState, reopen);
    }

    // The lock is used to wait until the window is opened and set in
    // this.window_.
    const unlock = await this.getLaunchLock();

    try {
      // If the window is already opened, reload the window.
      this.window_.contentWindow.appState = appState;
      this.window_.contentWindow.appReopen = reopen;
      if (!this.window_.contentWindow.reload) {
        // Currently the queue is not made to wait for window loading after
        // creating window. Therefore contentWindow might not have the reload()
        // function ready yet. This happens when launching the same app twice
        // quickly. See crbug.com/789226.
        console.error('Window reload requested before loaded. Skiping.');
      } else {
        this.window_.contentWindow.reload();
      }
    } finally {
      unlock();
    }
  }

  /**
   * Reopen a window if its state is saved in the local storage.
   * @param {function()=} opt_callback Completion callback.
   */
  reopen(opt_callback) {
    chrome.storage.local.get(this.id_, items => {
      const value = items[this.id_];
      if (!value) {
        opt_callback && opt_callback();
        return;  // No app state persisted.
      }

      let appState;
      try {
        appState = assertInstanceof(JSON.parse(value), Object);
      } catch (e) {
        console.error('Corrupt launch data for ' + this.id_, value);
        opt_callback && opt_callback();
        return;
      }
      this.launch(appState, true).then(() => opt_callback && opt_callback());
    });
  }
}
