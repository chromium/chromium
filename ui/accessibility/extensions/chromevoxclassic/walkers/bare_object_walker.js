// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A JavaScript class for walking the leaf nodes of the dom.
 * This is a bare class that tries to limit dependencies. It should only be used
 * when traversal of the leaf nodes is required (e.g. by other walkers), but
 * no other walker functionality (such as being able to describe the position).
 * It should not be used for user-visible navigation.
 */


goog.provide('cvox.BareObjectWalker');

goog.require('cvox.AbstractNodeWalker');

/**
 * @constructor
 * @extends {cvox.AbstractNodeWalker}
 */
cvox.BareObjectWalker = function() {
  goog.base(this);
};
goog.inherits(cvox.BareObjectWalker, cvox.AbstractNodeWalker);

/**
 * @override
 */
cvox.BareObjectWalker.prototype.stopNodeDescent = function(node) {
  return cvox.DomUtil.isLeafNode(node);
};
