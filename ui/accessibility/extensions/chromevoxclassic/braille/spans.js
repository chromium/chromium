// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Objects used in spannables as annotations for ARIA values
 * and selections.
 */

goog.provide('cvox.ExtraCellsSpan');
goog.provide('cvox.ValueSelectionSpan');
goog.provide('cvox.ValueSpan');

goog.require('Spannable');

/**
 * Attached to the value region of a braille spannable.
 * @param {number} offset The offset of the span into the value.
 * @constructor
 */
cvox.ValueSpan = function(offset) {
  /**
   * The offset of the span into the value.
   * @type {number}
   */
  this.offset = offset;
};


/**
 * Creates a value span from a json serializable object.
 * @param {!Object} obj The json serializable object to convert.
 * @return {!cvox.ValueSpan} The value span.
 */
cvox.ValueSpan.fromJson = function(obj) {
  return new cvox.ValueSpan(obj.offset);
};


/**
 * Converts this object to a json serializable object.
 * @return {!Object} The JSON representation.
 */
cvox.ValueSpan.prototype.toJson = function() {
  return this;
};


Spannable.registerSerializableSpan(
    cvox.ValueSpan,
    'cvox.ValueSpan',
    cvox.ValueSpan.fromJson,
    cvox.ValueSpan.prototype.toJson);


/**
 * Attached to the selected text within a value.
 * @constructor
 */
cvox.ValueSelectionSpan = function() {
};


Spannable.registerStatelessSerializableSpan(
    cvox.ValueSelectionSpan, 'cvox.ValueSelectionSpan');


/**
 * Causes raw cells to be added when translating from text to braille.
 * This is supported by the {@code cvox.ExpandingBrailleTranslator}
 * class.
 * @constructor
 */
cvox.ExtraCellsSpan = function() {
  /** @type {ArrayBuffer} */
  this.cells = new Uint8Array(0).buffer;
};
