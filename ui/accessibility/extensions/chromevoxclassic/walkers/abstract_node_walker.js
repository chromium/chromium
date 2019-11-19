// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A base class for walkers that have a concept of lowest-level
 * node. Base classes must override the stopNodeDescent method to define
 * what a lowest-level node is. Then this walker will use those nodes as the
 * set of valid CursorSelections.
 */


goog.provide('cvox.AbstractNodeWalker');

goog.require('cvox.AbstractWalker');
goog.require('cvox.CursorSelection');
goog.require('cvox.DomUtil');

/**
 * @constructor
 * @extends {cvox.AbstractWalker}
 */
cvox.AbstractNodeWalker = function() {
  goog.base(this);

  /**
   * To keep track of and break infinite loops when trying to call next on
   * a body that does not DomUtil.hasContent().
   * @type {boolean}
   * @private
   */
  this.wasBegin_ = false;
};
goog.inherits(cvox.AbstractNodeWalker, cvox.AbstractWalker);

/**
 * @override
 */
cvox.AbstractNodeWalker.prototype.next = function(sel) {
  var r = sel.isReversed();
  var node = sel.end.node || document.body;
  if (!node) {
    return null;
  }
  do {
    node = cvox.DomUtil.directedNextLeafLikeNode(node, r,
        goog.bind(this.stopNodeDescent, this));
    if (!node) {
      return null;
    }
    // and repeat all of the above until we have a node that is not empty
  } while (node && !cvox.DomUtil.hasContent(node));

  return cvox.CursorSelection.fromNode(node).setReversed(r);
};

/**
 * @override
 */
cvox.AbstractNodeWalker.prototype.sync = function(sel) {
  var ret = this.privateSync_(sel);
  this.wasBegin_ = false;
  return ret;
};


/**
 * Private version of sync to ensure that when a body has no content, we
 * don't do an infinite loop trying to find an empty node.
 * @param {!cvox.CursorSelection} sel The selection.
 * @return {cvox.CursorSelection} The synced selection.
 * @private
 */
cvox.AbstractNodeWalker.prototype.privateSync_ = function(sel) {
  var r = sel.isReversed();

  if (sel.equals(cvox.CursorSelection.fromBody())) {
    if (this.wasBegin_) {
      // if body is empty, we return just the body selection
      return cvox.CursorSelection.fromBody().setReversed(r);
    }
    this.wasBegin_ = true;
  }

  var node = sel.start.node;

  while (node != document.body && node.parentNode &&
      this.stopNodeDescent(node.parentNode)) {
    node = node.parentNode;
  }

  while (node && !this.stopNodeDescent(node)) {
    node = cvox.DomUtil.directedFirstChild(node, r);
  }

  var n = cvox.CursorSelection.fromNode(node);
  if (!cvox.DomUtil.hasContent(node)) {
    n = this.next(/** @type {!cvox.CursorSelection} */
        (cvox.CursorSelection.fromNode(node)).setReversed(r));
  }
  if (n) {
    return n.setReversed(r);
  }
  return this.begin({reversed: r});
};

/**
 * Returns true if this is "a leaf node" or lower. That is,
 * it is at the lowest valid level or lower for this granularity.
 * RESTRICTION: true for a node => true for all child nodes
 * RESTRICTION: true if node has no children
 * @param {!Node} node The node to check.
 * @return {boolean} true if this is at the "leaf node" level or lower
 * for this granularity.
 * @protected
 */
cvox.AbstractNodeWalker.prototype.stopNodeDescent = goog.abstractMethod;
