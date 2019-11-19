// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The spoken list builder. Used in test cases.
 */

goog.provide('cvox.SpokenListBuilder');
goog.require('cvox.QueueMode');


/**
 * Builds a spoken list.
 * @constructor
 */
cvox.SpokenListBuilder = function() {
  this.list_ = [];
};


/**
 * Adds an expected flushed utterance to the builder.
 * @param {string} expectedText The expected text.
 * @return {cvox.SpokenListBuilder} this.
 */
cvox.SpokenListBuilder.prototype.flush = function(expectedText) {
  this.list_.push([expectedText, cvox.QueueMode.FLUSH]);
  return this;  // for chaining
};


/**
 * Adds an expected queued utterance to the builder.
 * @param {string} expectedText The expected text.
 * @return {cvox.SpokenListBuilder} this.
 */
cvox.SpokenListBuilder.prototype.queue = function(expectedText) {
  this.list_.push([expectedText, cvox.QueueMode.QUEUE]);
  return this;  // for chaining
};


/**
 * Adds an expected category-flush utterance to the builder.
 * @param {string} expectedText The expected text.
 * @return {cvox.SpokenListBuilder} this.
 */
cvox.SpokenListBuilder.prototype.categoryFlush = function(expectedText) {
  this.list_.push([expectedText, cvox.QueueMode.CATEGORY_FLUSH]);
  return this;  // for chaining
};


/**
 * Builds the list.
 * @return {Array} The array of utterances.
 */
cvox.SpokenListBuilder.prototype.build = function() {
  return this.list_;
};
