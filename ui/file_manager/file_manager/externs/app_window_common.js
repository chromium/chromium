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