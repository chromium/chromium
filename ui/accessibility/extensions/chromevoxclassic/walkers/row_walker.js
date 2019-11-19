// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for walking rows.
 */


goog.provide('cvox.RowWalker');

goog.require('cvox.TableWalker');


/**
 * @constructor
 * @extends {cvox.TableWalker}
 */
cvox.RowWalker = function() {
  goog.base(this);
};
goog.inherits(cvox.RowWalker, cvox.TableWalker);


/**
 * @override
 */
cvox.RowWalker.prototype.next = function(sel) {
  return this.nextRow(sel);
};


/**
 * @override
 */
cvox.RowWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('row_granularity');
};
