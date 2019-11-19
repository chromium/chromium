// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Constants used throughout ChromeVox.
 */

goog.provide('constants');

/**
 * Possible directions to perform tree traversals.
 * @enum {string}
 */
constants.Dir = {
  /** Search from left to right. */
  FORWARD: 'forward',

  /** Search from right to left. */
  BACKWARD: 'backward'
};
