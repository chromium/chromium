/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides an interface that defines how users can extend the
 * `goog.labs.mock` mocking framework with a TimeoutMode. This is used
 * with waitAndVerify to specify a max timeout.
 *
 * In addition it exports a factory method that allows users to easily obtain
 * a TimeoutMode instance.
 */

goog.provide('goog.labs.mock.timeout');
goog.provide('goog.labs.mock.timeout.TimeoutMode');

/**
 * Used to specify max timeout on waitAndVerify
 * @const
 */
goog.labs.mock.timeout.TimeoutMode = class TimeoutMode {
  /**
   * @param {number} duration
   */
  constructor(duration) {
    /**
     * @type {number} duration
     * @public
     */
    this.duration = duration;
  }
};

/**
 * @param {number} duration
 * @return {!goog.labs.mock.timeout.TimeoutMode}
 */
goog.labs.mock.timeout.timeout = function(duration) {
  'use strict';
  return new goog.labs.mock.timeout.TimeoutMode(duration);
};
