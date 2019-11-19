// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {!Object}
 */
var launcher = {};

/**
 * Type of a Files app's instance launch.
 * @enum {number}
 */
const LaunchType = {
  ALWAYS_CREATE: 0,
  FOCUS_ANY_OR_CREATE: 1,
  FOCUS_SAME_OR_CREATE: 2
};

/**
 * Prefix for the file manager window ID.
 * @const {string}
 */
const FILES_ID_PREFIX = 'files#';

/**
 * Value of the next file manager window ID.
 * @type {number}
 */
let nextFileManagerWindowID = 0;

/**
 * File manager window create options.
 * @const {!Object}
 */
const FILE_MANAGER_WINDOW_CREATE_OPTIONS = {
  bounds: {
    left: Math.round(window.screen.availWidth * 0.1),
    top: Math.round(window.screen.availHeight * 0.1),
    // We choose 1000px as default window width to fit 4 columns in grid view,
    // as long as the width doesn't exceed 80% of the screen width.
    width: Math.min(Math.round(window.screen.availWidth * 0.8), 1000),
    height: Math.min(Math.round(window.screen.availHeight * 0.8), 600)
  },
  frame: {color: '#254fae'},
  minWidth: 480,
  minHeight: 300
};

/**
 * Regexp matching a file manager window ID.
 * @const {!RegExp}
 */
const FILES_ID_PATTERN = new RegExp('^' + FILES_ID_PREFIX + '(\\d*)$');

/**
 * Synchronous queue for asynchronous calls.
 * @type {!AsyncUtil.Queue}
 */
launcher.queue = new AsyncUtil.Queue();

/**
 * @param {Object=} opt_appState App state.
 * @param {number=} opt_id Window id.
 * @param {LaunchType=} opt_type Launch type. Default: ALWAYS_CREATE.
 * @param {function(string)=} opt_callback Completion callback with the App ID.
 */
launcher.launchFileManager = (opt_appState, opt_id, opt_type, opt_callback) => {
  const type = opt_type || LaunchType.ALWAYS_CREATE;
  opt_appState =
      /**
       * @type {(undefined|
       *         {currentDirectoryURL: (string|undefined),
       *          selectionURL: (string|undefined)})}
       */
      (opt_appState);

  // Wait until all windows are created.
  launcher.queue.run(onTaskCompleted => {
    // Check if there is already a window with the same URL. If so, then
    // reuse it instead of opening a new one.
    if (type == LaunchType.FOCUS_SAME_OR_CREATE ||
        type == LaunchType.FOCUS_ANY_OR_CREATE) {
      if (opt_appState) {
        for (var key in window.appWindows) {
          if (!key.match(FILES_ID_PATTERN)) {
            continue;
          }
          const contentWindow = window.appWindows[key].contentWindow;
          if (!contentWindow.appState) {
            continue;
          }
          // Different current directories.
          if (opt_appState.currentDirectoryURL !==
              contentWindow.appState.currentDirectoryURL) {
            continue;
          }
          // Selection URL specified, and it is different.
          if (opt_appState.selectionURL &&
              opt_appState.selectionURL !==
                  contentWindow.appState.selectionURL) {
            continue;
          }
          window.appWindows[key].focus();
          if (opt_callback) {
            opt_callback(key);
          }
          onTaskCompleted();
          return;
        }
      }
    }

    // Focus any window if none is focused. Try restored first.
    if (type == LaunchType.FOCUS_ANY_OR_CREATE) {
      // If there is already a focused window, then finish.
      for (var key in window.appWindows) {
        if (!key.match(FILES_ID_PATTERN)) {
          continue;
        }

        // The isFocused() method should always be available, but in case
        // the Files app's failed on some error, wrap it with try catch.
        try {
          if (window.appWindows[key].contentWindow.isFocused()) {
            if (opt_callback) {
              opt_callback(key);
            }
            onTaskCompleted();
            return;
          }
        } catch (e) {
          console.error(e.message);
        }
      }
      // Try to focus the first non-minimized window.
      for (var key in window.appWindows) {
        if (!key.match(FILES_ID_PATTERN)) {
          continue;
        }

        if (!window.appWindows[key].isMinimized()) {
          window.appWindows[key].focus();
          if (opt_callback) {
            opt_callback(key);
          }
          onTaskCompleted();
          return;
        }
      }
      // Restore and focus any window.
      for (var key in window.appWindows) {
        if (!key.match(FILES_ID_PATTERN)) {
          continue;
        }

        window.appWindows[key].focus();
        if (opt_callback) {
          opt_callback(key);
        }
        onTaskCompleted();
        return;
      }
    }

    // Create a new instance in case of ALWAYS_CREATE type, or as a fallback
    // for other types.

    const id = opt_id || nextFileManagerWindowID;
    nextFileManagerWindowID = Math.max(nextFileManagerWindowID, id + 1);
    const appId = FILES_ID_PREFIX + id;

    const appWindow = new AppWindowWrapper(
        'main.html', appId, FILE_MANAGER_WINDOW_CREATE_OPTIONS);
    appWindow.launch(opt_appState || {}, false, () => {
      appWindow.rawAppWindow.focus();
      if (opt_callback) {
        opt_callback(appId);
      }
      onTaskCompleted();
    });
  });
};
