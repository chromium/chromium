// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Some utilities for defining what groups are.
 */


goog.provide('cvox.GroupUtil');

goog.require('cvox.AriaUtil');
goog.require('cvox.DomUtil');


/**
 * If a node contains more characters than this, it should not be treated
 * as a leaf node by the smart navigation algorithm.
 *
 * This number was determined by looking at the average number of
 * characters in a paragraph:
 * http://www.fullondesign.co.uk/design/usability/
 * 285-how-many-characters-per-a-page-is-normal.htm
 * and then trying it out on a few popular websites (CNN, BBC,
 * Google Search, etc.) and making sure it made sense.
 * @type {number}
 * @private
 * @const
 */
cvox.GroupUtil.MAX_CHARCOUNT_ = 1500;


/**
 * If a node contains any of these elements, it should not be treated
 * as a leaf node by the smart navigation algorithm.
 * @type {string}
 * @private
 * @const
 */
cvox.GroupUtil.BREAKOUT_SELECTOR_ = 'blockquote,' +
    'button,' +
    'code,' +
    'form,' +
    'frame,' +
    'h1,' +
    'h2,' +
    'h3,' +
    'h4,' +
    'h5,' +
    'h6,' +
    'hr,' +
    'iframe,' +
    'input,' +
    'object,' +
    'ol,' +
    'p,' +
    'pre,' +
    'select,' +
    'table,' +
    'tr,' +
    'ul,' +
    'math,' +
  // This takes care of MathJax expressions.
    'span.math,' +
// TODO (sorge) Do we want to group all math or only display math?
//    '[mode="display"],' +
    // Aria widget roles
    '[role~="alert ' +
    'alertdialog ' +
    'button ' +
    'checkbox ' +
    'combobox ' +
    'dialog ' +
    'log ' +
    'marquee ' +
    'menubar ' +
    'progressbar ' +
    'radio ' +
    'radiogroup ' +
    'scrollbar ' +
    'slider ' +
    'spinbutton ' +
    'status ' +
    'tab ' +
    'tabpanel ' +
    'textbox ' +
    'toolbar ' +
    'tooltip ' +
    'treeitem ' +
    // Aria structure roles
    'article ' +
    'document ' +
    'group ' +
    'heading ' +
    'img ' +
    'list ' +
    'math ' +
    'region ' +
    'row ' +
    'separator"]';


/**
 * Returns true if this is a leaf node for groups.
 * true for a node => true for all child nodes
 * true if node has no children
 * @param {!Node} node The node to check.
 * @return {boolean} true if this is at the "leaf node" level or lower
 * for this granularity.
 */
cvox.GroupUtil.isLeafNode = function(node) {
  // TODO (stoarca): Write test to make sure that this function satisfies
  // the restriction given above.
  if (node.tagName == 'LABEL') {
    return cvox.DomUtil.isLeafNode(node);
  }
  if (cvox.DomUtil.isLeafNode(node)) {
    return true;
  }

  if (!cvox.DomUtil.isSemanticElt(node)) {
    var breakingNodes = node.querySelectorAll(
        cvox.GroupUtil.BREAKOUT_SELECTOR_);

    for (var i = 0; i < breakingNodes.length; ++i) {
      if (cvox.DomUtil.hasContent(breakingNodes[i])) {
        return false;
      }
    }
  }

  if (cvox.AriaUtil.isCompositeControl(node) &&
      !cvox.DomUtil.isFocusable(node)) {
    return false;
  }

  var content = cvox.DomUtil.collapseWhitespace(
      cvox.DomUtil.getValue(node) + ' ' +
      cvox.DomUtil.getName(node));
  if (content.length > cvox.GroupUtil.MAX_CHARCOUNT_) {
    return false;
  }

  if (content.replace(/\s/g, '') === '') {
    // Text only contains whitespace
    return false;
  }

  return true;
};
