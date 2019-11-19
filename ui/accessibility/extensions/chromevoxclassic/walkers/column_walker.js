// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for walking columns.
 */


goog.provide('cvox.ColumnWalker');

goog.require('cvox.TableWalker');


/**
 * @constructor
 * @extends {cvox.TableWalker}
 */
cvox.ColumnWalker = function() {
  goog.base(this);
};
goog.inherits(cvox.ColumnWalker, cvox.TableWalker);


/**
 * @override
 */
cvox.ColumnWalker.prototype.next = function(sel) {
  return this.nextCol(sel);
};


/**
 * @override
 */
cvox.ColumnWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('column_granularity');
};
