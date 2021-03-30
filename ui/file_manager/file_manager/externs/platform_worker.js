// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Externs which are common for all worker of chrome packaged apps.

/**
 * @param {string} url
 * @param {function(!Entry)} successCallback
 * @param {function(!FileError)=} opt_errorCallback
 */
function webkitResolveLocalFileSystemURL(
    url, successCallback, opt_errorCallback) {}
