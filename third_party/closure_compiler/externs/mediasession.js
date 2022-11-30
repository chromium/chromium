// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The current spec of the Media Session API.
 * @see https://wicg.github.io/mediasession/
 * @externs
 */

/**
 * @see https://wicg.github.io/mediasession/#the-mediaimage-dictionary
 * @record
 * @struct
 */
function MediaImage() {}

/** @type {string} */
MediaImage.prototype.src;

/** @type {(string|undefined)} */
MediaImage.prototype.sizes;

/** @type {(string|undefined)} */
MediaImage.prototype.type;

/**
 * A MediaMetadata object is a representation of the metadata associated with a
 * MediaSession that can be used by user agents to provide customized user
 * interface.
 * @see https://wicg.github.io/mediasession/#the-mediametadata-interface
 * @constructor
 * @param {?MediaMetadataInit} init
 */
function MediaMetadata(init) {}

/** @type {string} */
MediaMetadata.prototype.album;

/** @type {string} */
MediaMetadata.prototype.artist;

/** @type {!Array<!MediaImage>} */
MediaMetadata.prototype.artwork;

/** @type {string} */
MediaMetadata.prototype.title;

/**
 * @see https://wicg.github.io/mediasession/#the-mediametadata-interface
 * @record
 * @struct
 */
function MediaMetadataInit() {}

/** @type {(string|undefined)} */
MediaMetadataInit.prototype.album;

/** @type {(string|undefined)} */
MediaMetadataInit.prototype.artist;

/** @type {(!Array<!MediaImage>|undefined)} */
MediaMetadataInit.prototype.artwork;

/** @type {(string|undefined)} */
MediaMetadataInit.prototype.title;

/**
 * A MediaSession objects represents a media session for a given document and
 * allows a document to communicate to the user agent some information about the
 * playback and how to handle it.
 * @see https://wicg.github.io/mediasession/#the-mediasession-interface
 * @interface
 * @struct
 */
function MediaSession() {}

/** @type {?MediaMetadata} */
MediaSession.prototype.metadata;

/** @type {string} */
MediaSession.prototype.playbackState;

/** @type {function(string, ?function())} */
MediaSession.prototype.setActionHandler;

/** @type {?MediaSession} */
Navigator.prototype.mediaSession;
