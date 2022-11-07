// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @typedef {{
 *   text:string,
 *   callback:(function()|undefined)
 * }}
 */
let FilesToastAction;

/**
 * @constructor
 * @extends {HTMLElement}
 */
function FilesToast() {}

/**
 *  @param {string} text
 *  @param {FilesToastAction} action
 *
 */
FilesToast.prototype.show = function(text, action) {};

FilesToast.prototype.hide = function() {};

/** @type {boolean} */
FilesToast.prototype.visible;
