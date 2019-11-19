// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

goog.provide('cvox.BrailleTextHandler');

goog.require('cvox.BrailleInterface');
goog.require('cvox.BrailleUtil');
goog.require('cvox.ChromeVox');
goog.require('cvox.NavBraille');

/**
 * @fileoverview Updates braille display contents following text changes.
 *
 */

/**
 * Represents an editable text region.
 *
 * @constructor
 * @param {!cvox.BrailleInterface} braille Braille interface.
 */
cvox.BrailleTextHandler = function(braille) {
  /**
   * Braille interface used to produce output.
   * @type {!cvox.BrailleInterface}
   * @private
   */
  this.braille_ = braille;
};


/**
 * Called by controller class when text changes.
 * @param {string} line The text of the line.
 * @param {number} start The 0-based index starting selection.
 * @param {number} end The 0-based index ending selection.
 * @param {boolean} multiline True if the text comes from a multi line text
 * field.
 * @param {Element} element DOM node which line comes from.
 * @param {number} lineStart Start offset of line (might be > 0 for multiline
 * fields).
 */
cvox.BrailleTextHandler.prototype.changed = function(
    line, start, end, multiline, element, lineStart) {
  var content;
  if (multiline) {
    var spannable = cvox.BrailleUtil.createValue(line, start, end, lineStart);
    if (element) {
      spannable.setSpan(element, 0, line.length);
    }
    content = new cvox.NavBraille({text: spannable,
                                   startIndex: start,
                                   endIndex: end});
  } else {
    if (cvox.ChromeVox.navigationManager) {
      content = cvox.ChromeVox.navigationManager.getBraille();
    }
  }
  if (content) {
    this.braille_.write(content);
  }
};
