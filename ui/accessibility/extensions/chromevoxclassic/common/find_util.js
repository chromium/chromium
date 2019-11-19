// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Utilities for finding DOM nodes and CursorSelection's.
 */


goog.provide('cvox.FindUtil');

goog.require('cvox.BareObjectWalker');
goog.require('cvox.CursorSelection');


/**
 * @type {!cvox.BareObjectWalker}
 * @private
 */
cvox.FindUtil.objectWalker_ = new cvox.BareObjectWalker();


/**
 * Finds the next selection that matches the predicate function starting from
 * sel. Undefined if the nodes in sel are not attached to the document.
 * @param {!cvox.CursorSelection} sel The selection from which to start.
 * @param {function(Array<Node>):Node} predicate A function taking a
 * unique ancestor tree and outputting Node if the ancestor tree matches
 * the desired node to find.
 * @param {boolean=} opt_initialNode Whether to start the search from node
 * (true), or the next node (false); defaults to false.
 * @return {cvox.CursorSelection} The selection that was found.
 * null if end of document reached.
 */
cvox.FindUtil.findNext = function(sel, predicate, opt_initialNode) {
  var r = sel.isReversed();
  var cur = new cvox.CursorSelection(sel.absStart(), sel.absStart())
      .setReversed(r);

  // We may have been sync'ed into a subtree of the current predicate match.
  // Find our ancestor that matches the predicate.
  var ancestor;
  if (ancestor = predicate(cvox.DomUtil.getAncestors(cur.start.node))) {
    cur = cvox.CursorSelection.fromNode(ancestor).setReversed(r);
    if (opt_initialNode) {
      return cur;
    }
  }

  while (cur) {
    // Use ObjectWalker's traversal which guarantees us a stable iteration of
    // the DOM including returning null at page bounds.
    cur = cvox.FindUtil.objectWalker_.next(cur);
    var retNode = null;
    if (!cur ||
        (retNode = predicate(cvox.DomUtil.getAncestors(cur.start.node)))) {
      return retNode ? cvox.CursorSelection.fromNode(retNode) : null;
    }

    // Iframes require inter-frame messaging.
    if (cur.start.node.tagName == 'IFRAME') {
      return cur;
    }
  }
  return null;
};
