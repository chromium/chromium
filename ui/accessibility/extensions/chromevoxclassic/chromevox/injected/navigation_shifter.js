// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview The purpose of this class is to delegate to the correct walker
 * based on the navigation state that it is in. The navigation state is a
 * simplified view of the external environment; the smallest amount of knowledge
 * needed to correctly delegate. One example is whether the user
 * is subnavigating. Note that while this class does
 * decide which walker to delegate to, it does NOT decide when its state
 * should be changed. This is done by the layer above. The reason for this
 * separation is that trying to make the decision here would require a lot
 * of knowledge about the environment, making this class harder to
 * test and maintain.
 *
 * This class knows about the public interfaces of all the walkers (rather
 * than just of the abstract class) since there are currently walker operations
 * which apply only to specific walkers.
 *
 * The navigation model is organized around having a chain of walkers with
 * increasing "granularity". This means (with a few exceptions), that if
 * walker A is more granular than walker B, then every valid selection in A
 * is a subset of a valid selection in B. For example, characters are
 * more granular than words, because every character is either a word or
 * inside a word.
 *
 * Note that while any callers may assume the granularity chain exists (after
 * all, there is a method makeMoreGranular()), they may not assume anything
 * about the order in which the walkers occur in this chain. This is because
 * the order may depend on the navigation state, and having external interaction
 * would slow down the changes we could make to this class (which is a problem,
 * since this is one of the core classes that impacts user-perceptible
 * navigation).
 *
 * Thinking of adding something here? Ask these questions:
 * Is it exposing functionality in some walker, the execution of which depends
 * on navigation state?
 *  Then it is a good candidate.
 * Does it require knowing more about the environment?
 *  If you are sure that it belongs here, then the minimum amount of knowledge
 *  to make the delegation decision should be added as state to this class.
 *  The decision for when this state changes should not be made in this class.
 *
 */


goog.provide('cvox.NavigationShifter');

goog.require('cvox.AbstractShifter');
goog.require('cvox.CharacterWalker');
goog.require('cvox.GroupWalker');
goog.require('cvox.LayoutLineWalker');
goog.require('cvox.ObjectWalker');
goog.require('cvox.SentenceWalker');
goog.require('cvox.TraverseContent');
goog.require('cvox.WordWalker');


/**
 * @constructor
 * @extends {cvox.AbstractShifter}
 */
cvox.NavigationShifter = function() {
  this.reset_();
  goog.base(this);
};
goog.inherits(cvox.NavigationShifter, cvox.AbstractShifter);

// These "const" literals may be used, but no order may be assumed
// between them by any outside callers.
/**
 * @type {Object<number>}
 */
cvox.NavigationShifter.GRANULARITIES = {
  'CHARACTER': 0,
  'WORD': 1,
  'LINE': 2,
  'SENTENCE': 3,
  'OBJECT': 4,
  'GROUP': 5
};


/**
 * Stores state variables in a provided object.
 *
 * @param {Object} store The object.
 */
cvox.NavigationShifter.prototype.storeOn = function(store) {
  store['granularity'] = this.getGranularity();
};


/**
 * Updates the object with state variables from an earlier storeOn call.
 *
 * @param {Object} store The object.
 */
cvox.NavigationShifter.prototype.readFrom = function(store) {
  this.setGranularity(store['granularity']);
};


/**
 * @override
 */
cvox.NavigationShifter.prototype.next = function(sel) {
  var ret = this.currentWalker_.next(sel);
  if (this.currentWalkerIndex_ <= cvox.NavigationShifter.GRANULARITIES.LINE &&
      ret) {
    cvox.TraverseContent.getInstance().syncToCursorSelection(
        ret.clone().setReversed(false));
    cvox.TraverseContent.getInstance().updateSelection();
  }
  return ret;
};


/**
 * @override
 */
cvox.NavigationShifter.prototype.sync = function(sel) {
  return this.currentWalker_.sync(sel);
};


/**
 * @override
 */
cvox.NavigationShifter.prototype.getName = function() {
  return Msgs.getMsg('navigation_shifter');
};


/**
 * @override
 */
cvox.NavigationShifter.prototype.getDescription = function(prevSel, sel) {
  return this.currentWalker_.getDescription(prevSel, sel);
};


/**
 * Gets the braille representation of a node-based selection.
 * @override
 */
cvox.NavigationShifter.prototype.getBraille = function(prevSel, sel) {
  return this.lineWalker_.getBraille(prevSel, sel);
};


/**
 * Delegates to currentWalker_.
 * @return {string} The message string.
 */
cvox.NavigationShifter.prototype.getGranularityMsg = function() {
  return this.currentWalker_.getGranularityMsg();
};


/**
 * @override
 */
cvox.NavigationShifter.prototype.makeMoreGranular = function() {
  goog.base(this, 'makeMoreGranular');
  this.currentWalkerIndex_ = Math.max(this.currentWalkerIndex_ - 1, 0);
  if (!cvox.NavigationShifter.allowSentence && this.currentWalkerIndex_ ==
      cvox.NavigationShifter.GRANULARITIES.SENTENCE) {
    this.currentWalkerIndex_--;
  }
  this.currentWalker_ = this.walkers_[this.currentWalkerIndex_];
};

/**
 * @override
 */
cvox.NavigationShifter.prototype.makeLessGranular = function() {
  goog.base(this, 'makeLessGranular');
  this.currentWalkerIndex_ =
      Math.min(this.currentWalkerIndex_ + 1, this.walkers_.length - 1);
  if (!cvox.NavigationShifter.allowSentence && this.currentWalkerIndex_ ==
      cvox.NavigationShifter.GRANULARITIES.SENTENCE) {
    this.currentWalkerIndex_++;
  }
  this.currentWalker_ = this.walkers_[this.currentWalkerIndex_];
};

/**
 * Shift to a specified granularity.
 * NOTE: after a shift, we are no longer subnavigating, if we were.
 * @param {number} granularity The granularity to shift to.
 */
cvox.NavigationShifter.prototype.setGranularity = function(granularity) {
  this.ensureNotSubnavigating();
  this.currentWalkerIndex_ = granularity;
  this.currentWalker_ = this.walkers_[this.currentWalkerIndex_];
};

/**
 * Gets the granularity.
 * @return {number} The current granularity.
 *
 */
cvox.NavigationShifter.prototype.getGranularity = function() {
  var wasSubnavigating = this.isSubnavigating();
  this.ensureNotSubnavigating();
  var ret = this.currentWalkerIndex_;
  if (wasSubnavigating) {
    this.ensureSubnavigating();
  }
  return ret;
};


/** Actions. */
/**
 * @override
 */
cvox.NavigationShifter.prototype.hasAction = function(name) {
  if (name == 'toggleLineType') {
    return true;
  }
  return goog.base(this, 'hasAction', name);
};


/**
 * @override
 */
cvox.NavigationShifter.create = function(sel) {
  return new cvox.NavigationShifter();
};


/**
 * Resets navigation shifter to a "new" state. Makes testing easier.
 * @private
 */
cvox.NavigationShifter.prototype.reset_ = function() {
  this.groupWalker_ = new cvox.GroupWalker();
  this.objectWalker_ = new cvox.ObjectWalker();
  this.sentenceWalker_ = new cvox.SentenceWalker();
  this.lineWalker_ = new cvox.LayoutLineWalker();
  this.wordWalker_ = new cvox.WordWalker();
  this.characterWalker_ = new cvox.CharacterWalker();

  this.walkers_ = [
      this.characterWalker_,
      this.wordWalker_,
      this.lineWalker_,
      this.sentenceWalker_,
      this.objectWalker_,
      this.groupWalker_
  ];
  this.currentWalkerIndex_ = this.walkers_.indexOf(this.groupWalker_);

  /**
   * @type {cvox.AbstractWalker}
   * @private
   */
  this.currentWalker_ = this.walkers_[this.currentWalkerIndex_];
};


/**
 * @type {boolean}
 */
cvox.NavigationShifter.allowSentence = false;
