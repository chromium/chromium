// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A JavaScript class for walking sentences.
 */


goog.provide('cvox.SentenceWalker');

goog.require('cvox.AbstractSelectionWalker');
goog.require('cvox.TraverseContent');

/**
 * @constructor
 * @extends {cvox.AbstractSelectionWalker}
 */
cvox.SentenceWalker = function() {
  cvox.AbstractSelectionWalker.call(this);
  this.grain = cvox.TraverseContent.kSentence;
};
goog.inherits(cvox.SentenceWalker, cvox.AbstractSelectionWalker);

/**
 * @override
 */
cvox.SentenceWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('sentence_granularity');
};
