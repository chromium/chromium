// Copyright 2014 The Chromium Authors. All rights reserved.
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

/**
 * Media error: MEDIA_ERR_ABORTED.
 * @type {number}
 * @const
 * @see http://dev.w3.org/html5/spec-author-view/video.html#mediaerror
 */
MediaError.MEDIA_ERR_ABORTED = 1;

/**
 * Media error: MEDIA_ERR_NETWORK.
 * @type {number}
 * @const
 * @see http://dev.w3.org/html5/spec-author-view/video.html#mediaerror
 */
MediaError.MEDIA_ERR_NETWORK = 2;

/**
 * Media error: MEDIA_ERR_DECODE.
 * @type {number}
 * @const
 * @see http://dev.w3.org/html5/spec-author-view/video.html#mediaerror
 */
MediaError.MEDIA_ERR_DECODE = 3;

/**
 * Media error: MEDIA_ERR_SRC_NOT_SUPPORTED.
 * @type {number}
 * @const
 * @see http://dev.w3.org/html5/spec-author-view/video.html#mediaerror
 */
MediaError.MEDIA_ERR_SRC_NOT_SUPPORTED = 4;
