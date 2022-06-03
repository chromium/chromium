// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   entries: Array,
 *   tab_id: (number|undefined)
 * }}
 * @see https://developer.chrome.com/extensions/fileBrowserHandler#type-FileHandlerExecuteEventDetails
 */
let FileHandlerExecuteEventDetails;

/**
 * @namespace
 */
chrome.fileBrowserHandler = {};

/**
 * @param {!Object} selectionParams
 * @param {Function} callback
 * @see https://developer.chrome.com/extensions/fileBrowserHandler#method-selectFile
 */
chrome.fileBrowserHandler.selectFile = function(selectionParams, callback) {};

/** @type {!ChromeEvent} */
chrome.fileBrowserHandler.onExecute;
