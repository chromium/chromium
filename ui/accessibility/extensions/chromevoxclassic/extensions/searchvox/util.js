// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


/**
 * @fileoverview Helper functions.
 */

goog.provide('cvox.SearchUtil');

/** Utility functions. */
cvox.SearchUtil = function() {
};

/**
 * Extracts the first URL from an element.
 * @param {Node} node DOM element to extract from.
 * @return {?string} URL.
 */
cvox.SearchUtil.extractURL = function(node) {
  if (node) {
    if (node.tagName === 'A') {
      return node.href;
    }
    var anchor = node.querySelector('a');
    if (anchor) {
      return anchor.href;
    }
  }
  return null;
};

/**
 * Indicates whether or not the search widget has been activated.
 * @return {boolean} Whether or not the search widget is active.
 */
cvox.SearchUtil.isSearchWidgetActive = function() {
  var SEARCH_WIDGET_SELECT = '#cvox-search';
  return document.querySelector(SEARCH_WIDGET_SELECT) !== null;
};

/**
 * Adds one to and index with wrapping.
 * @param {number} index Index to add to.
 * @param {number} length Length to wrap at.
 * @return {number} The new index++, wrapped if exceeding length.
 */
cvox.SearchUtil.addOneWrap = function(index, length) {
  return (index + 1) % length;
};

/**
 * Subtracts one to and index with wrapping.
 * @param {number} index Index to subtract from.
 * @param {number} length Length to wrap at.
 * @return {number} The new index--, wrapped if below 0.
 */
cvox.SearchUtil.subOneWrap = function(index, length) {
  return (index - 1 + length) % length;
};

/**
 * Returns the id of a node's active descendant
 * @param {Node} targetNode The node.
 * @return {?string} The id of the active descendant.
 * @private
 */
var getActiveDescendantId_ = function(targetNode) {
  if (!targetNode.getAttribute) {
    return null;
  }

  var activeId = targetNode.getAttribute('aria-activedescendant');
  if (!activeId) {
    return null;
  }
  return activeId;
};

/**
 * If the node is an object with an active descendant, returns the
 * descendant node.
 *
 * This function will fully resolve an active descendant chain. If a circular
 * chain is detected, it will return null.
 *
 * @param {Node} targetNode The node to get descendant information for.
 * @return {Node} The descendant node or null if no node exists.
 */
var getActiveDescendant = function(targetNode) {
  var seenIds = {};
  var node = targetNode;

  while (node) {
    var activeId = getActiveDescendantId_(node);
    if (!activeId) {
      break;
    }
    if (activeId in seenIds) {
      // A circlar activeDescendant is an error, so return null.
      return null;
    }
    seenIds[activeId] = true;
    node = document.getElementById(activeId);
  }

  if (node == targetNode) {
    return null;
  }
  return node;
};

/**
 * Dispatches a left click event on the element that is the targetNode.
 * Clicks go in the sequence of mousedown, mouseup, and click.
 * @param {Node} targetNode The target node of this operation.
 * @param {boolean=} shiftKey Specifies if shift is held down.
 * @param {boolean=} callOnClickDirectly Specifies whether or not to directly
 * invoke the onclick method if there is one.
 * @param {boolean=} opt_double True to issue a double click.
 */
cvox.SearchUtil.clickElem = function(
    targetNode, shiftKey, callOnClickDirectly, opt_double) {
  // If there is an activeDescendant of the targetNode, then that is where the
  // click should actually be targeted.
  var activeDescendant = getActiveDescendant(targetNode);
  if (activeDescendant) {
    targetNode = activeDescendant;
  }
  if (callOnClickDirectly) {
    var onClickFunction = null;
    if (targetNode.onclick) {
      onClickFunction = targetNode.onclick;
    }
    if (!onClickFunction && (targetNode.nodeType != 1) &&
        targetNode.parentNode && targetNode.parentNode.onclick) {
      onClickFunction = targetNode.parentNode.onclick;
    }
    var keepGoing = true;
    if (onClickFunction) {
      try {
        keepGoing = onClickFunction();
      } catch (exception) {
        // Something went very wrong with the onclick method; we'll ignore it
        // and just dispatch a click event normally.
      }
    }
    if (!keepGoing) {
      // The onclick method ran successfully and returned false, meaning the
      // event should not bubble up, so we will return here.
      return;
    }
  }

  // Send a mousedown (or simply a double click if requested).
  var evt = document.createEvent('MouseEvents');
  var evtType = opt_double ? 'dblclick' : 'mousedown';
  evt.initMouseEvent(evtType, true, true, document.defaultView,
                     1, 0, 0, 0, 0, false, false, shiftKey, false, 0, null);
  // Mark any events we generate so we don't try to process our own events.
  evt.fromCvox = true;
  try {
    targetNode.dispatchEvent(evt);
  } catch (e) {}
  //Send a mouse up
  evt = document.createEvent('MouseEvents');
  evt.initMouseEvent('mouseup', true, true, document.defaultView,
                     1, 0, 0, 0, 0, false, false, shiftKey, false, 0, null);
  // Mark any events we generate so we don't try to process our own events.
  evt.fromCvox = true;
  try {
    targetNode.dispatchEvent(evt);
  } catch (e) {}
  //Send a click
  evt = document.createEvent('MouseEvents');
  evt.initMouseEvent('click', true, true, document.defaultView,
                     1, 0, 0, 0, 0, false, false, shiftKey, false, 0, null);
  // Mark any events we generate so we don't try to process our own events.
  evt.fromCvox = true;
  try {
    targetNode.dispatchEvent(evt);
  } catch (e) {}
};
