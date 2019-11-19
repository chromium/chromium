// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Defines a result type interface.
 */

goog.provide('cvox.AbstractResult');

goog.require('cvox.SearchUtil');

/**
 * @constructor
 */
cvox.AbstractResult = function() { };

/**
 * Checks the result if it is an unknown result.
 * @param {Element} result Result to be checked.
 * @return {boolean} Whether or not the element is an unknown result.
 */
cvox.AbstractResult.prototype.isType = function(result) {
  return false;
};

/**
 * Speak a generic search result.
 * @param {Node} result Generic result to be spoken.
 * @return {boolean} Whether or not the result was spoken.
 */
cvox.AbstractResult.prototype.speak = function(result) {
  return false;
};

/**
 * Extracts the wikipedia URL from knowledge panel.
 * @param {Node} result Result to extract from.
 * @return {?string} URL.
 */
cvox.AbstractResult.prototype.getURL = cvox.SearchUtil.extractURL;

/**
 * Returns the node to sync to.
 * @param {Node} result Result.
 * @return {?Node} Node to sync to.
 */
cvox.AbstractResult.prototype.getSyncNode = function(result) {
  return result;
};
