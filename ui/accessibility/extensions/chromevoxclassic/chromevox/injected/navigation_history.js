// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Navigation history tracks recently visited nodes. The
 * state of this class (the node history), is used to ensure the user is
 * navigating to and from valid nodes.
 * NOTE: The term "valid node" is simply a heuristic, defined in isValidNode_.
 *
 */


goog.provide('cvox.NavigationHistory');

goog.require('cvox.DomUtil');


/**
 * @constructor
 */
cvox.NavigationHistory = function() {
  this.reset_();
};


/**
 * The maximum length of history tracked for recently visited nodes.
 * @const
 * @type {number}
 * @private
 */
cvox.NavigationHistory.MAX_HISTORY_LEN_ = 30;


/**
 * Resets the navigation history.
 * @private
 */
cvox.NavigationHistory.prototype.reset_ = function() {
  var startNode = document.body;

  /**
   * An array of nodes ordered from newest to oldest in the history.
   * The most recent nodes are at the start of the array.
   * @type {Array<Node>}
   * @private
   */
  this.history_ = [startNode];

  /**
   * A flag to keep track of whether the last node added to the history was
   * valid or not. If false, something strange might be going on, and we
   * can react to this in the code.
   * @type {boolean}
   * @private
   */
  this.arrivedValid_ = true;

};


/**
 * Update the navigation history with the current element.
 * The most recent elements are at the start of the array.
 * @param {Node} newNode The new node to update the history with.
 */
cvox.NavigationHistory.prototype.update = function(newNode) {
  var previousNode = this.history_[0];

  // Avoid pushing consecutive duplicate elements.
  if (newNode && newNode != previousNode) {
    this.history_.unshift(newNode);
  }

  // If list is too long, pop the last (oldest) item.
  if (this.history_.length >
      cvox.NavigationHistory.MAX_HISTORY_LEN_) {
    this.history_.pop();
  }

  // Check if the node is valid upon arrival. If not, set a flag because
  // something fishy is probably going on.
  this.arrivedValid_ = this.isValidNode_(newNode);
};


/**
 * Routinely clean out history and determine if the given node has become
 * invalid since we arrived there (during the update call). If the node
 * was already invalid, we will return false.
 * @param {Node} node The node to check for validity change.
 * @return {boolean} True if node changed state to invalid.
 */
cvox.NavigationHistory.prototype.becomeInvalid = function(node) {
  // Remove any invalid nodes from history_.
  this.clean_();

  // If node was somehow already invalid on arrival, the page was probably
  // changing very quickly. Be defensive here and allow the default
  // navigation action by returning true.
  if (!this.arrivedValid_) {
    this.arrivedValid_ = true; // Reset flag.
    return false;
  }

  // Run the validation method on the given node.
  return !this.isValidNode_(node);
};


/**
 * Determine a valid reversion for the current navigation track. A reversion
 * provides both a current node to sync to and a previous node as context.
 * @param {function(Node)=} opt_predicate A function that takes in a node and
 *     returns true if it is a valid recovery candidate. Nodes that do not
 *     match the predicate are removed as we search for a match. If no
 *     predicate is provided, return the two most recent nodes.
 * @return {{current: ?Node, previous: ?Node}}
 *     The two nodes to override default navigation behavior with. Returning
 *     null or undefined means the history is empty.
 */
cvox.NavigationHistory.prototype.revert = function(opt_predicate) {
  // If the currently active element is valid, it is probably the best
  // recovery target. Add it to the history before computing the reversion.
  var active = document.activeElement;
  if (active != document.body && this.isValidNode_(active)) {
    this.update(active);
  }

  // Remove the most-recent-nodes that do not match the predicate.
  if (opt_predicate) {
    while (this.history_.length > 0) {
      var node = this.history_[0];
      if (opt_predicate(node)) {
        break;
      }
      this.history_.shift();
    }
  }

  // The reversion is just the first two nodes in the history.
  return {current: this.history_[0], previous: this.history_[1]};
};


/**
 * Remove any and all nodes from history_ that are no longer valid.
 * @return {boolean} True if any changes were made to the history.
 * @private
 */
cvox.NavigationHistory.prototype.clean_ = function() {
  var changed = false;
  for (var i = this.history_.length - 1; i >= 0; i--) {
    var valid = this.isValidNode_(this.history_[i]);
    if (!valid) {
      this.history_.splice(i, 1);
      changed = true;
    }
  }
  return changed;
};


/**
 * Determine if the given node is valid based on a heuristic.
 * A valid node must be attached to the DOM and visible.
 * @param {Node} node The node to validate.
 * @return {boolean} True if node is valid.
 * @private
 */
cvox.NavigationHistory.prototype.isValidNode_ = function(node) {
  // Confirm that the element is in the DOM.
  if (!cvox.DomUtil.isAttachedToDocument(node)) {
    return false;
  }

  // TODO (adu): In the future we may change this to just let users know the
  // node is invisible instead of restoring focus.
  if (!cvox.DomUtil.isVisible(node)) {
    return false;
  }

  return true;
};
