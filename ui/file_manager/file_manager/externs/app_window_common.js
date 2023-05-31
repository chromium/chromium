// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This definition is required by
 * ui/file_manager/file_manager/common/js/app_util.js.
 * @type {string}
 */
Window.prototype.appID;

/**
 * @type {string}
 */
Window.prototype.appInitialURL;

/**
 * @type {function()}
 */
Window.prototype.reload = function() {};

/**
 * True if in test: set by ash/webui/file_manager/resources/init_globals.js.
 *
 * @type {boolean}
 */
Window.prototype.IN_TEST;

/**
 * The function below is added by tslib_shim.ts to the global namespace.
 *
 * @param {*} decorators
 * @param {*} target
 * @param {string=} key
 * @param {*=} desc
 */
const __decorate = function(decorators, target, key, desc) {};

/**
 * Set this to true to log action data in the console for debugging purpose.
 * @type {boolean}
 */
Window.prototype.DEBUG_STORE;
