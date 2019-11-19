// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Defines the EditableTextAreaShadow class.
 */

goog.provide('cvox.EditableTextAreaShadow');

/**
 * Creates a shadow element for an editable text area used to compute line
 * numbers.
 * @constructor
 */
cvox.EditableTextAreaShadow = function() {
  /**
   * @type {Element}
   * @private
   */
  this.shadowElement_ = document.createElement('div');

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
 * Update the shadow element.
 * @param {Element} element The textarea element.
 */
cvox.EditableTextAreaShadow.prototype.update = function(element) {
  document.body.appendChild(this.shadowElement_);

  while (this.shadowElement_.childNodes.length) {
    this.shadowElement_.removeChild(this.shadowElement_.childNodes[0]);
  }
  this.shadowElement_.style.cssText =
      window.getComputedStyle(element, null).cssText;
  this.shadowElement_.style.position = 'absolute';
  this.shadowElement_.style.top = '-9999';
  this.shadowElement_.style.left = '-9999';
  this.shadowElement_.setAttribute('aria-hidden', 'true');

  // Add the text to the shadow element, but with an extra character to the
  // end so that we can get the bounding box of the last line - we can't
  // measure blank lines otherwise.
  var text = element.value;
  var textNode = document.createTextNode(text + '.');
  this.shadowElement_.appendChild(textNode);

  /**
   * For extra speed, try to skip this many characters at a time - if
   * none of the characters are newlines and they're all at the same
   * vertical position, we don't have to examine each one. If not,
   * fall back to moving by one character at a time.
   * @const
   */
  var SKIP = 8;

  /**
   * Map from line index to a data structure containing the start
   * and end index within the line.
   * @type {Object<number, {startIndex: number, endIndex: number}>}
   */
  var lines = {0: {startIndex: 0, endIndex: 0}};

  var range = document.createRange();
  var offset = 0;
  var lastGoodOffset = 0;
  var lineIndex = 0;
  var lastBottom = null;
  var nearNewline = false;
  var rect;
  while (offset <= text.length) {
    range.setStart(textNode, offset);

    // If we're near the end or if there's an explicit newline character,
    // don't even try to skip.
    if (offset + SKIP > text.length ||
        text.substr(offset, SKIP).indexOf('\n') >= 0) {
      nearNewline = true;
    }

    if (nearNewline) {
      // Move by one character.
      offset++;
      range.setEnd(textNode, offset);
      rect = range.getBoundingClientRect();
    } else {
      // Try to move by |SKIP| characters.
      range.setEnd(textNode, offset + SKIP);
      rect = range.getBoundingClientRect();
      if (rect.bottom == lastBottom) {
        // Great, they all seem to be on the same line.
        offset += SKIP;
      } else {
        // Nope, there might be a newline, better go one at a time to be safe.
        if (rect && lastBottom !== null) {
          nearNewline = true;
        }
        offset++;
        range.setEnd(textNode, offset);
        rect = range.getBoundingClientRect();
      }
    }

    if (offset > 0 && text[offset - 1] == '\n') {
      // Handle an explicit newline character - that always results in
      // a new line.
      lines[lineIndex].endIndex = offset - 1;
      lineIndex++;
      lines[lineIndex] = {startIndex: offset, endIndex: offset};
      lastBottom = null;
      nearNewline = false;
      lastGoodOffset = offset;
    } else if (rect && (lastBottom === null)) {
      // This is the first character we've successfully measured on this
      // line. Save the vertical position but don't do anything else.
      lastBottom = rect.bottom;
    } else if (rect && rect.bottom != lastBottom) {
      // This character is at a different vertical position, so place an
      // implicit newline immediately after the *previous* good character
      // we found (which we now know was the last character of the previous
      // line).
      lines[lineIndex].endIndex = lastGoodOffset;
      lineIndex++;
      lines[lineIndex] = {startIndex: lastGoodOffset, endIndex: lastGoodOffset};
      lastBottom = rect ? rect.bottom : null;
      nearNewline = false;
    }

    if (rect) {
      lastGoodOffset = offset;
    }
  }
  // Finish up the last line.
  lines[lineIndex].endIndex = text.length;

  // Create a map from character index to line number.
  var characterToLineMap = [];
  for (var i = 0; i <= lineIndex; i++) {
    for (var j = lines[i].startIndex; j <= lines[i].endIndex; j++) {
      characterToLineMap[j] = i;
    }
  }

  // Finish updating fields and remove the shadow element.
  this.characterToLineMap_ = characterToLineMap;
  this.lines_ = lines;
  document.body.removeChild(this.shadowElement_);
};

/**
 * Get the line number corresponding to a particular index.
 * @param {number} index The 0-based character index.
 * @return {number} The 0-based line number corresponding to that character.
 */
cvox.EditableTextAreaShadow.prototype.getLineIndex = function(index) {
  return this.characterToLineMap_[index];
};

/**
 * Get the start character index of a line.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the first character in this line.
 */
cvox.EditableTextAreaShadow.prototype.getLineStart = function(index) {
  return this.lines_[index].startIndex;
};

/**
 * Get the end character index of a line.
 * @param {number} index The 0-based line index.
 * @return {number} The 0-based index of the end of this line.
 */
cvox.EditableTextAreaShadow.prototype.getLineEnd = function(index) {
  return this.lines_[index].endIndex;
};
