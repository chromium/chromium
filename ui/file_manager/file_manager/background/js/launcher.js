// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {FilesAppState} from '../../common/js/files_app_state.js';
import {util} from '../../common/js/util.js';

import {AppWindowWrapper} from './app_window_wrapper.js';

/**
 * Namespace
 * @type {!Object}
 */
const launcher = {};

/**
 * Prefix for the file manager window ID.
 * @const {string}
 */
const FILES_ID_PREFIX = 'files#';

/**
 * Regexp matching a file manager window ID.
 * @const {!RegExp}
 */
export const FILES_ID_PATTERN = new RegExp('^' + FILES_ID_PREFIX + '(\\d*)$');

/**
 * Promise to serialize asynchronous calls.
 * @type {?Promise}
 */
launcher.initializationPromise_ = null;

launcher.setInitializationPromise = (promise) => {
  launcher.initializationPromise_ = promise;
};


/**
 * @param {!FilesAppState=} appState App state.
 * @param {number=} id Window id.
 * @return {!Promise<void>} Resolved when the window is launched.
 */
launcher.launchFileManager = async (appState = undefined, id = undefined) => {
  // Serialize concurrent calls to launchFileManager.
  if (!launcher.initializationPromise_) {
    throw new Error('Missing launcher.initializationPromise');
  }

  await launcher.initializationPromise_;

  const appWindow = new AppWindowWrapper('main.html');

  await appWindow.launch(appState || {}, false);
};

export {launcher};
