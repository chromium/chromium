// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * @suppress {uselessCode} Temporary suppress because of the line exporting.
 */

/**
 * @see https://wicg.github.io/mediasession/#enumdef-mediasessionplaybackstate
 * @enum {string}
 */
const MediaSessionPlaybackState = {
  NONE: 'none',
  PAUSED: 'paused',
  PLAYING: 'playing'
};

// eslint-disable-next-line semi,no-extra-semi
/* #export */ {MediaSessionPlaybackState};
