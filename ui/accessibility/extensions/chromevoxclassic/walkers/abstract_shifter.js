// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview An interface for an ordered collection of walkers, called a
 * shifter.
 */


goog.provide('cvox.AbstractShifter');

goog.require('cvox.AbstractWalker');
goog.require('cvox.CursorSelection');
goog.require('cvox.NavBraille');


/**
 * @constructor
 */
cvox.AbstractShifter = function() {
  this.isSubnavigating_ = false;
};


/**
 * Moves to the next selection in the DOM, performing any walker shifts as
 * necessary.
 * @param {!cvox.CursorSelection} sel The selection to go next from.
 * @return {cvox.CursorSelection} The resulting selection.
 */
cvox.AbstractShifter.prototype.next = goog.abstractMethod;


/**
 * Gets the first (or last) selection for this shifter's current granularity.
 * @param {?} sel
 * @param {{reversed: (undefined|boolean)}=} kwargs Extra arguments.
 *  reversed: If true, syncs to the end and returns a reversed selection.
 *    False by default.
 * @return {!cvox.CursorSelection} The valid selection.
 */
cvox.AbstractShifter.prototype.begin = function(sel, kwargs) {
  return this.currentWalker_.begin(kwargs);
};


/**
 * Syncs to this shifter.
 * @param {!cvox.CursorSelection} sel The selection to sync, if any.
 * @return {cvox.CursorSelection} The selection.
 */
cvox.AbstractShifter.prototype.sync = goog.abstractMethod;


/**
 * Name of this shifter.
 * @return {string} The shifter's name.
 */
cvox.AbstractShifter.prototype.getName = goog.abstractMethod;


/**
 * Gets the current description.
 * @param {!cvox.CursorSelection} prevSel The previous selection, for context.
 * @param {!cvox.CursorSelection} sel The current selection.
 * @return {Array<cvox.NavDescription>} The description array.
 */
cvox.AbstractShifter.prototype.getDescription = goog.abstractMethod;


/**
 * Gets the current braille.
 * @param {!cvox.CursorSelection} prevSel The previous selection, for context.
 * @param {!cvox.CursorSelection} sel The current selection.
 * @return {!cvox.NavBraille} The braille description.
 */
cvox.AbstractShifter.prototype.getBraille = goog.abstractMethod;


/**
 * Gets the granularity message.
 * @return {string} The message string.
 */
cvox.AbstractShifter.prototype.getGranularityMsg = goog.abstractMethod;


/**
 * Shifts to a less granular level.
 */
cvox.AbstractShifter.prototype.makeLessGranular = function() {
  this.ensureNotSubnavigating();
};


/**
 * Shifts to a more granular level.
 * NOTE: after a shift, we are no longer subnavigating, if we were.
 */
cvox.AbstractShifter.prototype.makeMoreGranular = function() {
  this.ensureNotSubnavigating();
};


/**
 * Enters subnavigation mode, if it was not already in it.
 * Subnavigation mode is where the shifter is temporarily one level
 * more granular (until either the next granularity shift or
 * ensureNotSubnavigating is called).
 */
cvox.AbstractShifter.prototype.ensureSubnavigating = function() {
  if (this.isSubnavigating_ == false) {
    this.makeMoreGranular();
    this.isSubnavigating_ = true;
  }
};


/**
 * Exits subnavigation mode, if it was in it.
 */
cvox.AbstractShifter.prototype.ensureNotSubnavigating = function() {
  if (this.isSubnavigating_ == true) {
    this.isSubnavigating_ = false;
    this.makeLessGranular();
  }
};


/**
 * Returns true if the shifter is currently in subnavigation mode.
 * @return {boolean} If in subnavigation mode.
 */
cvox.AbstractShifter.prototype.isSubnavigating = function() {
  return this.isSubnavigating_;
};


/**
 * Delegates to current walker.
  * @param {string} name Action name.
 * @return {boolean} True if this shifter contains action.
 */
cvox.AbstractShifter.prototype.hasAction = function(name) {
return this.currentWalker_.hasAction(name);
};


/**
 * Delegates an action to the current walker.
 * @param {string} name The action name.
 * @param {!cvox.CursorSelection} sel The current selection.
 * @return {cvox.CursorSelection} The selection after the action.
 */
cvox.AbstractShifter.prototype.performAction = function(name, sel) {
  return this.currentWalker_.performAction(name, sel);
};


/**
 * Factory method to create an instance of this shifter.
 * @param {!cvox.CursorSelection} sel The initial selection.
 * @return {cvox.AbstractShifter} The shifter or null if given a selection not
 * within the shifter's domain.
 */
cvox.AbstractShifter.create = goog.abstractMethod;
