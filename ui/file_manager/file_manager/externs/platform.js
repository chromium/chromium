// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Externs which are common for all chrome packaged apps.

/**
 * @param {string} url
 * @param {function(!Entry)} successCallback
 * @param {function(!FileError)=} opt_errorCallback
 */
Window.prototype.webkitResolveLocalFileSystemURL = function(
    url, successCallback, opt_errorCallback) {};
