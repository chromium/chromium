// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for walking "groups". Groups, intuitively, are logical
 * collections of dom elements. See AbstractNodeWalker and the
 * stopNodeDescent() method here for how groups are defined.
 */


goog.provide('cvox.GroupWalker');

goog.require('cvox.AbstractNodeWalker');
goog.require('cvox.BrailleUtil');
goog.require('cvox.CursorSelection');
goog.require('cvox.DescriptionUtil');
goog.require('cvox.DomUtil');
goog.require('cvox.GroupUtil');


/**
 * @constructor
 * @extends {cvox.AbstractNodeWalker}
 */
cvox.GroupWalker = function() {
  cvox.AbstractNodeWalker.call(this);
};
goog.inherits(cvox.GroupWalker, cvox.AbstractNodeWalker);


/**
 * @override
 */
cvox.GroupWalker.prototype.getDescription = function(prevSel, sel) {
  return cvox.DescriptionUtil.getCollectionDescription(prevSel, sel);
};


/**
 * @override
 */
cvox.GroupWalker.prototype.getBraille = function(prevSel, sel) {
  throw 'getBraille is unsupported';
};

/**
 * @override
 */
cvox.GroupWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('group_strategy');
};

/**
 * @override
 */
cvox.GroupWalker.prototype.stopNodeDescent = function(node) {
  return cvox.GroupUtil.isLeafNode(node);
};
