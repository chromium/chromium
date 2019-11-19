// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines the ContentEditableExtractor class.
 */

goog.provide('cvox.ContentEditableExtractor');

goog.require('cvox.Cursor');
goog.require('cvox.TraverseUtil');

/**
 * Extracts the text and line break information from a contenteditable region.
 * @constructor
 */
cvox.ContentEditableExtractor = function() {
  /**
   * The extracted, flattened, text.
   * @type {string}
   * @private
   */
  this.text_ = '';

  /**
   * The start cursor/selection index.
   * @type {number}
   * @private
   */
  this.start_ = 0;

  /**
   * The end cursor/selection index.
   * @type {number}
   * @private
   */
  this.end_ = 0;

  /**
   * Map from line index to a data structure containing the start
   * and end index within the line.
   * @type {Object<number, {startIndex: number, endIndex: number}>}
   * @private
   */
  this.lines_ = {};

  /**
   * Map from 0-based character index to 0-based line index.
   * @type {Array<number>}
   * @private
   */
  this.characterToLineMap_ = [];
};

/**
 * Update the metadata.
 * @param {Element} element The DOM element that's contentEditable.
 */
cvox.ContentEditableExtractor.prototype.update = function(element) {
  /**
   * Map from line index to a data structure containing the start
   * and end index within the line.
   * @type {Object<number, {startIndex: number, endIndex: number}>}
   */
  var lines = {0: {startIndex: 0, endIndex: 0}};
  var startCursor = new cvox.Cursor(element, 0, '');
  var endCursor = startCursor.clone();
  var range = document.createRange();
  var rect;
  var lineIndex = 0;
  var lastBottom = null;
  var text = '';
  var textSize = 0;
  var selectionStartIndex = -1;
  var selectionEndIndex = -1;
  var sel = window.getSelection();
  var selectionStart = new cvox.Cursor(sel.baseNode, sel.baseOffset, '');
  var selectionEnd = new cvox.Cursor(sel.extentNode, sel.extentOffset, '');
  var setStart = false;
  var setEnd = false;
  while (true) {
    var entered = [];
    var left = [];
    var c = cvox.TraverseUtil.forwardsChar(endCursor, entered, left);
    var done = false;
    if (!c) {
      done = true;
    }
    for (var i = 0; i < left.length && !done; i++) {
      if (left[i] == element) {
        done = true;
      }
    }
    if (done) {
      break;
    }

    range.setStart(startCursor.node, startCursor.index);
    range.setEnd(endCursor.node, endCursor.index);
    rect = range.getBoundingClientRect();
    if (!rect || rect.width == 0 || rect.height == 0) {
      continue;
    }

    if (lastBottom !== null &&
        rect.bottom != lastBottom &&
        textSize > 0 &&
        text.substr(-1).match(/\S/) &&
        c.match(/\S/)) {
      text += '\n';
      textSize++;
    }

    if (startCursor.node != endCursor.node && endCursor.index > 0) {
      range.setStart(endCursor.node, endCursor.index - 1);
      rect = range.getBoundingClientRect();
      if (!rect || rect.width == 0 || rect.height == 0) {
        continue;
      }
    }

    if (!setStart &&
        selectionStartIndex == -1 &&
        endCursor.node == selectionStart.node &&
        endCursor.index >= selectionStart.index) {
      if (endCursor.index > selectionStart.index) {
        selectionStartIndex = textSize;
      } else {
        setStart = true;
      }
    }
    if (!setEnd &&
        selectionEndIndex == -1 &&
        endCursor.node == selectionEnd.node &&
        endCursor.index >= selectionEnd.index) {
      if (endCursor.index > selectionEnd.index) {
        selectionEndIndex = textSize;
      } else {
        setEnd = true;
      }
    }

    if (lastBottom === null) {
      // This is the first character we've successfully measured on this
      // line. Save the vertical position but don't do anything else.
      lastBottom = rect.bottom;
    } else if (rect.bottom != lastBottom) {
      lines[lineIndex].endIndex = textSize;
      lineIndex++;
      lines[lineIndex] = {startIndex: textSize, endIndex: textSize};
      lastBottom = rect.bottom;
    }
    text += c;
    textSize++;
    startCursor = endCursor.clone();

    if (setStart) {
      selectionStartIndex = textSize;
      setStart = false;
    }
    if (setEnd) {
      selectionEndIndex = textSize;
      setEnd = false;
    }
  }

  // Finish up the last line.
  lines[lineIndex].endIndex = textSize;

  // Create a map from character index to line number.
  var characterToLineMap = [];
  for (var i = 0; i <= lineIndex; i++) {
    for (var j = lines[i].startIndex; j <= lines[i].endIndex; j++) {
      characterToLineMap[j] = i;
    }
  }

  // Finish updating fields.
  this.text_ = text;
  this.characterToLineMap_ = characterToLineMap;
  this.lines_ = lines;

  this.start_ = selectionStartIndex >= 0 ? selectionStartIndex : text.length;
  this.end_ = selectionEndIndex >= 0 ? selectionEndIndex : text.length;
};

/**
 * Get the text.
 * @return {string} The extracted, flattened, text.
 */
cvox.ContentEditableExtractor.prototype.getText = function() {
  return this.text_;
};

/**
 * @return {number} The start cursor/selection index.
 */
cvox.ContentEditableExtractor.prototype.getStartIndex = function() {
  return this.start_;
};

/**
 * @return {number} The end cursor/selection index.
 */
cvox.ContentEditableExtractor.prototype.getEndIndex = function() {
  return this.end_;
};

/**
 * Get the line number corresponding to a particular index.
 * @param {number} index The 0-based character index.
 * @return {number} The 0-based line number corresponding to that character.
 */
cvox.ContentEditableExtractor.prototype.getLineIndex = function(index) {
  return this.characterToLineMap_[index];
};

/**
 * Get the start character index of a line.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the first character in this line.
 */
cvox.ContentEditableExtractor.prototype.getLineStart = function(index) {
  return this.lines_[index].startIndex;
};

/**
 * Get the end character index of a line.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the end of this line.
 */
cvox.ContentEditableExtractor.prototype.getLineEnd = function(index) {
  return this.lines_[index].endIndex;
};
