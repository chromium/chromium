/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Utilities for working with W3C multi-part ranges.
 */



// TODO(user): We're trying to migrate all ES5 subclasses of Closure
// Library to ES6. In ES6 this cannot be referenced before super is called. This
// file has at least one this before a super call (in ES5) and cannot be
// automatically upgraded to ES6 as a result. Please fix this if you have a
// chance. Note: This can sometimes be caused by not calling the super
// constructor at all. You can run the conversion tool yourself to see what it
// does on this file: blaze run //javascript/refactoring/es6_classes:convert.

goog.provide('goog.dom.MultiRange');
goog.provide('goog.dom.MultiRangeIterator');

goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.dom.AbstractMultiRange');
goog.require('goog.dom.AbstractRange');
goog.require('goog.dom.RangeIterator');
goog.require('goog.dom.RangeType');
goog.require('goog.dom.SavedCaretRange');
goog.require('goog.dom.SavedRange');
goog.require('goog.dom.TextRange');
goog.require('goog.iter');
goog.require('goog.log');



/**
 * Creates a new multi part range with no properties.  Do not use this
 * constructor: use one of the goog.dom.Range.createFrom* methods instead.
 * @constructor
 * @extends {goog.dom.AbstractMultiRange}
 * @final
 */
goog.dom.MultiRange = function() {
  'use strict';
  /**
   * Logging object.
   * @private {goog.log.Logger}
   */
  this.logger_ = goog.log.getLogger('goog.dom.MultiRange');

  /**
   * Array of browser sub-ranges comprising this multi-range.
   * @private {Array<Range>}
   */
  this.browserRanges_ = [];

  /**
   * Lazily initialized array of range objects comprising this multi-range.
   * @private {Array<goog.dom.TextRange>}
   */
  this.ranges_ = [];

  /**
   * Lazily computed sorted version of ranges_, sorted by start point.
   * @private {Array<?goog.dom.TextRange>?}
   */
  this.sortedRanges_ = null;

  /**
   * Lazily computed container node.
   * @private {?Node}
   */
  this.container_ = null;
};
goog.inherits(goog.dom.MultiRange, goog.dom.AbstractMultiRange);


/**
 * Creates a new range wrapper from the given browser selection object.  Do not
 * use this method directly - please use goog.dom.Range.createFrom* instead.
 * @param {Selection} selection The browser selection object.
 * @return {!goog.dom.MultiRange} A range wrapper object.
 */
goog.dom.MultiRange.createFromBrowserSelection = function(selection) {
  'use strict';
  var range = new goog.dom.MultiRange();
  for (var i = 0, len = selection.rangeCount; i < len; i++) {
    range.browserRanges_.push(selection.getRangeAt(i));
  }
  return range;
};


/**
 * Creates a new range wrapper from the given browser ranges.  Do not
 * use this method directly - please use goog.dom.Range.createFrom* instead.
 * @param {Array<Range>} browserRanges The browser ranges.
 * @return {!goog.dom.MultiRange} A range wrapper object.
 */
goog.dom.MultiRange.createFromBrowserRanges = function(browserRanges) {
  'use strict';
  var range = new goog.dom.MultiRange();
  range.browserRanges_ = goog.array.clone(browserRanges);
  return range;
};


/**
 * Creates a new range wrapper from the given goog.dom.TextRange objects.  Do
 * not use this method directly - please use goog.dom.Range.createFrom* instead.
 * @param {Array<goog.dom.TextRange>} textRanges The text range objects.
 * @return {!goog.dom.MultiRange} A range wrapper object.
 */
goog.dom.MultiRange.createFromTextRanges = function(textRanges) {
  'use strict';
  var range = new goog.dom.MultiRange();
  range.ranges_ = textRanges;
  range.browserRanges_ = textRanges.map(function(range) {
    'use strict';
    return range.getBrowserRangeObject();
  });
  return range;
};


// Method implementations


/**
 * Clears cached values.  Should be called whenever this.browserRanges_ is
 * modified.
 * @private
 */
goog.dom.MultiRange.prototype.clearCachedValues_ = function() {
  'use strict';
  this.ranges_ = [];
  this.sortedRanges_ = null;
  this.container_ = null;
};


/**
 * @return {!goog.dom.MultiRange} A clone of this range.
 * @override
 */
goog.dom.MultiRange.prototype.clone = function() {
  'use strict';
  return goog.dom.MultiRange.createFromBrowserRanges(this.browserRanges_);
};


/** @override */
goog.dom.MultiRange.prototype.getType = function() {
  'use strict';
  return goog.dom.RangeType.MULTI;
};


/** @override */
goog.dom.MultiRange.prototype.getBrowserRangeObject = function() {
  'use strict';
  // NOTE(robbyw): This method does not make sense for multi-ranges.
  if (this.browserRanges_.length > 1) {
    goog.log.warning(
        this.logger_,
        'getBrowserRangeObject called on MultiRange with more than 1 range');
  }
  return this.browserRanges_[0];
};


/** @override */
goog.dom.MultiRange.prototype.setBrowserRangeObject = function(nativeRange) {
  'use strict';
  // TODO(robbyw): Look in to adding setBrowserSelectionObject.
  return false;
};


/** @override */
goog.dom.MultiRange.prototype.getTextRangeCount = function() {
  'use strict';
  return this.browserRanges_.length;
};


/** @override */
goog.dom.MultiRange.prototype.getTextRange = function(i) {
  'use strict';
  if (!this.ranges_[i]) {
    this.ranges_[i] =
        goog.dom.TextRange.createFromBrowserRange(this.browserRanges_[i]);
  }
  return this.ranges_[i];
};


/** @override */
goog.dom.MultiRange.prototype.getContainer = function() {
  'use strict';
  if (!this.container_) {
    var nodes = [];
    for (var i = 0, len = this.getTextRangeCount(); i < len; i++) {
      nodes.push(this.getTextRange(i).getContainer());
    }
    this.container_ = goog.dom.findCommonAncestor.apply(null, nodes);
  }
  return this.container_;
};


/**
 * @return {!Array<goog.dom.TextRange>} An array of sub-ranges, sorted by start
 *     point.
 */
goog.dom.MultiRange.prototype.getSortedRanges = function() {
  'use strict';
  if (!this.sortedRanges_) {
    this.sortedRanges_ = this.getTextRanges();
    this.sortedRanges_.sort(function(a, b) {
      'use strict';
      var aStartNode = a.getStartNode();
      var aStartOffset = a.getStartOffset();
      var bStartNode = b.getStartNode();
      var bStartOffset = b.getStartOffset();

      if (aStartNode == bStartNode && aStartOffset == bStartOffset) {
        return 0;
      }

      /**
       * @suppress {missingRequire} Cannot depend on goog.dom.Range because
       *     it creates a circular dependency.
       */
      const isReversed = goog.dom.Range.isReversed(
          aStartNode, aStartOffset, bStartNode, bStartOffset);
      return isReversed ? 1 : -1;
    });
  }
  return this.sortedRanges_;
};


/** @override */
goog.dom.MultiRange.prototype.getStartNode = function() {
  'use strict';
  return this.getSortedRanges()[0].getStartNode();
};


/** @override */
goog.dom.MultiRange.prototype.getStartOffset = function() {
  'use strict';
  return this.getSortedRanges()[0].getStartOffset();
};


/** @override */
goog.dom.MultiRange.prototype.getEndNode = function() {
  'use strict';
  // NOTE(robbyw): This may return the wrong node if any subranges overlap.
  return goog.array.peek(this.getSortedRanges()).getEndNode();
};


/** @override */
goog.dom.MultiRange.prototype.getEndOffset = function() {
  'use strict';
  // NOTE(robbyw): This may return the wrong value if any subranges overlap.
  return goog.array.peek(this.getSortedRanges()).getEndOffset();
};


/** @override */
goog.dom.MultiRange.prototype.isRangeInDocument = function() {
  'use strict';
  return this.getTextRanges().every(function(range) {
    'use strict';
    return range.isRangeInDocument();
  });
};


/** @override */
goog.dom.MultiRange.prototype.isCollapsed = function() {
  'use strict';
  return this.browserRanges_.length == 0 ||
      this.browserRanges_.length == 1 && this.getTextRange(0).isCollapsed();
};


/** @override */
goog.dom.MultiRange.prototype.getText = function() {
  'use strict';
  return this.getTextRanges()
      .map(function(range) {
        'use strict';
        return range.getText();
      })
      .join('');
};


/** @override */
goog.dom.MultiRange.prototype.getHtmlFragment = function() {
  'use strict';
  return this.getValidHtml();
};


/** @override */
goog.dom.MultiRange.prototype.getValidHtml = function() {
  'use strict';
  // NOTE(robbyw): This does not behave well if the sub-ranges overlap.
  return this.getTextRanges()
      .map(function(range) {
        'use strict';
        return range.getValidHtml();
      })
      .join('');
};


/** @override */
goog.dom.MultiRange.prototype.getPastableHtml = function() {
  'use strict';
  // TODO(robbyw): This should probably do something smart like group TR and TD
  // selections in to the same table.
  return this.getValidHtml();
};


/** @override */
goog.dom.MultiRange.prototype.__iterator__ = function(opt_keys) {
  'use strict';
  return new goog.dom.MultiRangeIterator(this);
};


// RANGE ACTIONS


/**
 * @override
 * @suppress {strictMissingProperties} Added to tighten compiler checks
 */
goog.dom.MultiRange.prototype.select = function() {
  'use strict';
  var selection =
      goog.dom.AbstractRange.getBrowserSelectionForWindow(this.getWindow());
  selection.removeAllRanges();
  for (var i = 0, len = this.getTextRangeCount(); i < len; i++) {
    selection.addRange(this.getTextRange(i).getBrowserRangeObject());
  }
};


/** @override */
goog.dom.MultiRange.prototype.removeContents = function() {
  'use strict';
  this.getTextRanges().forEach(function(range) {
    'use strict';
    range.removeContents();
  });
};


// SAVE/RESTORE


/** @override */
goog.dom.MultiRange.prototype.saveUsingDom = function() {
  'use strict';
  return new goog.dom.DomSavedMultiRange_(this);
};

/** @override */
goog.dom.MultiRange.prototype.saveUsingCarets = function() {
  'use strict';
  return (this.getStartNode() && this.getEndNode()) ?
      new goog.dom.SavedCaretRange(this) :
      null;
};

// RANGE MODIFICATION


/**
 * Collapses this range to a single point, either the first or last point
 * depending on the parameter.  This will result in the number of ranges in this
 * multi range becoming 1.
 * @param {boolean} toAnchor Whether to collapse to the anchor.
 * @override
 */
goog.dom.MultiRange.prototype.collapse = function(toAnchor) {
  'use strict';
  if (!this.isCollapsed()) {
    var range = toAnchor ? this.getTextRange(0) :
                           this.getTextRange(this.getTextRangeCount() - 1);

    this.clearCachedValues_();
    range.collapse(toAnchor);
    this.ranges_ = [range];
    this.sortedRanges_ = [range];
    this.browserRanges_ = [range.getBrowserRangeObject()];
  }
};


// SAVED RANGE OBJECTS



/**
 * A SavedRange implementation using DOM endpoints.
 * @param {goog.dom.MultiRange} range The range to save.
 * @constructor
 * @extends {goog.dom.SavedRange}
 * @private
 */
goog.dom.DomSavedMultiRange_ = function(range) {
  'use strict';
  /**
   * Array of saved ranges.
   * @type {Array<goog.dom.SavedRange>}
   * @private
   */
  this.savedRanges_ = range.getTextRanges().map(function(range) {
    'use strict';
    return range.saveUsingDom();
  });
};
goog.inherits(goog.dom.DomSavedMultiRange_, goog.dom.SavedRange);


/**
 * @return {!goog.dom.MultiRange} The restored range.
 * @override
 */
goog.dom.DomSavedMultiRange_.prototype.restoreInternal = function() {
  'use strict';
  var ranges = this.savedRanges_.map(function(savedRange) {
    'use strict';
    return savedRange.restore();
  });
  return goog.dom.MultiRange.createFromTextRanges(ranges);
};


/** @override */
goog.dom.DomSavedMultiRange_.prototype.disposeInternal = function() {
  'use strict';
  goog.dom.DomSavedMultiRange_.superClass_.disposeInternal.call(this);

  this.savedRanges_.forEach(function(savedRange) {
    'use strict';
    savedRange.dispose();
  });
  delete this.savedRanges_;
};


// RANGE ITERATION



/**
 * Subclass of goog.dom.TagIterator that iterates over a DOM range.  It
 * adds functions to determine the portion of each text node that is selected.
 *
 * @param {goog.dom.MultiRange} range The range to traverse.
 * @constructor
 * @extends {goog.dom.RangeIterator}
 * @final
 */
goog.dom.MultiRangeIterator = function(range) {
  'use strict';
  /**
   * The list of range iterators left to traverse.
   * @private {?Array<?goog.dom.RangeIterator>}
   */
  this.iterators_ = null;

  /**
   * The index of the current sub-iterator being traversed.
   * @private {number}
   */
  this.currentIdx_ = 0;

  if (range) {
    this.iterators_ = range.getSortedRanges().map(function(r) {
      'use strict';
      return goog.iter.toIterator(r);
    });
  }

  goog.dom.MultiRangeIterator.base(
      this, 'constructor', range ? this.getStartNode() : null, false);
};
goog.inherits(goog.dom.MultiRangeIterator, goog.dom.RangeIterator);


/** @override */
goog.dom.MultiRangeIterator.prototype.getStartTextOffset = function() {
  'use strict';
  return this.iterators_[this.currentIdx_].getStartTextOffset();
};


/** @override */
goog.dom.MultiRangeIterator.prototype.getEndTextOffset = function() {
  'use strict';
  return this.iterators_[this.currentIdx_].getEndTextOffset();
};


/** @override */
goog.dom.MultiRangeIterator.prototype.getStartNode = function() {
  'use strict';
  return this.iterators_[0].getStartNode();
};


/** @override */
goog.dom.MultiRangeIterator.prototype.getEndNode = function() {
  'use strict';
  return goog.array.peek(this.iterators_).getEndNode();
};


/** @override */
goog.dom.MultiRangeIterator.prototype.isLast = function() {
  'use strict';
  return this.iterators_[this.currentIdx_].isLast();
};


/**
 * @return {!IIterableResult<!Node>}
 * @override
 */
goog.dom.MultiRangeIterator.prototype.next = function() {
  'use strict';
  while (this.currentIdx_ < this.iterators_.length) {
    const iterator = this.iterators_[this.currentIdx_];
    const it = iterator.next();
    if (it.done) {
      this.currentIdx_++;
      // Try again from the top, will move to return 'done' if no more iterators
      continue;
    }
    this.setPosition(iterator.node, iterator.tagType, iterator.depth);
    return it;
  }
  return goog.iter.ES6_ITERATOR_DONE;
};


/** @override */
goog.dom.MultiRangeIterator.prototype.copyFrom = function(other) {
  'use strict';
  /** @suppress {strictMissingProperties} Added to tighten compiler checks */
  this.iterators_ = goog.array.clone(other.iterators_);
  goog.dom.MultiRangeIterator.superClass_.copyFrom.call(this, other);
};


/**
 * @return {!goog.dom.MultiRangeIterator} An identical iterator.
 * @override
 */
goog.dom.MultiRangeIterator.prototype.clone = function() {
  'use strict';
  var copy = new goog.dom.MultiRangeIterator(null);
  copy.copyFrom(this);
  return copy;
};
