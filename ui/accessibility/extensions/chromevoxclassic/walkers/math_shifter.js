// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class for walking mathml expressions.
 */

goog.provide('cvox.MathShifter');

goog.require('cvox.AbstractShifter');
goog.require('cvox.BrailleUtil');
goog.require('cvox.CursorSelection');
goog.require('cvox.DomUtil');
goog.require('cvox.MathmlStore');
goog.require('cvox.MathmlStoreRules');
goog.require('cvox.NavDescription');
goog.require('cvox.SpeechRuleEngine');
goog.require('cvox.TraverseMath');


/**
 * @constructor
 * @extends {cvox.AbstractShifter}
 * @param {cvox.CursorSelection=} sel A cursor selection.
 */
cvox.MathShifter = function(sel) {
  goog.base(this);

  /**
   * Indicates the depth of the currently read expression.
   * @type {number}
   * @private
   */
  this.level_ = 0;

  /**
   * Indicates the vertical direction of movement (true for up, false for down).
   * @type {boolean}
   * @private
   */
  this.direction_ = false;

  /**
   * Indicates whether or not we've bumped against an edge in the math
   * structure.
   * @private
   */
  this.bumped_ = false;

cvox.TraverseMath.getInstance().initialize(sel.start.node);
};
goog.inherits(cvox.MathShifter, cvox.AbstractShifter);


/**
 * @override
 */
cvox.MathShifter.prototype.next = function(sel) {
  // Delegate to TraverseMath which manages selection inside of the math tree.
  var r = sel.isReversed();
  this.bumped_ = !cvox.TraverseMath.getInstance().nextSibling(r);
  var attachedNode = cvox.TraverseMath.getInstance().getAttachedActiveNode();
  return attachedNode ? cvox.CursorSelection.fromNode(attachedNode) : sel;
};


/**
 * @override
 */
cvox.MathShifter.prototype.sync = function(sel) {
  var attachedNode = cvox.TraverseMath.getInstance().getAttachedActiveNode();
  return attachedNode ? cvox.CursorSelection.fromNode(attachedNode) : sel;
};


/**
 * @override
 */
cvox.MathShifter.prototype.getName = function() {
  return Msgs.getMsg('math_shifter');
};


/**
 * @override
 */
cvox.MathShifter.prototype.getDescription = function(prevSel, sel) {
  var descs = cvox.SpeechRuleEngine.getInstance().evaluateNode(
      cvox.TraverseMath.getInstance().activeNode);
  if (this.bumped_ && descs.length > 0) {
    descs[0].pushEarcon(cvox.Earcon.WRAP_EDGE);
  }
  return descs;
};


/**
 * @override
 */
cvox.MathShifter.prototype.getBraille = function(prevSel, sel) {
  return new cvox.NavBraille({
    text: cvox.BrailleUtil.getTemplated(prevSel.start.node, sel.start.node)
  });
};


/**
 * @override
 */
cvox.MathShifter.prototype.getGranularityMsg = function() {
  return this.direction_ ? 'up to level ' + this.level_ :
      'down to level ' + this.level_;
};


/**
 * @override
 */
cvox.MathShifter.prototype.makeLessGranular = function() {
  this.level_ = this.level_ > 0 ? this.level_ - 1 : 0;
  this.direction_ = true;
  this.bumped_ = !cvox.TraverseMath.getInstance().nextParentChild(true);
};


/**
 * @override
 */
cvox.MathShifter.prototype.makeMoreGranular = function() {
  this.direction_ = false;
  this.bumped_ = !cvox.TraverseMath.getInstance().nextParentChild(false);
  if (!this.bumped_) {
    this.level_++;
  }
};


/**
 * @override
 */
cvox.MathShifter.create = function(sel) {
  if (cvox.DomPredicates.mathPredicate(
      cvox.DomUtil.getAncestors(sel.start.node))) {
    var mathNode = cvox.DomUtil.getContainingMath(sel.end.node);
    cvox.TraverseMath.getInstance().initialize(mathNode);
    cvox.SpeechRuleEngine.getInstance().parameterize(
        cvox.MathmlStore.getInstance());
    // TODO (sorge) Embed these changes into a local context menu/options menu.
    var dynamicCstr = cvox.MathStore.createDynamicConstraint(
        cvox.TraverseMath.getInstance().domain,
        cvox.TraverseMath.getInstance().style);
    cvox.SpeechRuleEngine.getInstance().setDynamicConstraint(dynamicCstr);
    return new cvox.MathShifter(sel);
  }
  return null;
};


/**
 * The active domain of the MathShifter.
 *
 * @return {string} The name of the current Math Domain.
 */
cvox.MathShifter.prototype.getDomainMsg = function() {
  return cvox.TraverseMath.getInstance().domain;
};
