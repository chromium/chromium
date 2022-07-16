/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview LooseMock of goog.dom.AbstractRange.
 */

goog.setTestOnly('goog.testing.MockRange');
goog.provide('goog.testing.MockRange');

goog.require('goog.dom.AbstractRange');
goog.require('goog.dom.SavedCaretRange');
goog.require('goog.testing.LooseMock');



/**
 * LooseMock of goog.dom.AbstractRange. Useful because the mock framework cannot
 * simply create a mock out of an abstract class, and cannot create a mock out
 * of classes that implements __iterator__ because it relies on the default
 * behavior of iterating through all of an object's properties.
 * @constructor
 * @extends {goog.testing.LooseMock}
 * @final
 */
goog.testing.MockRange = function() {
  'use strict';
  goog.testing.LooseMock.call(this, goog.testing.MockRange.ConcreteRange_);
};
goog.inherits(goog.testing.MockRange, goog.testing.LooseMock);


// *** Private helper class ************************************************* //



/**
 * Concrete subclass of goog.dom.AbstractRange that simply sets the abstract
 * method __iterator__ to undefined so that javascript defaults to iterating
 * through all of the object's properties.
 * @constructor
 * @extends {goog.dom.AbstractRange}
 * @private
 */
goog.testing.MockRange.ConcreteRange_ = function() {
  'use strict';
  goog.dom.AbstractRange.call(this);
};
goog.inherits(goog.testing.MockRange.ConcreteRange_, goog.dom.AbstractRange);


/**
 * Undefine the iterator so the mock framework can loop through this class'
 * properties.
 * @override
 */
goog.testing.MockRange.ConcreteRange_.prototype.__iterator__ =
    // This isn't really type-safe.
    /** @type {?} */ (undefined);

/** @override */
goog.testing.MockRange.ConcreteRange_.prototype.saveUsingCarets = function() {
  'use strict';
  return (this.getStartNode() && this.getEndNode()) ?
      new goog.dom.SavedCaretRange(this) :
      null;
};
