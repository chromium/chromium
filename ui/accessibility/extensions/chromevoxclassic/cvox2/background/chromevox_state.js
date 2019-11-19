// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface for querying and modifying the global
 *     ChromeVox state, to avoid direct dependencies on the Background
 *     object and to facilitate mocking for tests.
 */

goog.provide('ChromeVoxMode');
goog.provide('ChromeVoxState');

goog.require('cursors.Cursor');

/**
 * All possible modes ChromeVox can run.
 * @enum {string}
 */
ChromeVoxMode = {
  CLASSIC: 'classic',
  COMPAT: 'compat',
  NEXT: 'next',
  FORCE_NEXT: 'force_next'
};

/**
 * ChromeVox2 state object.
 * @constructor
 */
ChromeVoxState = function() {
  if (ChromeVoxState.instance)
    throw 'Trying to create two instances of singleton ChromeVoxState.';
  ChromeVoxState.instance = this;
};

/**
 * @type {ChromeVoxState}
 */
ChromeVoxState.instance;

/**
 * Holds the un-composite tts object.
 * @type {Object}
 */
ChromeVoxState.backgroundTts;

/**
 * @type {boolean}
 */
ChromeVoxState.isReadingContinuously;

ChromeVoxState.prototype = {
  /** @type {ChromeVoxMode} */
  get mode() {
    return this.getMode();
  },

  /**
   * @return {ChromeVoxMode} The current mode.
   * @protected
   */
  getMode: function() {
    return ChromeVoxMode.NEXT;
  },

  /** @type {cursors.Range} */
  get currentRange() {
    return this.getCurrentRange();
  },

  /**
   * @return {cursors.Range} The current range.
   * @protected
   */
  getCurrentRange: function() {
    return null;
  },

  /**
   * @param {cursors.Range} newRange The new range.
   */
  setCurrentRange: goog.abstractMethod,
};
