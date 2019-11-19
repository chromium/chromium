// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An abstract class for walking at the sub-element level.
 * For example, walking at the sentence, word, or character level.
 * This class is an adapter around TraverseContent which exposes the interface
 * required by walkers. Subclasses must override the this.grain attribute
 * on initialization.
 */


goog.provide('cvox.AbstractSelectionWalker');

goog.require('Spannable');
goog.require('cvox.AbstractWalker');
goog.require('cvox.BareObjectWalker');
goog.require('cvox.DescriptionUtil');
goog.require('cvox.DomUtil');
goog.require('cvox.TraverseContent');

/**
 * @constructor
 * @extends {cvox.AbstractWalker}
 */
cvox.AbstractSelectionWalker = function() {
  cvox.AbstractWalker.call(this);
  this.objWalker_ = new cvox.BareObjectWalker();
  this.tc_ = cvox.TraverseContent.getInstance();
  this.grain /** @protected */ = ''; // child must override
};
goog.inherits(cvox.AbstractSelectionWalker, cvox.AbstractWalker);

/**
 * @override
 */
cvox.AbstractSelectionWalker.prototype.next = function(sel) {
  var r = sel.isReversed();
  this.tc_.syncToCursorSelection(sel.clone().setReversed(false));
  var ret = r ? this.tc_.prevElement(this.grain) :
      this.tc_.nextElement(this.grain);
  if (ret == null) {
    // Unfortunately, we can't trust TraverseContent; fall back to ObjectWalker.
    return this.objWalker_.next(sel);
  }
  var retSel = this.tc_.getCurrentCursorSelection().setReversed(r);
  var objSel = this.objWalker_.next(sel);
  objSel = objSel ? objSel.setReversed(r) : null;

  // ObjectWalker wins when there's a discrepancy between it and
  // TraverseContent. The only exception is with an end cursor on a text node.
  // In all other cases, this makes sure we visit the same selections as
  // object walker.
  if (objSel &&
      (retSel.end.node.constructor.name != 'Text' ||
          objSel.end.node.constructor.name != 'Text') &&
      !cvox.DomUtil.isDescendantOfNode(retSel.end.node, sel.end.node) &&
      !cvox.DomUtil.isDescendantOfNode(retSel.end.node, objSel.end.node)) {
    return objSel;
  }
  return retSel;
};

/**
 * @override
 */
cvox.AbstractSelectionWalker.prototype.sync = function(sel) {
  var r = sel.isReversed();
  var newSel = null;
  if (sel.start.equals(sel.end) && sel.start.node.constructor.name != 'Text') {
    var node = sel.start.node;

    // Find the deepest visible node; written specifically here because we want
    // to move across siblings if necessary and take the deepest node which can
    // be BODY.
    while (node &&
        cvox.DomUtil.directedFirstChild(node, r) &&
        !cvox.TraverseUtil.treatAsLeafNode(node)) {
      var child = cvox.DomUtil.directedFirstChild(node, r);

      // Find the first visible child.
      while (child) {
        if (cvox.DomUtil.isVisible(child,
            {checkAncestors: false, checkDescendants: false})) {
          node = child;
          break;
        } else {
          child = cvox.DomUtil.directedNextSibling(child, r);
        }
      }

      // node has no visible children; it's therefore the deepest visible node.
      if (!child) {
        break;
      }
    }
    newSel = cvox.CursorSelection.fromNode(node);
  } else {
    newSel = sel.clone();
    if (r) {
      newSel.start = newSel.end;
    } else {
      newSel.end = newSel.start;
    }
  }

  // This.next places us at the correct initial position (except below).
  newSel = this.next(newSel.setReversed(false));

  // ObjectWalker wins when there's a discrepancy between it and
  // TraverseContent. The only exception is with an end cursor on a text node.
  // In all other cases, this makes sure we visit the same selections as
  // object walker.
  var objSel = this.objWalker_.sync(sel);
  objSel = objSel ? objSel.setReversed(r) : null;

  if (!newSel) {
    return objSel;
  }

  newSel.setReversed(r);

  if (objSel &&
      (newSel.end.node.constructor.name != 'Text' ||
          objSel.end.node.constructor.name != 'Text') &&
      !cvox.DomUtil.isDescendantOfNode(newSel.end.node, sel.end.node) &&
      !cvox.DomUtil.isDescendantOfNode(newSel.end.node, objSel.end.node)) {
    return objSel;
  }
  return newSel;
};

/**
 * @override
 */
cvox.AbstractSelectionWalker.prototype.getDescription = function(prevSel, sel) {
  var description = cvox.DescriptionUtil.getDescriptionFromAncestors(
      cvox.DomUtil.getUniqueAncestors(prevSel.end.node, sel.start.node),
      true,
      cvox.ChromeVox.verbosity);
  description.text = sel.getText() || description.text;
  return [description];
};

/**
 * @override
 */
cvox.AbstractSelectionWalker.prototype.getBraille = function(prevSel, sel) {
  var node = sel.absStart().node;
  var text = cvox.TraverseUtil.getNodeText(node);
  var spannable = new Spannable(text);
  spannable.setSpan(node, 0, text.length);
  return new cvox.NavBraille({
    text: spannable,
    startIndex: sel.absStart().index,
    endIndex: sel.absEnd().index
  });
};
