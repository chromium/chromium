/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview An API for saving and restoring ranges as HTML carets.
 */


goog.provide('goog.dom.SavedCaretRange');

goog.require('goog.dom');
goog.require('goog.dom.AbstractSavedCaretRange');
goog.require('goog.dom.TagName');
goog.require('goog.string');
goog.requireType('goog.dom.AbstractRange');


/**
 * A struct for holding context about saved selections.
 * This can be used to preserve the selection and restore while the DOM is
 * manipulated, or through an asynchronous call. Use goog.dom.Range factory
 * methods to obtain an {@see goog.dom.AbstractRange} instance, and use
 * {@see goog.dom.AbstractRange#saveUsingCarets} to obtain a SavedCaretRange.
 * For editor ranges under content-editable elements or design-mode iframes,
 * prefer using {@see goog.editor.range.saveUsingNormalizedCarets}.
 * @param {goog.dom.AbstractRange} range The range being saved.
 * @constructor
 * @extends {goog.dom.AbstractSavedCaretRange}
 */
goog.dom.SavedCaretRange = function(range) {
  'use strict';
  goog.dom.AbstractSavedCaretRange.call(this);

  /**
   * The DOM id of the caret at the start of the range.
   * @type {string}
   * @private
   */
  this.startCaretId_ = goog.string.createUniqueString();

  /**
   * The DOM id of the caret at the end of the range.
   * @type {string}
   * @private
   */
  this.endCaretId_ = goog.string.createUniqueString();

  /**
   * Whether the range is reversed (anchor at the end).
   * @private {boolean}
   */
  this.reversed_ = range.isReversed();

  /**
   * A DOM helper for storing the current document context.
   * @type {goog.dom.DomHelper}
   * @private
   */
  this.dom_ = goog.dom.getDomHelper(range.getDocument());

  range.surroundWithNodes(this.createCaret_(true), this.createCaret_(false));
};
goog.inherits(goog.dom.SavedCaretRange, goog.dom.AbstractSavedCaretRange);


/**
 * Gets the range that this SavedCaretRage represents, without selecting it
 * or removing the carets from the DOM.
 * @return {goog.dom.AbstractRange?} An abstract range.
 * @override
 */
goog.dom.SavedCaretRange.prototype.toAbstractRange = function() {
  'use strict';
  var range = null;
  var startCaret = this.getCaret(true);
  var endCaret = this.getCaret(false);
  if (startCaret && endCaret) {
    const TextRange = goog.module.get('goog.dom.TextRange');
    range = TextRange.createFromNodes(startCaret, 0, endCaret, 0);
  }
  return range;
};


/**
 * Gets carets.
 * @param {boolean} start If true, returns the start caret. Otherwise, get the
 *     end caret.
 * @return {Element} The start or end caret in the given document.
 * @override
 */
goog.dom.SavedCaretRange.prototype.getCaret = function(start) {
  'use strict';
  return this.dom_.getElement(start ? this.startCaretId_ : this.endCaretId_);
};


/**
 * Removes the carets from the current restoration document.
 * @param {goog.dom.AbstractRange=} opt_range A range whose offsets have already
 *     been adjusted for caret removal; it will be adjusted if it is also
 *     affected by post-removal operations, such as text node normalization.
 * @return {goog.dom.AbstractRange|undefined} The adjusted range, if opt_range
 *     was provided.
 * @override
 */
goog.dom.SavedCaretRange.prototype.removeCarets = function(opt_range) {
  'use strict';
  goog.dom.removeNode(this.getCaret(true));
  goog.dom.removeNode(this.getCaret(false));
  // This appears unused, but the range is sometimes adjusted in other
  // implementations of AbstractSavedCaretRange.
  return opt_range;
};


/**
 * Sets the document where the range will be restored.
 * @param {!Document} doc An HTML document.
 * @override
 */
goog.dom.SavedCaretRange.prototype.setRestorationDocument = function(doc) {
  'use strict';
  this.dom_.setDocument(doc);
};


/**
 * Reconstruct the selection from the given saved range. Removes carets after
 * restoring the selection. If restore does not dispose this saved range, it may
 * only be restored a second time if innerHTML or some other mechanism is used
 * to restore the carets to the dom.
 * @return {goog.dom.AbstractRange?} Restored selection.
 * @override
 * @protected
 */
goog.dom.SavedCaretRange.prototype.restoreInternal = function() {
  'use strict';
  var range = null;
  var anchorCaret = this.getCaret(!this.reversed_);
  var focusCaret = this.getCaret(this.reversed_);
  if (anchorCaret && focusCaret) {
    var anchorNode = anchorCaret.parentNode;
    var anchorOffset =
        Array.prototype.indexOf.call(anchorNode.childNodes, anchorCaret);
    var focusNode = focusCaret.parentNode;
    var focusOffset =
        Array.prototype.indexOf.call(focusNode.childNodes, focusCaret);
    if (focusNode == anchorNode) {
      // Compensate for the start caret being removed.
      if (this.reversed_) {
        anchorOffset--;
      } else {
        focusOffset--;
      }
    }

    const TextRange = goog.module.get('goog.dom.TextRange');
    range = TextRange.createFromNodes(
        anchorNode, anchorOffset, focusNode, focusOffset);
    range = this.removeCarets(range);
    range.select();
  } else {
    // If only one caret was found, remove it.
    this.removeCarets();
  }
  return range;
};


/**
 * Dispose the saved range and remove the carets from the DOM.
 * @override
 */
goog.dom.SavedCaretRange.prototype.disposeInternal = function() {
  'use strict';
  this.removeCarets();
  this.dom_ = null;
};


/**
 * Creates a caret element.
 * @param {boolean} start If true, creates the start caret. Otherwise,
 *     creates the end caret.
 * @return {!Element} The new caret element.
 * @private
 */
goog.dom.SavedCaretRange.prototype.createCaret_ = function(start) {
  'use strict';
  return this.dom_.createDom(
      goog.dom.TagName.SPAN,
      {'id': start ? this.startCaretId_ : this.endCaretId_});
};


/**
 * A regex that will match all saved range carets in a string.
 * @type {RegExp}
 */
goog.dom.SavedCaretRange.CARET_REGEX = /<span\s+id="?goog_\d+"?><\/span>/ig;


/**
 * Returns whether two strings of html are equal, ignoring any saved carets.
 * Thus two strings of html whose only difference is the id of their saved
 * carets will be considered equal, since they represent html with the
 * same selection.
 * @param {string} str1 The first string.
 * @param {string} str2 The second string.
 * @return {boolean} Whether two strings of html are equal, ignoring any
 *     saved carets.
 */
goog.dom.SavedCaretRange.htmlEqual = function(str1, str2) {
  'use strict';
  return str1 == str2 ||
      str1.replace(goog.dom.SavedCaretRange.CARET_REGEX, '') ==
      str2.replace(goog.dom.SavedCaretRange.CARET_REGEX, '');
};
