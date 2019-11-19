// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Logic for panning a braille display within a line of braille
 * content that might not fit on a single display.
 */

goog.provide('cvox.PanStrategy');

/**
 * @constructor
 *
 * A stateful class that keeps track of the current 'viewport' of a braille
 * display in a line of content.
 */
cvox.PanStrategy = function() {
  /**
   * @type {number}
   * @private
   */
  this.displaySize_ = 0;
  /**
   * @type {number}
   * @private
   */
  this.contentLength_ = 0;
  /**
   * Points before which it is desirable to break content if it doesn't fit
   * on the display.
   * @type {!Array<number>}
   * @private
   */
  this.breakPoints_ = [];
  /**
   * @type {!cvox.PanStrategy.Range}
   * @private
   */
  this.viewPort_ = {start: 0, end: 0};
};

/**
 * A range used to represent the viewport with inclusive start and xclusive
 * end position.
 * @typedef {{start: number, end: number}}
 */
cvox.PanStrategy.Range;

cvox.PanStrategy.prototype = {
  /**
   * Gets the current viewport which is never larger than the current
   * display size and whose end points are always within the limits of
   * the current content.
   * @type {!cvox.PanStrategy.Range}
   */
  get viewPort() {
    return this.viewPort_;
  },

  /**
   * Sets the display size.  This call may update the viewport.
   * @param {number} size the new display size, or {@code 0} if no display is
   *     present.
   */
  setDisplaySize: function(size) {
    this.displaySize_ = size;
    this.panToPosition_(this.viewPort_.start);
  },

  /**
   * Sets the current content that panning should happen within.  This call may
   * change the viewport.
   * @param {!ArrayBuffer} translatedContent The new content.
   * @param {number} targetPosition Target position.  The viewport is changed
   *     to overlap this position.
   */
  setContent: function(translatedContent, targetPosition) {
    this.breakPoints_ = this.calculateBreakPoints_(translatedContent);
    this.contentLength_ = translatedContent.byteLength;
    this.panToPosition_(targetPosition);
  },

  /**
   * If possible, changes the viewport to a part of the line that follows
   * the current viewport.
   * @return {boolean} {@code true} if the viewport was changed.
   */
  next: function() {
    var newStart = this.viewPort_.end;
    var newEnd;
    if (newStart + this.displaySize_ < this.contentLength_) {
      newEnd = this.extendRight_(newStart);
    } else {
      newEnd = this.contentLength_;
    }
    if (newEnd > newStart) {
      this.viewPort_ = {start: newStart, end: newEnd};
      return true;
    }
    return false;
  },

  /**
   * If possible, changes the viewport to a part of the line that precedes
   * the current viewport.
   * @return {boolean} {@code true} if the viewport was changed.
   */
  previous: function() {
    if (this.viewPort_.start > 0) {
      var newStart, newEnd;
      if (this.viewPort_.start <= this.displaySize_) {
        newStart = 0;
        newEnd = this.extendRight_(newStart);
      } else {
        newEnd = this.viewPort_.start;
        var limit = newEnd - this.displaySize_;
        newStart = limit;
        var pos = 0;
        while (pos < this.breakPoints_.length &&
            this.breakPoints_[pos] < limit) {
          pos++;
        }
        if (pos < this.breakPoints_.length &&
            this.breakPoints_[pos] < newEnd) {
          newStart = this.breakPoints_[pos];
        }
      }
      if (newStart < newEnd) {
        this.viewPort_ = {start: newStart, end: newEnd};
        return true;
      }
    }
    return false;
  },

  /**
   * Finds the end position for a new viewport start position, considering
   * current breakpoints as well as display size and content length.
   * @param {number} from Start of the region to extend.
   * @return {number}
   * @private
   */
  extendRight_: function(from) {
    var limit = Math.min(from + this.displaySize_, this.contentLength_);
    var pos = 0;
    var result = limit;
    while (pos < this.breakPoints_.length && this.breakPoints_[pos] <= from) {
      pos++;
    }
    while (pos < this.breakPoints_.length && this.breakPoints_[pos] <= limit) {
      result = this.breakPoints_[pos];
      pos++;
    }
    return result;
  },

  /**
   * Overridden by subclasses to provide breakpoints given translated
   * braille cell content.
   * @param {!ArrayBuffer} content New display content.
   * @return {!Array<number>} The points before which it is desirable to break
   *     content if needed or the empty array if no points are more desirable
   *     than any position.
   * @private
   */
  calculateBreakPoints_: function(content) {return [];},

  /**
   * Moves the viewport so that it overlaps a target position without taking
   * the current viewport position into consideration.
   * @param {number} position Target position.
   */
  panToPosition_: function(position) {
    if (this.displaySize_ > 0) {
      this.viewPort_ = {start: 0, end: 0};
      while (this.next() && this.viewPort_.end <= position) {
        // Nothing to do.
      }
    } else {
      this.viewPort_ = {start: position, end: position};
    }
  },
};

/**
 * A pan strategy that fits as much content on the display as possible, that
 * is, it doesn't do any wrapping.
 * @constructor
 * @extends {cvox.PanStrategy}
 */
cvox.FixedPanStrategy = cvox.PanStrategy;
/**
 * A pan strategy that tries to wrap 'words' when breaking content.
 * A 'word' in this context is just a chunk of non-blank braille cells
 * delimited by blank cells.
 * @constructor
 * @extends {cvox.PanStrategy}
 */
cvox.WrappingPanStrategy = function() {
  cvox.PanStrategy.call(this);
};

cvox.WrappingPanStrategy.prototype = {
  __proto__: cvox.PanStrategy.prototype,

  /** @override */
  calculateBreakPoints_: function(content) {
    var view = new Uint8Array(content);
    var newContentLength = view.length;
    var result = [];
    var lastCellWasBlank = false;
    for (var pos = 0; pos < view.length; ++pos) {
      if (lastCellWasBlank && view[pos] != 0) {
        result.push(pos);
      }
      lastCellWasBlank = (view[pos] == 0);
    }
    return result;
  },
};
