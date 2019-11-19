// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A JavaScript class for walking the leaf nodes of the dom.
 */


goog.provide('cvox.ObjectWalker');

goog.require('cvox.AbstractNodeWalker');
goog.require('cvox.BrailleUtil');
goog.require('cvox.DescriptionUtil');

/**
 * @constructor
 * @extends {cvox.AbstractNodeWalker}
 */
cvox.ObjectWalker = function() {
  goog.base(this);
};
goog.inherits(cvox.ObjectWalker, cvox.AbstractNodeWalker);

/**
 * @override
 */
cvox.ObjectWalker.prototype.stopNodeDescent = function(node) {
  return cvox.DomUtil.isLeafNode(node);
};

// TODO(dtseng): Causes a circular dependency if put into AbstractNodeWalker.
/**
 * @override
 */
cvox.AbstractNodeWalker.prototype.getDescription = function(prevSel, sel) {
  return cvox.DescriptionUtil.getDescriptionFromNavigation(
      prevSel.end.node,
      sel.start.node,
      true,
      cvox.ChromeVox.verbosity);
};

/**
 * @override
 */
cvox.ObjectWalker.prototype.getBraille = function(prevSel, sel) {
  throw 'getBraille is unsupported';
};

/**
 * @override
 */
cvox.ObjectWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('object_strategy');
};
