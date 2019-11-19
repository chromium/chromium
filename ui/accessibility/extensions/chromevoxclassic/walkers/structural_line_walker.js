// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A JavaScript class for walking lines.
 */


goog.provide('cvox.StructuralLineWalker');

goog.require('cvox.AbstractSelectionWalker');
goog.require('cvox.BrailleUtil');
goog.require('cvox.TraverseContent');

/**
 * @constructor
 * @extends {cvox.AbstractSelectionWalker}
 */
cvox.StructuralLineWalker = function() {
  goog.base(this);
  this.grain = cvox.TraverseContent.kLine;
};
goog.inherits(cvox.StructuralLineWalker, cvox.AbstractSelectionWalker);


/**
 * @override
 */
cvox.StructuralLineWalker.prototype.getGranularityMsg = function() {
  return Msgs.getMsg('structural_line');
};


/**
 * @override
 */
cvox.StructuralLineWalker.prototype.getDescription = function(prevSel, sel) {
  var desc = goog.base(this, 'getDescription', prevSel, sel);
  desc[0].text = cvox.DomUtil.getPrefixText(
      sel.absStart().node, sel.absStart().index) + desc[0].text;
  return desc;
};


/**
 * @override
 */
cvox.StructuralLineWalker.prototype.getBraille = function(prevSel, sel) {
  var braille = goog.base(this, 'getBraille', prevSel, sel);

  var objNode = this.objWalker_.sync(sel).absStart().node;
  var node = sel.absStart().node;
  var prevNode = prevSel.absEnd().node;

  // Show only the visible line in braille for DOM ranges. This overrides any
  // labels computed for the node.
  //
  // <textarea> needs to be treated specially. It may have TextNode children,
  // but these reflect the initial value of the node only, and are not updated
  // as content changes.
  var name = undefined;
  if (!sel.start.equals(sel.end) &&
      !cvox.DomPredicates.editTextPredicate([objNode])) {
    var prefix =
        cvox.DomUtil.getPrefixText(sel.absStart().node, sel.absStart().index);
    name = prefix + sel.getText();
  }
  var spannable =
      cvox.BrailleUtil.getTemplated(prevNode, objNode, {name: name});
  spannable.setSpan(objNode, 0, spannable.length);
  braille.text = spannable;

  // Remove any selections.
  braille.startIndex = 0;
  braille.endIndex = 0;
  return braille;
};
