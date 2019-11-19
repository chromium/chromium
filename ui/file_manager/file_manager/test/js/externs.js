// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview External objects and functions required for compiling.
 */

/** @param {...*} var_args */
ChromeEvent.prototype.dispatchEvent = (var_args) => {};

/** @type {string} */
let FILE_MANAGER_ROOT;

/** @type {!FileManagerTestDeps} */
let fileManager;

/**
 * Declare as FileBrowserBackground rather than FileBrowserBackgroundFull to
 * simplify deps.  Individual fields such as progressCenter below can be
 * defined as needed.
 * @type {!FileBrowserBackground}}
 */
fileManager.fileBrowserBackground_;

/**
 * Declare ProgressCenterPanel here rather than deal with its deps.  It is
 * referenced in //ui/file_manager/externs/background/progress_center.js
 * @interface
 */
class ProgressCenterPanel {}

/** @type {!ProgressCenter} */
fileManager.fileBrowserBackground_.progressCenter;

/** @type {!FileBrowserBackground} */
window.background;
