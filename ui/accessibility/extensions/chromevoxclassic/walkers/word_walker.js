// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for walking one word at a time.
 */


goog.provide('cvox.WordWalker');

goog.require('cvox.AbstractSelectionWalker');
goog.require('cvox.TraverseContent');

/**
 * @constructor
 * @extends {cvox.AbstractSelectionWalker}
 */
cvox.WordWalker = function() {
  cvox.AbstractSelectionWalker.call(this);
  this.grain = cvox.TraverseContent.kWord;
};
goog.inherits(cvox.WordWalker, cvox.AbstractSelectionWalker);

/**
 * @override
 */
cvox.WordWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('word_granularity');
};
