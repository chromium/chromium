/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview A generic interface for saving and restoring ranges.
 */


goog.provide('goog.dom.AbstractSavedCaretRange');
goog.provide('goog.dom.SavedRange');

goog.require('goog.Disposable');
goog.require('goog.log');
goog.requireType('goog.dom.AbstractRange');



/**
 * Abstract interface for a saved range.
 * // TODO(user): rename to AbstractSavedRange?
 * @constructor
 * @extends {goog.Disposable}
 * @abstract
 */
goog.dom.SavedRange = function() {
  'use strict';
  goog.Disposable.call(this);
};
goog.inherits(goog.dom.SavedRange, goog.Disposable);


/**
 * Logging object.
 * @type {goog.log.Logger}
 * @private
 */
goog.dom.SavedRange.logger_ = goog.log.getLogger('goog.dom.SavedRange');


/**
 * Restores the range and by default disposes of the saved copy.  Take note:
 * this means the by default SavedRange objects are single use objects.
 * @param {boolean=} opt_stayAlive Whether this SavedRange should stay alive
 *     (not be disposed) after restoring the range. Defaults to false (dispose).
 * @return {goog.dom.AbstractRange} The restored range.
 */
goog.dom.SavedRange.prototype.restore = function(opt_stayAlive) {
  'use strict';
  if (this.isDisposed()) {
    goog.log.error(
        goog.dom.SavedRange.logger_,
        'Disposed SavedRange objects cannot be restored.');
  }

  var range = this.restoreInternal();
  if (!opt_stayAlive) {
    this.dispose();
  }
  return range;
};

/**
 * Internal method to restore the saved range.
 * @return {goog.dom.AbstractRange} The restored range.
 * @protected
 */
goog.dom.SavedRange.prototype.restoreInternal = goog.abstractMethod;

/**
 * Abstract interface for a range saved using carets.
 * @constructor
 * @extends {goog.dom.SavedRange}
 * @abstract
 */
goog.dom.AbstractSavedCaretRange = function() {
  'use strict';
  goog.dom.SavedRange.call(this);
};
goog.inherits(goog.dom.AbstractSavedCaretRange, goog.dom.SavedRange);

/**
 * Gets the range that this SavedCaretRage represents, without selecting it
 * or removing the carets from the DOM.
 * @return {goog.dom.AbstractRange?} An abstract range.
 */
goog.dom.AbstractSavedCaretRange.prototype.toAbstractRange =
    goog.abstractMethod;

/**
 * Gets carets.
 * @param {boolean} start If true, returns the start caret. Otherwise, get the
 *     end caret.
 * @return {?Element} The start or end caret in the given document.
 * @abstract
 */
goog.dom.AbstractSavedCaretRange.prototype.getCaret = function(start) {};

/**
 * Removes the carets from the current restoration document.
 * @param {!goog.dom.AbstractRange=} opt_range A range whose offsets have
 *     already been adjusted for caret removal; it will be adjusted if it is
 *     also affected by post-removal operations, such as text node
 *     normalization.
 * @return {?goog.dom.AbstractRange|undefined} The adjusted range, if opt_range
 *     was provided.
 * @abstract
 */
goog.dom.AbstractSavedCaretRange.prototype.removeCarets = function(
    opt_range) {};


/**
 * Sets the document where the range will be restored.
 * @param {!Document} doc An HTML document.
 * @abstract
 */
goog.dom.AbstractSavedCaretRange.prototype.setRestorationDocument = function(
    doc) {};
