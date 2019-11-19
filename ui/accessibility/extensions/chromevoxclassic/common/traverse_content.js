// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview A DOM traversal interface for moving a selection around a
 * webpage. Provides multiple granularities:
 * 1. Move by paragraph.
 * 2. Move by sentence.
 * 3. Move by line.
 * 4. Move by word.
 * 5. Move by character.
 */

goog.provide('cvox.TraverseContent');

goog.require('cvox.CursorSelection');
goog.require('cvox.DomUtil');
goog.require('cvox.SelectionUtil');
goog.require('cvox.TraverseUtil');

/**
 * Moves a selection around a document or within a provided DOM object.
 *
 * @constructor
 * @param {Node=} domObj a DOM node (optional).
 */
cvox.TraverseContent = function(domObj) {
  if (domObj != null) {
    this.currentDomObj = domObj;
  } else {
    this.currentDomObj = document.body;
  }
  var range = document.createRange();
  // TODO (dmazzoni): Switch this to avoid using range methods. Range methods
  // can cause exceptions (such as if the node is not attached to the DOM).
  try {
    range.selectNode(this.currentDomObj);
    this.startCursor_ = new cvox.Cursor(
        range.startContainer, range.startOffset,
        cvox.TraverseUtil.getNodeText(range.startContainer));
    this.endCursor_ = new cvox.Cursor(
        range.endContainer, range.endOffset,
        cvox.TraverseUtil.getNodeText(range.endContainer));
  } catch (e) {
    // Ignoring this error so that it will not break everything else.
    window.console.log('Error: Unselectable node:');
    window.console.log(domObj);
  }
};
goog.addSingletonGetter(cvox.TraverseContent);

/**
 * Whether the last navigated selection only contained whitespace.
 * @type {boolean}
 */
cvox.TraverseContent.prototype.lastSelectionWasWhitespace = false;

/**
 * Whether we should skip whitespace when traversing individual characters.
 * @type {boolean}
 */
cvox.TraverseContent.prototype.skipWhitespace = false;

/**
 * If moveNext and movePrev should skip past an invalid selection,
 * so the user never gets stuck. Ideally the navigation code should never
 * return a range that's not a valid selection, but this keeps the user from
 * getting stuck if that code fails.  This is set to false for unit testing.
 * @type {boolean}
 */
cvox.TraverseContent.prototype.skipInvalidSelections = true;

/**
 * If line and sentence navigation should break at <a> links.
 * @type {boolean}
 */
cvox.TraverseContent.prototype.breakAtLinks = true;

/**
 * The string constant for character granularity.
 * @type {string}
 * @const
 */
cvox.TraverseContent.kCharacter = 'character';

/**
 * The string constant for word granularity.
 * @type {string}
 * @const
 */
cvox.TraverseContent.kWord = 'word';

/**
 * The string constant for sentence granularity.
 * @type {string}
 * @const
 */
cvox.TraverseContent.kSentence = 'sentence';

/**
 * The string constant for line granularity.
 * @type {string}
 * @const
 */
cvox.TraverseContent.kLine = 'line';

/**
 * The string constant for paragraph granularity.
 * @type {string}
 * @const
 */
cvox.TraverseContent.kParagraph = 'paragraph';

/**
 * A constant array of all granularities.
 * @type {Array<string>}
 * @const
 */
cvox.TraverseContent.kAllGrains =
    [cvox.TraverseContent.kParagraph,
     cvox.TraverseContent.kSentence,
     cvox.TraverseContent.kLine,
     cvox.TraverseContent.kWord,
     cvox.TraverseContent.kCharacter];

/**
 * Set the current position to match the current WebKit selection.
 */
cvox.TraverseContent.prototype.syncToSelection = function() {
  this.normalizeSelection();

  var selection = window.getSelection();
  if (!selection || !selection.anchorNode || !selection.focusNode) {
    return;
  }
  this.startCursor_ = new cvox.Cursor(
      selection.anchorNode, selection.anchorOffset,
      cvox.TraverseUtil.getNodeText(selection.anchorNode));
  this.endCursor_ = new cvox.Cursor(
      selection.focusNode, selection.focusOffset,
      cvox.TraverseUtil.getNodeText(selection.focusNode));
};

/**
 * Set the start and end cursors to the selection.
 * @param {cvox.CursorSelection} sel The selection.
 */
cvox.TraverseContent.prototype.syncToCursorSelection = function(sel) {
  this.startCursor_ = sel.start.clone();
  this.endCursor_ = sel.end.clone();
};

/**
 * Get the cursor selection.
 * @return {cvox.CursorSelection} The selection.
 */
cvox.TraverseContent.prototype.getCurrentCursorSelection = function() {
  return new cvox.CursorSelection(this.startCursor_, this.endCursor_);
};

/**
 * Set the WebKit selection based on the current position.
 */
cvox.TraverseContent.prototype.updateSelection = function() {
  cvox.TraverseUtil.setSelection(this.startCursor_, this.endCursor_);
  cvox.SelectionUtil.scrollToSelection(window.getSelection());
};

/**
 * Get the current position as a range.
 * @return {Range} The current range.
 */
cvox.TraverseContent.prototype.getCurrentRange = function() {
  var range = document.createRange();
  try {
    range.setStart(this.startCursor_.node, this.startCursor_.index);
    range.setEnd(this.endCursor_.node, this.endCursor_.index);
  } catch (e) {
    console.log('Invalid range ');
  }
  return range;
};

/**
 * Get the current text content as a string.
 * @return {string} The current spanned content.
 */
cvox.TraverseContent.prototype.getCurrentText = function() {
  return cvox.SelectionUtil.getRangeText(this.getCurrentRange());
};

/**
 * Collapse to the end of the range.
 */
cvox.TraverseContent.prototype.collapseToEnd = function() {
  this.startCursor_ = this.endCursor_.clone();
};

/**
 * Collapse to the start of the range.
 */
cvox.TraverseContent.prototype.collapseToStart = function() {
  this.endCursor_ = this.startCursor_.clone();
};

/**
 * Moves selection forward.
 *
 * @param {string} grain specifies "sentence", "word", "character",
 *     or "paragraph" granularity.
 * @return {?string} Either:
 *                1) The new selected text.
 *                2) null if the end of the domObj has been reached.
 */
cvox.TraverseContent.prototype.moveNext = function(grain) {
  var breakTags = this.getBreakTags();

  // As a special case, if the current selection is empty or all
  // whitespace, ensure that the next returned selection will NOT be
  // only whitespace - otherwise you can get trapped.
  var skipWhitespace = this.skipWhitespace;

  var range = this.getCurrentRange();
  if (!cvox.SelectionUtil.isRangeValid(range)) {
    skipWhitespace = true;
  }

  var elementsEntered = [];
  var elementsLeft = [];
  var str;
  do {
    if (grain === cvox.TraverseContent.kSentence) {
      str = cvox.TraverseUtil.getNextSentence(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft,
          breakTags);
    } else if (grain === cvox.TraverseContent.kWord) {
      str = cvox.TraverseUtil.getNextWord(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft);
    } else if (grain === cvox.TraverseContent.kCharacter) {
      str = cvox.TraverseUtil.getNextChar(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft,
          skipWhitespace);
    } else if (grain === cvox.TraverseContent.kParagraph) {
      str = cvox.TraverseUtil.getNextParagraph(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft);
    } else if (grain === cvox.TraverseContent.kLine) {
      str = cvox.TraverseUtil.getNextLine(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft,
          breakTags);
    } else {
      // User has provided an invalid string.
      // Fall through to default: extend by sentence
      window.console.log('Invalid selection granularity: "' + grain + '"');
      grain = cvox.TraverseContent.kSentence;
      str = cvox.TraverseUtil.getNextSentence(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft,
          breakTags);
    }

    if (str == null) {
      // We reached the end of the document.
      return null;
    }

    range = this.getCurrentRange();
    var isInvalid = !range.getBoundingClientRect();
  } while (this.skipInvalidSelections && isInvalid);

  if (!cvox.SelectionUtil.isRangeValid(range)) {
    // It's OK if the selection navigation lands on whitespace once (in
    // character granularity), but if it hits whitespace more than once, then
    // skip forward until there is real content.
    if (!this.lastSelectionWasWhitespace &&
        grain == cvox.TraverseContent.kCharacter) {
      this.lastSelectionWasWhitespace = true;
    } else {
      while (!cvox.SelectionUtil.isRangeValid(this.getCurrentRange())) {
        if (this.moveNext(grain) == null) {
          break;
        }
      }
    }
  } else {
    this.lastSelectionWasWhitespace = false;
  }

  return this.getCurrentText();
};


/**
 * Moves selection backward.
 *
 * @param {string} grain specifies "sentence", "word", "character",
 *     or "paragraph" granularity.
 * @return {?string} Either:
 *                1) The new selected text.
 *                2) null if the beginning of the domObj has been reached.
 */
cvox.TraverseContent.prototype.movePrev = function(grain) {
  var breakTags = this.getBreakTags();

  // As a special case, if the current selection is empty or all
  // whitespace, ensure that the next returned selection will NOT be
  // only whitespace - otherwise you can get trapped.
  var skipWhitespace = this.skipWhitespace;

  var range = this.getCurrentRange();
  if (!cvox.SelectionUtil.isRangeValid(range)) {
    skipWhitespace = true;
  }

  var elementsEntered = [];
  var elementsLeft = [];
  var str;
  do {
    if (grain === cvox.TraverseContent.kSentence) {
      str = cvox.TraverseUtil.getPreviousSentence(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft,
          breakTags);
    } else if (grain === cvox.TraverseContent.kWord) {
      str = cvox.TraverseUtil.getPreviousWord(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft);
    } else if (grain === cvox.TraverseContent.kCharacter) {
      str = cvox.TraverseUtil.getPreviousChar(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft,
          skipWhitespace);
    } else if (grain === cvox.TraverseContent.kParagraph) {
      str = cvox.TraverseUtil.getPreviousParagraph(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft);
    } else if (grain === cvox.TraverseContent.kLine) {
      str = cvox.TraverseUtil.getPreviousLine(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft,
          breakTags);
    } else {
      // User has provided an invalid string.
      // Fall through to default: extend by sentence
      window.console.log('Invalid selection granularity: "' + grain + '"');
      grain = cvox.TraverseContent.kSentence;
      str = cvox.TraverseUtil.getPreviousSentence(
          this.startCursor_, this.endCursor_, elementsEntered, elementsLeft,
          breakTags);
    }

    if (str == null) {
      // We reached the end of the document.
      return null;
    }

    range = this.getCurrentRange();
    var isInvalid = !range.getBoundingClientRect();
  } while (this.skipInvalidSelections && isInvalid);

  if (!cvox.SelectionUtil.isRangeValid(range)) {
    // It's OK if the selection navigation lands on whitespace once (in
    // character granularity), but if it hits whitespace more than once, then
    // skip forward until there is real content.
    if (!this.lastSelectionWasWhitespace &&
        grain == cvox.TraverseContent.kCharacter) {
      this.lastSelectionWasWhitespace = true;
    } else {
      while (!cvox.SelectionUtil.isRangeValid(this.getCurrentRange())) {
        if (this.movePrev(grain) == null) {
          break;
        }
      }
    }
  } else {
    this.lastSelectionWasWhitespace = false;
  }

  return this.getCurrentText();
};

/**
 * Get the tag names that should break a sentence or line. Currently
 * just an anchor 'A' should break a sentence or line if the breakAtLinks
 * flag is true, but in the future we might have other rules for breaking.
 *
 * @return {Object} An associative array mapping a tag name to true if
 *     it should break a sentence or line.
 */
cvox.TraverseContent.prototype.getBreakTags = function() {
  return {
    'A': this.breakAtLinks,
    'BR': true,
    'HR': true
  };
};

/**
 * Selects the next element of the document or within the provided DOM object.
 * Scrolls the window as appropriate.
 *
 * @param {string} grain specifies "sentence", "word", "character",
 *     or "paragraph" granularity.
 * @param {Node=} domObj a DOM node (optional).
 * @return {?string} Either:
 *                1) The new selected text.
 *                2) null if the end of the domObj has been reached.
 */
cvox.TraverseContent.prototype.nextElement = function(grain, domObj) {
  if (domObj != null) {
    this.currentDomObj = domObj;
  }

  var result = this.moveNext(grain);
  if (result != null &&
      (!cvox.DomUtil.isDescendantOfNode(
          this.startCursor_.node, this.currentDomObj) ||
       !cvox.DomUtil.isDescendantOfNode(
           this.endCursor_.node, this.currentDomObj))) {
    return null;
  }

  return result;
};


/**
 * Selects the previous element of the document or within the provided DOM
 * object. Scrolls the window as appropriate.
 *
 * @param {string} grain specifies "sentence", "word", "character",
 *     or "paragraph" granularity.
 * @param {Node=} domObj a DOM node (optional).
 * @return {?string} Either:
 *                1) The new selected text.
 *                2) null if the beginning of the domObj has been reached.
 */
cvox.TraverseContent.prototype.prevElement = function(grain, domObj) {
  if (domObj != null) {
    this.currentDomObj = domObj;
  }

  var result = this.movePrev(grain);
  if (result != null &&
      (!cvox.DomUtil.isDescendantOfNode(
          this.startCursor_.node, this.currentDomObj) ||
       !cvox.DomUtil.isDescendantOfNode(
           this.endCursor_.node, this.currentDomObj))) {
    return null;
  }

  return result;
};

/**
 * Make sure that exactly one item is selected. If there's no selection,
 * set the selection to the start of the document.
 */
cvox.TraverseContent.prototype.normalizeSelection = function() {
  var selection = window.getSelection();
  if (selection.rangeCount < 1) {
    // Before the user has clicked a freshly-loaded page

    var range = document.createRange();
    range.setStart(this.currentDomObj, 0);
    range.setEnd(this.currentDomObj, 0);

    selection.removeAllRanges();
    selection.addRange(range);

  } else if (selection.rangeCount > 1) {
    //  Multiple ranges exist - remove all ranges but the last one
    for (var i = 0; i < (selection.rangeCount - 1); i++) {
      selection.removeRange(selection.getRangeAt(i));
    }
  }
};

/**
 * Resets the selection.
 *
 * @param {Node=} domObj a DOM node.  Optional.
 *
 */
cvox.TraverseContent.prototype.reset = function(domObj) {
  window.getSelection().removeAllRanges();
};
