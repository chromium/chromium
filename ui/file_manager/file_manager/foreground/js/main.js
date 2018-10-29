// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Creates the fileManager object. Note the DOM and external scripts are not
 * fully loaded yet.
 * @type {FileManager}
 */
const fileManager = new FileManager();

/**
 * Initialize the core stuff, which doesn't require access to the DOM, or to
 * external scripts.
 */
fileManager.initializeCore();

/**
 * Initializes the File Manager's UI. Called after the DOM, and all external
 * scripts, have been loaded.
 */
function initializeUI() {
  fileManager.initializeUI(document.body).then(() => {
    util.testSendMessage('ready');
    metrics.recordInterval('Load.Total');
    fileManager.tracker.send(metrics.Management.WINDOW_CREATED);
  });
}

/**
 * Final initialization performed after DOM and all scripts have loaded. See
 * also crbug.com/581028 which added a 'defer' attribute to main_scripts.js.
 */
if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', initializeUI);
} else {
  initializeUI();
}

/** Record script load metric: must be the last line. */
metrics.recordInterval('Load.Script');
