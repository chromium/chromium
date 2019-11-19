// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Walkers to traverse a table.
 */


goog.provide('cvox.TableShifter');

goog.require('cvox.AbstractShifter');
goog.require('cvox.ColumnWalker');
goog.require('cvox.CursorSelection');
goog.require('cvox.DomPredicates');
goog.require('cvox.DomUtil');
goog.require('cvox.NavBraille');
goog.require('cvox.RowWalker');


/**
 * @constructor
 * @extends {cvox.AbstractShifter}
 */
cvox.TableShifter = function() {
  this.rowWalker_ = new cvox.RowWalker();
  this.columnWalker_ = new cvox.ColumnWalker();
  this.currentWalker_ = this.rowWalker_;
  this.bumpedEdge_ = false;
  this.begin_ = true;
  goog.base(this);
};
goog.inherits(cvox.TableShifter, cvox.AbstractShifter);


/**
 * @override
 */
cvox.TableShifter.prototype.next = function(sel) {
  var nextSel = this.currentWalker_.next(sel);
  if (!nextSel) {
    // Bumped edge.
    this.bumpedEdge_ = true;
    return sel;
  }
  return nextSel;
};


/**
 * @override
 */
cvox.TableShifter.prototype.sync = function(sel) {
  if (sel.start.node.tagName == 'TABLE') {
    return sel.isReversed() ? this.currentWalker_.goToLastCell(sel) :
        this.currentWalker_.goToFirstCell(sel);
  }
  return this.currentWalker_.sync(sel);
};


/**
 * @override
 */
cvox.TableShifter.prototype.getName = function() {
  return Msgs.getMsg('table_shifter');
};


/**
 * @override
 * @suppress {checkTypes} actual parameter 2 of
 * Msgs.prototype.getMsg does not match formal parameter
 * found   : Array<number>
 * required: (Array<string>|null|undefined)
 */
cvox.TableShifter.prototype.getDescription = function(prevSel, sel) {
  var descs = this.currentWalker_.getDescription(prevSel, sel);
  if (descs.length > 0) {
    if (this.bumpedEdge_) {
      descs[0].pushEarcon(cvox.Earcon.WRAP_EDGE);
      this.bumpedEdge_ = false;
    }
    if (this.begin_) {
      var len = descs.length;
      var summaryText = this.currentWalker_.tt.summaryText();
      var locationInfo = this.currentWalker_.getLocationInfo(sel);
      if (locationInfo != null) {
        descs.push(new cvox.NavDescription({
          context: Msgs.getMsg('table_location', locationInfo),
          text: '',
          annotation: summaryText ? summaryText + ' ' : ''
        }));
      }
      if (this.currentWalker_.tt.isSpanned()) {
        descs.push(new cvox.NavDescription({
          text: '',
          annotation: Msgs.getMsg('spanned')
        }));
      }
      this.begin_ = false;
    }
  }
  return descs;
};


/**
 * @override
 */
cvox.TableShifter.prototype.getBraille = function(prevSel, sel) {
  return this.currentWalker_.getBraille(prevSel, sel);
};


/**
 * @override
 */
cvox.TableShifter.prototype.getGranularityMsg = function() {
  return this.currentWalker_.getGranularityMsg();
};


/**
 * @override
 */
cvox.TableShifter.prototype.makeLessGranular = function() {
  goog.base(this, 'makeLessGranular');
  this.currentWalker_ = this.rowWalker_;
};


/**
 * @override
 */
cvox.TableShifter.prototype.makeMoreGranular = function() {
  goog.base(this, 'makeMoreGranular');
  this.currentWalker_ = this.columnWalker_;
};


/**
 * @override
 */
cvox.TableShifter.create = function(sel) {
  var ancestors = cvox.DomUtil.getAncestors(sel.start.node);
  if (cvox.DomPredicates.tablePredicate(ancestors) &&
      !cvox.DomPredicates.captionPredicate(ancestors)) {
    return new cvox.TableShifter();
  }
  return null;
};
