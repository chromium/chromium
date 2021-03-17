// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {?{
 *   position: (number|undefined),
 *   time: (number|undefined),
 *   expanded: (boolean|undefined),
 *   items: (!Array<string>|undefined)
 * }}
 */
let Playlist;

/**
 * @type {(Playlist|Object|undefined)}
 */
Window.prototype.appState;

/**
 * @type {(boolean|undefined)}
 */
Window.prototype.appReopen;
