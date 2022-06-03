// Copyright 2016 The Chromium Authors. All rights reserved.
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
 *
 * Created by HTML imports polyfill.
 * @type {!Object}
 */
Window.prototype.HTMLImports;

/**
 * @type {function(function())}
 */
Window.prototype.HTMLImports.whenReady;

/**
 * True if in test: set by the background.js page.
 *
 * @type {boolean}
 */
Window.prototype.IN_TEST;

/**
 * Set true in some unit tests.
 *
 * @type {boolean}
 */
Window.prototype.UNIT_TEST;

/**
 * True if files app is running in a SWA container.
 *
 * @type {boolean}
 */
Window.prototype.isSWA;
