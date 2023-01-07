// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Picture-in-picture APIs.
 * @see https://wicg.github.io/picture-in-picture/
 * @externs
 */

/**
 * @interface
 * @extends {EventTarget}
 * @see https://wicg.github.io/picture-in-picture/#interface-picture-in-picture-window
 */
function PictureInPictureWindow() {}

/** @type {number} */
PictureInPictureWindow.prototype.width;

/** @type {number} */
PictureInPictureWindow.prototype.length;

/** @type {?function(!Event)} */
PictureInPictureWindow.prototype.onresize;

/**
 * @see https://wicg.github.io/picture-in-picture/#htmlvideoelement-extensions
 * @return {!Promise<!PictureInPictureWindow>}
 */
HTMLVideoElement.prototype.requestPictureInPicture = function() {};

/**
 * @type {?function(!Event)}
 * @see https://wicg.github.io/picture-in-picture/#htmlvideoelement-extensions
 */
HTMLVideoElement.prototype.onenterpictureinpicture;

/**
 * @type {?function(!Event)}
 * @see https://wicg.github.io/picture-in-picture/#htmlvideoelement-extensions
 */
HTMLVideoElement.prototype.onleavepictureinpicture;

/**
 * @type {boolean}
 * @see https://wicg.github.io/picture-in-picture/#htmlvideoelement-extensions
 */
HTMLVideoElement.prototype.autoPictureInPicture;

/**
 * @type {boolean}
 * @see https://wicg.github.io/picture-in-picture/#htmlvideoelement-extensions
 */
HTMLVideoElement.prototype.disablePictureInPicture;

/**
 * @type {boolean}
 * @see https://wicg.github.io/picture-in-picture/#document-extensions
 */
Document.prototype.pictureInPictureEnabled;

/**
 * @see https://wicg.github.io/picture-in-picture/#document-extensions
 * @return {!Promise<void>}
 */
Document.prototype.exitPictureInPicture = function() {};

/**
 * @type {?HTMLVideoElement}
 * @see https://wicg.github.io/picture-in-picture/#documentorshadowroot-extension
 */
Document.prototype.pictureInPictureElement;