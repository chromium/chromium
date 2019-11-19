// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A collection of JavaScript utilities used to improve selection
 * at different granularities.
 */


goog.provide('cvox.SelectionUtil');

goog.require('cvox.DomUtil');
goog.require('cvox.XpathUtil');

/**
 * Utilities for improving selection.
 * @constructor
 */
cvox.SelectionUtil = function() {};

/**
 * Cleans up a paragraph selection acquired by extending forward.
 * In this context, a paragraph selection is 'clean' when the focus
 * node (the end of the selection) is not on a text node.
 * @param {Selection} sel The paragraph-length selection.
 * @return {boolean} True if the selection has been cleaned.
 * False if the selection cannot be cleaned without invalid extension.
 */
cvox.SelectionUtil.cleanUpParagraphForward = function(sel) {
  var expand = true;

  // nodeType:3 == TEXT_NODE
  while (sel.focusNode.nodeType == 3) {
    // Ending with a text node, which is incorrect. Keep extending forward.
    var fnode = sel.focusNode;
    var foffset = sel.focusOffset;

    sel.modify('extend', 'forward', 'sentence');
    if ((fnode == sel.focusNode) && (foffset == sel.focusOffset)) {
      // Nothing more to be done, cannot extend forward further.
      return false;
    }
  }

  return true;
};

/**
 * Cleans up a paragraph selection acquired by extending backward.
 * In this context, a paragraph selection is 'clean' when the focus
 * node (the end of the selection) is not on a text node.
 * @param {Selection} sel The paragraph-length selection.
 * @return {boolean} True if the selection has been cleaned.
 *     False if the selection cannot be cleaned without invalid extension.
 */
cvox.SelectionUtil.cleanUpParagraphBack = function(sel) {
  var expand = true;

  var fnode;
  var foffset;

  // nodeType:3 == TEXT_NODE
  while (sel.focusNode.nodeType == 3) {
    // Ending with a text node, which is incorrect. Keep extending backward.
    fnode = sel.focusNode;
    foffset = sel.focusOffset;

    sel.modify('extend', 'backward', 'sentence');

    if ((fnode == sel.focusNode) && (foffset == sel.focusOffset)) {
      // Nothing more to be done, cannot extend backward further.
      return true;
    }
  }

  return true;
};

/**
 * Cleans up a sentence selection by extending forward.
 * In this context, a sentence selection is 'clean' when the focus
 * node (the end of the selection) is either:
 * - not on a text node
 * - on a text node that ends with a period or a space
 * @param {Selection} sel The sentence-length selection.
 * @return {boolean} True if the selection has been cleaned.
 *     False if the selection cannot be cleaned without invalid extension.
 */
cvox.SelectionUtil.cleanUpSentence = function(sel) {
  var expand = true;
  var lastSelection;
  var lastSelectionOffset;

  while (expand) {

    // nodeType:3 == TEXT_NODE
    if (sel.focusNode.nodeType == 3) {
      // The focus node is of type text, check end for period

      var fnode = sel.focusNode;
      var foffset = sel.focusOffset;

      if (sel.rangeCount > 0 && sel.getRangeAt(0).endOffset > 0) {
        if (fnode.substringData(sel.getRangeAt(0).endOffset - 1, 1) == '.') {
          // Text node ends with period.
          return true;
        } else if (fnode.substringData(sel.getRangeAt(0).endOffset - 1, 1) ==
                   ' ') {
          // Text node ends with space.
          return true;
        } else {
          // Text node does not end with period or space. Extend forward.
          sel.modify('extend', 'forward', 'sentence');

          if ((fnode == sel.focusNode) && (foffset == sel.focusOffset)) {
            // Nothing more to be done, cannot extend forward any further.
            return false;
          }
        }
      } else {
        return true;
      }
    } else {
      // Focus node is not text node, no further cleaning required.
      return true;
    }
  }

  return true;
};

/**
 * Finds the starting position (height from top and left width) of a
 * selection in a document.
 * @param {Selection} sel The selection.
 * @return {Array} The coordinates [top, left] of the selection.
 */
cvox.SelectionUtil.findSelPosition = function(sel) {
  if (sel.rangeCount == 0) {
    return [0, 0];
  }

  var clientRect = sel.getRangeAt(0).getBoundingClientRect();

  if (!clientRect) {
    return [0, 0];
  }

  var top = window.pageYOffset + clientRect.top;
  var left = window.pageXOffset + clientRect.left;
  return [top, left];
};

/**
 * Calculates the horizontal and vertical position of a node
 * @param {Node} targetNode The node.
 * @return {Array} The coordinates [top, left] of the node.
 */
cvox.SelectionUtil.findTopLeftPosition = function(targetNode) {
  var left = 0;
  var top = 0;
  var obj = targetNode;

  if (obj.offsetParent) {
    left = obj.offsetLeft;
    top = obj.offsetTop;
    obj = obj.offsetParent;

    while (obj !== null) {
      left += obj.offsetLeft;
      top += obj.offsetTop;
      obj = obj.offsetParent;
    }
  }

  return [top, left];
};


/**
 * Checks the contents of a selection for meaningful content.
 * @param {Selection} sel The selection.
 * @return {boolean} True if the selection is valid.  False if the selection
 *     contains only whitespace or is an empty string.
 */
cvox.SelectionUtil.isSelectionValid = function(sel) {
  var regExpWhiteSpace = new RegExp(/^\s+$/);
  return (! ((regExpWhiteSpace.test(sel.toString())) ||
             (sel.toString() == '')));
};

/**
 * Checks the contents of a range for meaningful content.
 * @param {Range} range The range.
 * @return {boolean} True if the range is valid.  False if the range
 *     contains only whitespace or is an empty string.
 */
cvox.SelectionUtil.isRangeValid = function(range) {
  var text = range.cloneContents().textContent;
  var regExpWhiteSpace = new RegExp(/^\s+$/);
  return (! ((regExpWhiteSpace.test(text)) ||
             (text == '')));
};

/**
 * Returns absolute top and left positions of an element.
 *
 * @param {!Node} node The element for which to compute the position.
 * @return {Array<number>} Index 0 is the left; index 1 is the top.
 * @private
 */
cvox.SelectionUtil.findPos_ = function(node) {
  var curLeft = 0;
  var curTop = 0;
  if (node.offsetParent) {
    do {
      curLeft += node.offsetLeft;
      curTop += node.offsetTop;
    } while (node = node.offsetParent);
  }
  return [curLeft, curTop];
};

/**
 * Scrolls node in its parent node such the given node is visible.
 * @param {Node} focusNode The node.
 */
cvox.SelectionUtil.scrollElementsToView = function(focusNode) {
  // First, walk up the DOM until we find a node with a bounding rectangle.
  while (focusNode && !focusNode.getBoundingClientRect) {
    focusNode = focusNode.parentElement;
  }
  if (!focusNode) {
    return;
  }

  // Walk up the DOM, ensuring each element is visible inside its parent.
  var node = focusNode;
  var parentNode = node.parentElement;
  while (node != document.body && parentNode) {
    node.scrollTop = node.offsetTop;
    node.scrollLeft = node.offsetLeft;
    node = parentNode;
    parentNode = node.parentElement;
  }

  // Center the active element on the page once we know it's visible.
  var pos = cvox.SelectionUtil.findPos_(focusNode);
  window.scrollTo(pos[0] - window.innerWidth / 2,
                  pos[1] - window.innerHeight / 2);
};

/**
 * Scrolls the selection into view if it is out of view in the current window.
 * Inspired by workaround for already-on-screen elements @
 * http://
 * www.performantdesign.com/2009/08/26/scrollintoview-but-only-if-out-of-view/
 * @param {Selection} sel The selection to be scrolled into view.
 */
cvox.SelectionUtil.scrollToSelection = function(sel) {
  if (sel.rangeCount == 0) {
    return;
  }

  // First, scroll all parent elements into view.  Later, move the body
  // which works slightly differently.

  cvox.SelectionUtil.scrollElementsToView(sel.focusNode);

  var pos = cvox.SelectionUtil.findSelPosition(sel);
  var top = pos[0];
  var left = pos[1];

  var scrolledVertically = window.pageYOffset ||
      document.documentElement.scrollTop ||
      document.body.scrollTop;
  var pageHeight = window.innerHeight ||
      document.documentElement.clientHeight || document.body.clientHeight;
  var pageWidth = window.innerWidth ||
      document.documentElement.innerWidth || document.body.clientWidth;

  if (left < pageWidth) {
    left = 0;
  }

  // window.scroll puts specified pixel in upper left of window
  if ((scrolledVertically + pageHeight) < top) {
    // Align with bottom of page
    var diff = top - pageHeight;
    window.scroll(left, diff + 100);
  } else if (top < scrolledVertically) {
    // Align with top of page
    window.scroll(left, top - 100);
  }
};

/**
 * This is from  https://developer.mozilla.org/en/Whitespace_in_the_DOM
 * Determine whether a node's text content is entirely whitespace.
 *
 * Throughout, whitespace is defined as one of the characters
 *  "\t" TAB \u0009
 *  "\n" LF  \u000A
 *  "\r" CR  \u000D
 *  " "  SPC \u0020
 *
 * This does not use Javascript's "\s" because that includes non-breaking
 * spaces (and also some other characters).
 *
 * @param {Node} node A node implementing the |CharacterData| interface (i.e.,
 *             a |Text|, |Comment|, or |CDATASection| node.
 * @return {boolean} True if all of the text content of |node| is whitespace,
 *             otherwise false.
 */
cvox.SelectionUtil.isAllWs = function(node) {
  // Use ECMA-262 Edition 3 String and RegExp features
  return !(/[^\t\n\r ]/.test(node.data));
};


/**
 * This is from  https://developer.mozilla.org/en/Whitespace_in_the_DOM
 * Determine if a node should be ignored by the iterator functions.
 *
 * @param {Node} node  An object implementing the DOM1 |Node| interface.
 * @return {boolean}  True if the node is:
 *                1) A |Text| node that is all whitespace
 *                2) A |Comment| node
 *             and otherwise false.
 */

cvox.SelectionUtil.isIgnorable = function(node) {
  return (node.nodeType == 8) || // A comment node
         ((node.nodeType == 3) &&
          cvox.SelectionUtil.isAllWs(node)); // a text node, all ws
};

/**
 * This is from  https://developer.mozilla.org/en/Whitespace_in_the_DOM
 * Version of |previousSibling| that skips nodes that are entirely
 * whitespace or comments.  (Normally |previousSibling| is a property
 * of all DOM nodes that gives the sibling node, the node that is
 * a child of the same parent, that occurs immediately before the
 * reference node.)
 *
 * @param {Node} sib  The reference node.
 * @return {Node} Either:
 *               1) The closest previous sibling to |sib| that is not
 *                  ignorable according to |isIgnorable|, or
 *               2) null if no such node exists.
 */
cvox.SelectionUtil.nodeBefore = function(sib) {
  while ((sib = sib.previousSibling)) {
    if (!cvox.SelectionUtil.isIgnorable(sib)) {
      return sib;
    }
  }
  return null;
};

/**
 * This is from  https://developer.mozilla.org/en/Whitespace_in_the_DOM
 * Version of |nextSibling| that skips nodes that are entirely
 * whitespace or comments.
 *
 * @param {Node} sib  The reference node.
 * @return {Node} Either:
 *               1) The closest next sibling to |sib| that is not
 *                  ignorable according to |isIgnorable|, or
 *               2) null if no such node exists.
 */
cvox.SelectionUtil.nodeAfter = function(sib) {
  while ((sib = sib.nextSibling)) {
    if (!cvox.SelectionUtil.isIgnorable(sib)) {
      return sib;
    }
  }
  return null;
};

/**
 * This is from  https://developer.mozilla.org/en/Whitespace_in_the_DOM
 * Version of |lastChild| that skips nodes that are entirely
 * whitespace or comments.  (Normally |lastChild| is a property
 * of all DOM nodes that gives the last of the nodes contained
 * directly in the reference node.)
 *
 * @param {Node} par  The reference node.
 * @return {Node} Either:
 *               1) The last child of |sib| that is not
 *                  ignorable according to |isIgnorable|, or
 *               2) null if no such node exists.
 */
cvox.SelectionUtil.lastChildNode = function(par) {
  var res = par.lastChild;
  while (res) {
    if (!cvox.SelectionUtil.isIgnorable(res)) {
      return res;
    }
    res = res.previousSibling;
  }
  return null;
};

/**
 * This is from  https://developer.mozilla.org/en/Whitespace_in_the_DOM
 * Version of |firstChild| that skips nodes that are entirely
 * whitespace and comments.
 *
 * @param {Node} par  The reference node.
 * @return {Node} Either:
 *               1) The first child of |sib| that is not
 *                  ignorable according to |isIgnorable|, or
 *               2) null if no such node exists.
 */
cvox.SelectionUtil.firstChildNode = function(par) {
  var res = par.firstChild;
  while (res) {
    if (!cvox.SelectionUtil.isIgnorable(res)) {
      return res;
    }
    res = res.nextSibling;
  }
  return null;
};

/**
 * This is from  https://developer.mozilla.org/en/Whitespace_in_the_DOM
 * Version of |data| that doesn't include whitespace at the beginning
 * and end and normalizes all whitespace to a single space.  (Normally
 * |data| is a property of text nodes that gives the text of the node.)
 *
 * @param {Node} txt  The text node whose data should be returned.
 * @return {string} A string giving the contents of the text node with
 *             whitespace collapsed.
 */
cvox.SelectionUtil.dataOf = function(txt) {
  var data = txt.data;
  // Use ECMA-262 Edition 3 String and RegExp features
  data = data.replace(/[\t\n\r ]+/g, ' ');
  if (data.charAt(0) == ' ') {
    data = data.substring(1, data.length);
  }
  if (data.charAt(data.length - 1) == ' ') {
    data = data.substring(0, data.length - 1);
  }
  return data;
};

/**
 * Returns true if the selection has content from at least one node
 * that has the specified tagName.
 *
 * @param {Selection} sel The selection.
 * @param {string} tagName  Tagname that the selection should be checked for.
 * @return {boolean} True if the selection has content from at least one node
 *                   with the specified tagName.
 */
cvox.SelectionUtil.hasContentWithTag = function(sel, tagName) {
  if (!sel || !sel.anchorNode || !sel.focusNode) {
    return false;
  }
  if (sel.anchorNode.tagName && (sel.anchorNode.tagName == tagName)) {
    return true;
  }
  if (sel.focusNode.tagName && (sel.focusNode.tagName == tagName)) {
    return true;
  }
  if (sel.anchorNode.parentNode.tagName &&
      (sel.anchorNode.parentNode.tagName == tagName)) {
    return true;
  }
  if (sel.focusNode.parentNode.tagName &&
      (sel.focusNode.parentNode.tagName == tagName)) {
    return true;
  }
  var docFrag = sel.getRangeAt(0).cloneContents();
  var span = document.createElement('span');
  span.appendChild(docFrag);
  return (span.getElementsByTagName(tagName).length > 0);
};

/**
 * Selects text within a text node.
 *
 * Note that the input node MUST be of type TEXT; otherwise, the offset
 * count would not mean # of characters - this is because of the way Range
 * works in JavaScript.
 *
 * @param {Node} textNode The text node to select text within.
 * @param {number} start  The start of the selection.
 * @param {number} end The end of the selection.
 */
cvox.SelectionUtil.selectText = function(textNode, start, end) {
  var newRange = document.createRange();
  newRange.setStart(textNode, start);
  newRange.setEnd(textNode, end);
  var sel = window.getSelection();
  sel.removeAllRanges();
  sel.addRange(newRange);
};

/**
 * Selects all the text in a given node.
 *
 * @param {Node} node The target node.
 */
cvox.SelectionUtil.selectAllTextInNode = function(node) {
  var newRange = document.createRange();
  newRange.setStart(node, 0);
  newRange.setEndAfter(node);
  var sel = window.getSelection();
  sel.removeAllRanges();
  sel.addRange(newRange);
};

/**
 * Collapses the selection to the start. If nothing is selected,
 * selects the beginning of the given node.
 *
 * @param {Node} node The target node.
 */
cvox.SelectionUtil.collapseToStart = function(node) {
  var sel = window.getSelection();
  var cursorNode = sel.anchorNode;
  var cursorOffset = sel.anchorOffset;
  if (cursorNode == null) {
    cursorNode = node;
    cursorOffset = 0;
  }
  var newRange = document.createRange();
  newRange.setStart(cursorNode, cursorOffset);
  newRange.setEnd(cursorNode, cursorOffset);
  sel.removeAllRanges();
  sel.addRange(newRange);
};

/**
 * Collapses the selection to the end. If nothing is selected,
 * selects the end of the given node.
 *
 * @param {Node} node The target node.
 */
cvox.SelectionUtil.collapseToEnd = function(node) {
  var sel = window.getSelection();
  var cursorNode = sel.focusNode;
  var cursorOffset = sel.focusOffset;
  if (cursorNode == null) {
    cursorNode = node;
    cursorOffset = 0;
  }
  var newRange = document.createRange();
  newRange.setStart(cursorNode, cursorOffset);
  newRange.setEnd(cursorNode, cursorOffset);
  sel.removeAllRanges();
  sel.addRange(newRange);
};

/**
 * Retrieves all the text within a selection.
 *
 * Note that this can be different than simply using the string from
 * window.getSelection() as this will account for IMG nodes, etc.
 *
 * @return {string} The string of text contained in the current selection.
 */
cvox.SelectionUtil.getText = function() {
  var sel = window.getSelection();
  if (cvox.SelectionUtil.hasContentWithTag(sel, 'IMG')) {
    var text = '';
    var docFrag = sel.getRangeAt(0).cloneContents();
    var span = document.createElement('span');
    span.appendChild(docFrag);
    var leafNodes = cvox.XpathUtil.getLeafNodes(span);
    for (var i = 0, node; node = leafNodes[i]; i++) {
      text = text + ' ' + cvox.DomUtil.getName(node);
    }
    return text;
  } else {
    return this.getSelectionText_();
  }
};

/**
 * Returns the selection as text instead of a selection object. Note that this
 * function must be used in place of getting text directly from the DOM
 * if you want i18n tests to pass.
 *
 * @return {string} The text.
 * @private
 */
cvox.SelectionUtil.getSelectionText_ = function() {
  return '' + window.getSelection();
};


/**
 * Returns a range as text instead of a selection object. Note that this
 * function must be used in place of getting text directly from the DOM
 * if you want i18n tests to pass.
 *
 * @param {Range} range A range.
 * @return {string} The text.
 */
cvox.SelectionUtil.getRangeText = function(range) {
  if (range)
    return range.cloneContents().textContent.replace(/\s+/g, ' ');
  else
    return '';
};
