// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A class representing a DOM selection conveyed through
 * CursorSelection idioms.
 * A PageSelection is just a DOM selection. The class itself manages a single
 * CursorSelection that surrounds a fragment on the page. It also provides an
 * extend operation to either grow or shrink the selection given a
 * CursorSelection. The class handles correctly moving the internal
 * CursorSelection and providing immediate access to a full description of the
 * selection at any time.
 */

goog.provide('cvox.PageSelection');

goog.require('cvox.AbstractEarcons');
goog.require('cvox.CursorSelection');
goog.require('cvox.NavDescription');

/**
 * @constructor
 * @param {!cvox.CursorSelection} sel The initial selection.
 */
cvox.PageSelection = function(sel) {
  this.sel_ = sel.clone();
  this.sel_.select();
  this.wasBegin_ = true;
};


/**
 * Gets a description for the DOM selection during the course of navigation.
 * @param {cvox.AbstractShifter} navShifter Used to obtain walker-based
 * descriptions.
 * @param {!cvox.CursorSelection} prevSel Previous CursorSelection in
 * navigation.
 * @param {!cvox.CursorSelection} curSel Current CursorSelection in navigation.
 * @return {Array<cvox.NavDescription>} The new description.
 */
cvox.PageSelection.prototype.getDescription =
    function(navShifter, prevSel, curSel) {
  var desc = [];
  if (this.sel_.isReversed() != curSel.isReversed()) {
    // A shrinking selection.
    desc = navShifter.getDescription(curSel, prevSel);
    desc[0].annotation = Msgs.getMsg('describe_unselected');
    desc[0].pushEarcon(cvox.Earcon.SELECTION_REVERSE);
  } else {
    // A growing selection.
    desc = navShifter.getDescription(prevSel, curSel);
    desc[0].annotation = Msgs.getMsg('describe_selected');
    desc[0].pushEarcon(cvox.Earcon.SELECTION);
    if (!this.wasBegin_ && this.sel_.absEquals(curSel.clone().normalize())) {
      // A selection has inverted across the start cursor. Describe it.
      var prevDesc = navShifter.getDescription(curSel, prevSel);
      prevDesc[0].annotation =
          Msgs.getMsg('describe_unselected');
      prevDesc[0].pushEarcon(cvox.Earcon.SELECTION_REVERSE);
      prevDesc[0].pushEarcon(cvox.Earcon.WRAP);
      desc = prevDesc.concat(desc);
    }
  }
  return desc;
};


/**
 * Gets a full description for the entire DOM selection.
 * Use this description when you want to describe the entire selection
 * represented by this instance.
 *
 * @return {Array<cvox.NavDescription>} The new description.
 */
cvox.PageSelection.prototype.getFullDescription = function() {
  return [new cvox.NavDescription(
      {text: window.getSelection().toString(),
       context: Msgs.getMsg('selection_is')})];
};


/**
 * Extends this selection.
 * @param {!cvox.CursorSelection} sel Extend DOM selection to the selection.
 * @return {boolean} True if the extension occurred, false if the PageSelection
 * was reset to sel.
 */
cvox.PageSelection.prototype.extend = function(sel) {
  if (!this.sel_.directedBefore(sel)) {
    // Do not allow for crossed selections. This restarts a page selection that
    // has been collapsed. This occurs when two CursorSelection's point away
    // from one another.
    this.sel_ = sel.clone();
  } else {
    // Otherwise, it is assumed that the CursorSelection's are in directed
    // document order. The CursorSelection's are either pointing in the same
    // direction or towards one another. In the first case, shrink/extend this
    // PageSelection to the end of "sel". In the second case, shrink/extend this
    // PageSelection to the start of "sel".
    this.sel_.end = this.sel_.isReversed() == sel.isReversed() ?
        sel.end.clone() : sel.start.clone();
  }
  this.sel_.select();
  this.wasBegin_ = false;
  return !this.sel_.absEquals(sel);
};
