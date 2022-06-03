/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utilities for working with IE control ranges.
 *
 * @suppress {strictMissingProperties}
 */



// TODO(user): We're trying to migrate all ES5 subclasses of Closure
// Library to ES6. In ES6 this cannot be referenced before super is called. This
// file has at least one this before a super call (in ES5) and cannot be
// automatically upgraded to ES6 as a result. Please fix this if you have a
// chance. Note: This can sometimes be caused by not calling the super
// constructor at all. You can run the conversion tool yourself to see what it
// does on this file: blaze run //javascript/refactoring/es6_classes:convert.

goog.provide('goog.dom.ControlRange');
goog.provide('goog.dom.ControlRangeIterator');

goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.dom.AbstractMultiRange');
goog.require('goog.dom.AbstractRange');
goog.require('goog.dom.RangeIterator');
goog.require('goog.dom.RangeType');
goog.require('goog.dom.SavedCaretRange');
goog.require('goog.dom.SavedRange');
goog.require('goog.dom.TagWalkType');
goog.require('goog.dom.TextRange');
goog.require('goog.iter.StopIteration');
goog.require('goog.userAgent');



/**
 * Create a new control selection with no properties.  Do not use this
 * constructor: use one of the goog.dom.Range.createFrom* methods instead.
 * @constructor
 * @extends {goog.dom.AbstractMultiRange}
 * @final
 */
goog.dom.ControlRange = function() {
  'use strict';
  /**
   * The IE control range obejct.
   * @private {?Object}
   */
  this.range_ = null;

  /**
   * Cached list of elements.
   * @private {?Array<?Element>}
   */
  this.elements_ = null;

  /**
   * Cached sorted list of elements.
   * @private {?Array<?Element>}
   */
  this.sortedElements_ = null;
};
goog.inherits(goog.dom.ControlRange, goog.dom.AbstractMultiRange);


/**
 * Create a new range wrapper from the given browser range object.  Do not use
 * this method directly - please use goog.dom.Range.createFrom* instead.
 * @param {Object} controlRange The browser range object.
 * @return {!goog.dom.ControlRange} A range wrapper object.
 */
goog.dom.ControlRange.createFromBrowserRange = function(controlRange) {
  'use strict';
  var range = new goog.dom.ControlRange();
  range.range_ = controlRange;
  return range;
};


/**
 * Create a new range wrapper that selects the given element.  Do not use
 * this method directly - please use goog.dom.Range.createFrom* instead.
 * @param {...Element} var_args The element(s) to select.
 * @return {!goog.dom.ControlRange} A range wrapper object.
 */
goog.dom.ControlRange.createFromElements = function(var_args) {
  'use strict';
  var range = goog.dom.getOwnerDocument(arguments[0]).body.createControlRange();
  for (var i = 0, len = arguments.length; i < len; i++) {
    range.addElement(arguments[i]);
  }
  return goog.dom.ControlRange.createFromBrowserRange(range);
};


// Method implementations


/**
 * Clear cached values.
 * @private
 */
goog.dom.ControlRange.prototype.clearCachedValues_ = function() {
  'use strict';
  this.elements_ = null;
  this.sortedElements_ = null;
};


/** @override */
goog.dom.ControlRange.prototype.clone = function() {
  'use strict';
  return goog.dom.ControlRange.createFromElements.apply(
      this, this.getElements());
};


/** @override */
goog.dom.ControlRange.prototype.getType = function() {
  'use strict';
  return goog.dom.RangeType.CONTROL;
};


/** @override */
goog.dom.ControlRange.prototype.getBrowserRangeObject = function() {
  'use strict';
  return this.range_ || document.body.createControlRange();
};


/** @override */
goog.dom.ControlRange.prototype.setBrowserRangeObject = function(nativeRange) {
  'use strict';
  if (!goog.dom.AbstractRange.isNativeControlRange(nativeRange)) {
    return false;
  }
  this.range_ = nativeRange;
  return true;
};


/** @override */
goog.dom.ControlRange.prototype.getTextRangeCount = function() {
  'use strict';
  return this.range_ ? this.range_.length : 0;
};


/** @override */
goog.dom.ControlRange.prototype.getTextRange = function(i) {
  'use strict';
  return goog.dom.TextRange.createFromNodeContents(this.range_.item(i));
};


/** @override */
goog.dom.ControlRange.prototype.getContainer = function() {
  'use strict';
  return goog.dom.findCommonAncestor.apply(null, this.getElements());
};


/** @override */
goog.dom.ControlRange.prototype.getStartNode = function() {
  'use strict';
  return this.getSortedElements()[0];
};


/** @override */
goog.dom.ControlRange.prototype.getStartOffset = function() {
  'use strict';
  return 0;
};


/** @override */
goog.dom.ControlRange.prototype.getEndNode = function() {
  'use strict';
  var sorted = this.getSortedElements();
  var startsLast = /** @type {Node} */ (goog.array.peek(sorted));
  return /** @type {Node} */ (sorted.find(function(el) {
    'use strict';
    return goog.dom.contains(el, startsLast);
  }));
};


/** @override */
goog.dom.ControlRange.prototype.getEndOffset = function() {
  'use strict';
  return this.getEndNode().childNodes.length;
};


// TODO(robbyw): Figure out how to unify getElements with TextRange API.
/**
 * @return {!Array<Element>} Array of elements in the control range.
 */
goog.dom.ControlRange.prototype.getElements = function() {
  'use strict';
  if (!this.elements_) {
    this.elements_ = [];
    if (this.range_) {
      for (var i = 0; i < this.range_.length; i++) {
        this.elements_.push(this.range_.item(i));
      }
    }
  }

  return this.elements_;
};


/**
 * @return {!Array<Element>} Array of elements comprising the control range,
 *     sorted by document order.
 */
goog.dom.ControlRange.prototype.getSortedElements = function() {
  'use strict';
  if (!this.sortedElements_) {
    this.sortedElements_ = this.getElements().concat();
    this.sortedElements_.sort(function(a, b) {
      'use strict';
      return a.sourceIndex - b.sourceIndex;
    });
  }

  return this.sortedElements_;
};


/** @override */
goog.dom.ControlRange.prototype.isRangeInDocument = function() {
  'use strict';
  var returnValue = false;

  try {
    returnValue = this.getElements().every(function(element) {
      'use strict';
      // On IE, this throws an exception when the range is detached.
      return goog.userAgent.IE ?
          !!element.parentNode :
          goog.dom.contains(element.ownerDocument.body, element);
    });
  } catch (e) {
    // IE sometimes throws Invalid Argument errors for detached elements.
    // Note: trying to return a value from the above try block can cause IE
    // to crash.  It is necessary to use the local returnValue.
  }

  return returnValue;
};


/** @override */
goog.dom.ControlRange.prototype.isCollapsed = function() {
  'use strict';
  return !this.range_ || !this.range_.length;
};


/** @override */
goog.dom.ControlRange.prototype.getText = function() {
  'use strict';
  // TODO(robbyw): What about for table selections?  Should those have text?
  return '';
};


/** @override */
goog.dom.ControlRange.prototype.getHtmlFragment = function() {
  'use strict';
  return this.getSortedElements().map(goog.dom.getOuterHtml).join('');
};


/** @override */
goog.dom.ControlRange.prototype.getValidHtml = function() {
  'use strict';
  return this.getHtmlFragment();
};


/** @override */
goog.dom.ControlRange.prototype.getPastableHtml =
    goog.dom.ControlRange.prototype.getValidHtml;


/** @override */
goog.dom.ControlRange.prototype.__iterator__ = function(opt_keys) {
  'use strict';
  return new goog.dom.ControlRangeIterator(this);
};


// RANGE ACTIONS


/** @override */
goog.dom.ControlRange.prototype.select = function() {
  'use strict';
  if (this.range_) {
    this.range_.select();
  }
};


/** @override */
goog.dom.ControlRange.prototype.removeContents = function() {
  'use strict';
  // TODO(robbyw): Test implementing with execCommand('Delete')
  if (this.range_) {
    var nodes = [];
    for (var i = 0, len = this.range_.length; i < len; i++) {
      nodes.push(this.range_.item(i));
    }
    nodes.forEach(goog.dom.removeNode);

    this.collapse(false);
  }
};


/** @override */
goog.dom.ControlRange.prototype.replaceContentsWithNode = function(node) {
  'use strict';
  // Control selections have to have the node inserted before removing the
  // selection contents because a collapsed control range doesn't have start or
  // end nodes.
  var result = this.insertNode(node, true);

  if (!this.isCollapsed()) {
    this.removeContents();
  }

  return result;
};


// SAVE/RESTORE


/** @override */
goog.dom.ControlRange.prototype.saveUsingDom = function() {
  'use strict';
  return new goog.dom.DomSavedControlRange_(this);
};

/** @override */
goog.dom.ControlRange.prototype.saveUsingCarets = function() {
  'use strict';
  return (this.getStartNode() && this.getEndNode()) ?
      new goog.dom.SavedCaretRange(this) :
      null;
};

// RANGE MODIFICATION


/** @override */
goog.dom.ControlRange.prototype.collapse = function(toAnchor) {
  'use strict';
  // TODO(robbyw): Should this return a text range?  If so, API needs to change.
  this.range_ = null;
  this.clearCachedValues_();
};


// SAVED RANGE OBJECTS



/**
 * A SavedRange implementation using DOM endpoints.
 * @param {goog.dom.ControlRange} range The range to save.
 * @constructor
 * @extends {goog.dom.SavedRange}
 * @private
 */
goog.dom.DomSavedControlRange_ = function(range) {
  'use strict';
  /**
   * The element list.
   * @type {Array<Element>}
   * @private
   */
  this.elements_ = range.getElements();
};
goog.inherits(goog.dom.DomSavedControlRange_, goog.dom.SavedRange);


/** @override */
goog.dom.DomSavedControlRange_.prototype.restoreInternal = function() {
  'use strict';
  var doc = this.elements_.length ?
      goog.dom.getOwnerDocument(this.elements_[0]) :
      document;
  var controlRange = doc.body.createControlRange();
  for (var i = 0, len = this.elements_.length; i < len; i++) {
    controlRange.addElement(this.elements_[i]);
  }
  return goog.dom.ControlRange.createFromBrowserRange(controlRange);
};


/** @override */
goog.dom.DomSavedControlRange_.prototype.disposeInternal = function() {
  'use strict';
  goog.dom.DomSavedControlRange_.superClass_.disposeInternal.call(this);
  delete this.elements_;
};


// RANGE ITERATION



/**
 * Subclass of goog.dom.TagIterator that iterates over a DOM range.  It
 * adds functions to determine the portion of each text node that is selected.
 *
 * @param {goog.dom.ControlRange?} range The range to traverse.
 * @constructor
 * @extends {goog.dom.RangeIterator}
 * @final
 */
goog.dom.ControlRangeIterator = function(range) {
  'use strict';
  /**
   * The first node in the selection.
   * @private {?Node}
   */
  this.startNode_ = null;

  /**
   * The last node in the selection.
   * @private {?Node}
   */
  this.endNode_ = null;

  /**
   * The list of elements left to traverse.
   * @private {Array<?Element>?}
   */
  this.elements_ = null;

  if (range) {
    this.elements_ = range.getSortedElements();
    this.startNode_ = this.elements_.shift();
    this.endNode_ = /** @type {Node} */ (goog.array.peek(this.elements_)) ||
        this.startNode_;
  }

  goog.dom.ControlRangeIterator.base(
      this, 'constructor', this.startNode_, false);
};
goog.inherits(goog.dom.ControlRangeIterator, goog.dom.RangeIterator);


/** @override */
goog.dom.ControlRangeIterator.prototype.getStartTextOffset = function() {
  'use strict';
  return 0;
};


/** @override */
goog.dom.ControlRangeIterator.prototype.getEndTextOffset = function() {
  'use strict';
  return 0;
};


/** @override */
goog.dom.ControlRangeIterator.prototype.getStartNode = function() {
  'use strict';
  return this.startNode_;
};


/** @override */
goog.dom.ControlRangeIterator.prototype.getEndNode = function() {
  'use strict';
  return this.endNode_;
};


/** @override */
goog.dom.ControlRangeIterator.prototype.isLast = function() {
  'use strict';
  return !this.depth && !this.elements_.length;
};


/**
 * Move to the next position in the selection.
 * Throws `goog.iter.StopIteration` when it passes the end of the range.
 * @return {Node} The node at the next position.
 * @override
 */
goog.dom.ControlRangeIterator.prototype.nextValueOrThrow = function() {
  'use strict';
  // Iterate over each element in the range, and all of its children.
  if (this.isLast()) {
    throw goog.iter.StopIteration;
  } else if (!this.depth) {
    var el = this.elements_.shift();
    this.setPosition(
        el, goog.dom.TagWalkType.START_TAG, goog.dom.TagWalkType.START_TAG);
    return el;
  }

  // Call the super function.
  return goog.dom.ControlRangeIterator.superClass_.nextValueOrThrow.call(this);
};



/** @override */
goog.dom.ControlRangeIterator.prototype.copyFrom = function(other) {
  'use strict';
  var that = /** @type {!goog.dom.ControlRangeIterator} */ (other);
  this.elements_ = that.elements_;
  this.startNode_ = that.startNode_;
  this.endNode_ = that.endNode_;

  goog.dom.ControlRangeIterator.superClass_.copyFrom.call(this, that);
};


/**
 * @return {!goog.dom.ControlRangeIterator} An identical iterator.
 * @override
 */
goog.dom.ControlRangeIterator.prototype.clone = function() {
  'use strict';
  var copy = new goog.dom.ControlRangeIterator(null);
  copy.copyFrom(this);
  return copy;
};
