// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Classes related to cursors that point to and select parts of
 * the automation tree.
 */

goog.provide('cursors.Cursor');
goog.provide('cursors.Movement');
goog.provide('cursors.Range');
goog.provide('cursors.Unit');

goog.require('AutomationPredicate');
goog.require('AutomationUtil');
goog.require('StringUtil');
goog.require('constants');

/**
 * The special index that represents a cursor pointing to a node without
 * pointing to any part of its accessible text.
 */
cursors.NODE_INDEX = -1;

/**
 * Represents units of CursorMovement.
 * @enum {string}
 */
cursors.Unit = {
  /** A single character within accessible name or value. */
  CHARACTER: 'character',

  /** A range of characters (given by attributes on automation nodes). */
  WORD: 'word',

  /** A leaf node. */
  NODE: 'node',

  /** A leaf DOM-node. */
  DOM_NODE: 'dom_node',

  /** Formed by a set of leaf nodes that are inline. */
  LINE: 'line'
};

/**
 * Represents the ways in which cursors can move given a cursor unit.
 * @enum {string}
 */
cursors.Movement = {
  /** Move to the beginning or end of the current unit. */
  BOUND: 'bound',

  /** Move to the next unit in a particular direction. */
  DIRECTIONAL: 'directional'
};

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var Movement = cursors.Movement;
var RoleType = chrome.automation.RoleType;
var Unit = cursors.Unit;

/**
 * Represents a position within the automation tree.
 * @constructor
 * @param {!AutomationNode} node
 * @param {number} index A 0-based index into this cursor node's primary
 * accessible name. An index of |cursors.NODE_INDEX| means the node as a whole
 * is pointed to and covers the case where the accessible text is empty.
 */
cursors.Cursor = function(node, index) {
  /** @type {number} @private */
  this.index_ = index;
  /** @type {Array<AutomationNode>} @private */
  this.ancestry_ = [];
  var nodeWalker = node;
  while (nodeWalker) {
    this.ancestry_.push(nodeWalker);
    nodeWalker = nodeWalker.parent;
    if (nodeWalker && AutomationPredicate.root(nodeWalker))
      break;
  }
};

/**
 * Convenience method to construct a Cursor from a node.
 * @param {!AutomationNode} node
 * @return {!cursors.Cursor}
 */
cursors.Cursor.fromNode = function(node) {
  return new cursors.Cursor(node, cursors.NODE_INDEX);
};

cursors.Cursor.prototype = {
  /**
   * Returns true if |rhs| is equal to this cursor.
   * @param {!cursors.Cursor} rhs
   * @return {boolean}
   */
  equals: function(rhs) {
    return this.node === rhs.node && this.index === rhs.index;
  },

  /**
   * Returns the node. If the node is invalid since the last time it
   * was accessed, moves the cursor to the nearest valid ancestor first.
   * @return {AutomationNode}
   */
  get node() {
    for (var i = 0; i < this.ancestry_.length; i++) {
      var firstValidNode = this.ancestry_[i];
      if (firstValidNode != null && firstValidNode.role !== undefined &&
          firstValidNode.root !== undefined) {
        return firstValidNode;
      }
      // If we have to walk up to an ancestor, reset the index to NODE_INDEX.
      this.index_ = cursors.NODE_INDEX;
    }
    return null;
  },

  /**
   * @return {number}
   */
  get index() {
    return this.index_;
  },

  /**
   * An index appropriate for making selections.
   * @return {number}
   * @private
   */
  get selectionIndex_() {
    if (this.index_ == cursors.NODE_INDEX)
      return cursors.NODE_INDEX;

    var adjustedIndex = this.index_;

    if (this.node.role == RoleType.INLINE_TEXT_BOX) {
      var sibling = this.node.previousSibling;
      while (sibling) {
        adjustedIndex += sibling.name.length;
        sibling = sibling.previousSibling;
      }
    }

    return adjustedIndex;
  },

  /**
   * Gets the accessible text of the node associated with this cursor.
   *
   * @param {!AutomationNode=} opt_node Use this node rather than this cursor's
   * node.
   * @return {string}
   */
  getText: function(opt_node) {
    var node = opt_node || this.node;
    if (node.role === RoleType.TEXT_FIELD)
      return node.value;
    return node.name || '';
  },

  /**
   * Makes a Cursor which has been moved from this cursor by the unit in the
   * given direction using the given movement type.
   * @param {Unit} unit
   * @param {Movement} movement
   * @param {Dir} dir
   * @return {!cursors.Cursor} The moved cursor.
   */
  move: function(unit, movement, dir) {
    var originalNode = this.node;
    if (!originalNode)
      return this;

    var newNode = originalNode;
    var newIndex = this.index_;

    if ((unit != Unit.NODE || unit != Unit.DOM_NODE) &&
        newIndex === cursors.NODE_INDEX)
      newIndex = 0;

    switch (unit) {
      case Unit.CHARACTER:
        // BOUND and DIRECTIONAL are the same for characters.
        var text = this.getText();
        newIndex = dir == Dir.FORWARD ?
            StringUtil.nextCodePointOffset(text, newIndex) :
            StringUtil.previousCodePointOffset(text, newIndex);
        if (newIndex < 0 || newIndex >= text.length) {
          newNode = AutomationUtil.findNextNode(
              newNode, dir, AutomationPredicate.leafWithText);
          if (newNode) {
            var newText = this.getText(newNode);
            newIndex = dir == Dir.FORWARD ?
                0 :
                StringUtil.previousCodePointOffset(newText, newText.length);
            newIndex = Math.max(newIndex, 0);
          } else {
            newIndex = this.index_;
          }
        }
        break;
      case Unit.WORD:
        if (newNode.role != RoleType.INLINE_TEXT_BOX) {
          newNode = AutomationUtil.findNextNode(
                        newNode, Dir.FORWARD, AutomationPredicate.inlineTextBox,
                        {skipInitialSubtree: false}) ||
              newNode;
        }
        switch (movement) {
          case Movement.BOUND:
            if (newNode.role == RoleType.INLINE_TEXT_BOX) {
              var start, end;
              for (var i = 0; i < newNode.wordStarts.length; i++) {
                if (newIndex >= newNode.wordStarts[i] &&
                    newIndex <= newNode.wordEnds[i]) {
                  start = newNode.wordStarts[i];
                  end = newNode.wordEnds[i];
                  break;
                }
              }
              if (goog.isDef(start) && goog.isDef(end))
                newIndex = dir == Dir.FORWARD ? end : start;
            } else {
              // TODO(dtseng): Figure out what to do in this case.
            }
            break;
          case Movement.DIRECTIONAL:
            if (newNode.role == RoleType.INLINE_TEXT_BOX) {
              var start, end;
              for (var i = 0; i < newNode.wordStarts.length; i++) {
                if (newIndex >= newNode.wordStarts[i] &&
                    newIndex <= newNode.wordEnds[i]) {
                  var nextIndex = dir == Dir.FORWARD ? i + 1 : i - 1;
                  start = newNode.wordStarts[nextIndex];
                  end = newNode.wordEnds[nextIndex];
                  break;
                }
              }
              if (goog.isDef(start)) {
                newIndex = start;
              } else {
                // The backward case is special at the beginning of nodes.
                if (dir == Dir.BACKWARD && newIndex != 0) {
                  newIndex = 0;
                } else {
                  newNode = AutomationUtil.findNextNode(
                      newNode, dir, AutomationPredicate.leaf);
                  if (newNode) {
                    newIndex = 0;
                    if (dir == Dir.BACKWARD &&
                        newNode.role == RoleType.INLINE_TEXT_BOX) {
                      var starts = newNode.wordStarts;
                      newIndex = starts[starts.length - 1] || 0;
                    } else {
                      // TODO(dtseng): Figure out what to do for general nodes.
                    }
                  }
                }
              }
            } else {
              // TODO(dtseng): Figure out what to do in this case.
            }
        }
        break;
      case Unit.NODE:
      case Unit.DOM_NODE:
        switch (movement) {
          case Movement.BOUND:
            newIndex = dir == Dir.FORWARD ? this.getText().length - 1 : 0;
            break;
          case Movement.DIRECTIONAL:
            var pred = unit == Unit.NODE ? AutomationPredicate.leaf :
                                           AutomationPredicate.object;
            newNode =
                AutomationUtil.findNextNode(newNode, dir, pred) || originalNode;
            newIndex = cursors.NODE_INDEX;
            break;
        }
        break;
      case Unit.LINE:
        newIndex = 0;
        switch (movement) {
          case Movement.BOUND:
            newNode = AutomationUtil.findNodeUntil(
                newNode, dir, AutomationPredicate.linebreak, true);
            newNode = newNode || originalNode;
            newIndex = dir == Dir.FORWARD ? this.getText(newNode).length : 0;
            break;
          case Movement.DIRECTIONAL:
            newNode = AutomationUtil.findNodeUntil(
                newNode, dir, AutomationPredicate.linebreak);
            break;
        }
        break;
      default:
        throw Error('Unrecognized unit: ' + unit);
    }
    newNode = newNode || originalNode;
    newIndex = goog.isDef(newIndex) ? newIndex : this.index_;
    return new cursors.Cursor(newNode, newIndex);
  },

  /**
   * Returns whether this cursor points to a valid position.
   * @return {boolean}
   */
  isValid: function() {
    return this.node != null;
  }
};

/**
 * A cursors.Cursor that wraps from beginning to end and vice versa when moved.
 * @constructor
 * @param {!AutomationNode} node
 * @param {number} index A 0-based index into this cursor node's primary
 * accessible name. An index of |cursors.NODE_INDEX| means the node as a whole
 * is pointed to and covers the case where the accessible text is empty.
 * @extends {cursors.Cursor}
 */
cursors.WrappingCursor = function(node, index) {
  cursors.Cursor.call(this, node, index);
};


/**
 * Convenience method to construct a Cursor from a node.
 * @param {!AutomationNode} node
 * @return {!cursors.WrappingCursor}
 */
cursors.WrappingCursor.fromNode = function(node) {
  return new cursors.WrappingCursor(node, cursors.NODE_INDEX);
};

cursors.WrappingCursor.prototype = {
  __proto__: cursors.Cursor.prototype,

  /** @override */
  move: function(unit, movement, dir) {
    var result = this;
    if (!result.node)
      return this;

    // Regular movement.
    if (!AutomationPredicate.root(this.node) || dir == Dir.FORWARD)
      result = cursors.Cursor.prototype.move.call(this, unit, movement, dir);

    // There are two cases for wrapping:
    // 1. moving forwards from the last element.
    // 2. moving backwards from the first element.
    // Both result in |move| returning the same cursor.
    // For 1, simply place the new cursor on the document node.
    // For 2, place range on the root (if not already there). If at root,
    // try to descend to the first leaf-like object.
    if (movement == Movement.DIRECTIONAL && result.equals(this)) {
      var pred = unit == Unit.DOM_NODE ? AutomationPredicate.object :
                                         AutomationPredicate.leaf;
      var endpoint = this.node;
      if (!endpoint)
        return this;

      // Case 1: forwards (find the root-like node).
      while (!AutomationPredicate.root(endpoint) && endpoint.parent)
        endpoint = endpoint.parent;

      // Always play a wrap earcon when moving forward.
      var playEarcon = dir == Dir.FORWARD;

      // Case 2: backward (sync downwards to a leaf), if already on the root.
      if (dir == Dir.BACKWARD && endpoint == this.node) {
        playEarcon = true;
        endpoint = AutomationUtil.findNodePre(endpoint, dir, function(n) {
          return pred(n) && !AutomationPredicate.shouldIgnoreNode(n);
        }) || endpoint;
      }

      if (playEarcon)
        cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.WRAP);

      return new cursors.WrappingCursor(endpoint, cursors.NODE_INDEX);
    }
    return new cursors.WrappingCursor(result.node, result.index);
  }
};

/**
 * Represents a range in the automation tree. There is no visible selection on
 * the page caused by usage of this object.
 * It is assumed that the caller provides |start| and |end| in document order.
 * @param {!cursors.Cursor} start
 * @param {!cursors.Cursor} end
 * @constructor
 */
cursors.Range = function(start, end) {
  /** @type {!cursors.Cursor} @private */
  this.start_ = start;
  /** @type {!cursors.Cursor} @private */
  this.end_ = end;
};

/**
 * Convenience method to construct a Range surrounding one node.
 * @param {!AutomationNode} node
 * @return {!cursors.Range}
 */
cursors.Range.fromNode = function(node) {
  var cursor = cursors.WrappingCursor.fromNode(node);
  return new cursors.Range(cursor, cursor);
};

/**
 * Given |rangeA| and |rangeB| in order, determine which |Dir|
 * relates them.
 * @param {!cursors.Range} rangeA
 * @param {!cursors.Range} rangeB
 * @return {Dir}
 */
cursors.Range.getDirection = function(rangeA, rangeB) {
  if (!rangeA || !rangeB)
    return Dir.FORWARD;

  if (!rangeA.start.node || !rangeA.end.node || !rangeB.start.node ||
      !rangeB.end.node)
    return Dir.FORWARD;

  // They are the same range.
  if (rangeA.start.node === rangeB.start.node &&
      rangeB.end.node === rangeA.end.node)
    return Dir.FORWARD;

  var testDirA =
      AutomationUtil.getDirection(rangeA.start.node, rangeB.end.node);
  var testDirB =
      AutomationUtil.getDirection(rangeB.start.node, rangeA.end.node);

  // The two ranges are either partly overlapping or non overlapping.
  if (testDirA == Dir.FORWARD && testDirB == Dir.BACKWARD)
    return Dir.FORWARD;
  else if (testDirA == Dir.BACKWARD && testDirB == Dir.FORWARD)
    return Dir.BACKWARD;
  else
    return testDirA;
};

cursors.Range.prototype = {
  /**
   * Returns true if |rhs| is equal to this range.
   * @param {!cursors.Range} rhs
   * @return {boolean}
   */
  equals: function(rhs) {
    return this.start_.equals(rhs.start) && this.end_.equals(rhs.end);
  },

  /**
   * Gets a cursor bounding this range.
   * @param {Dir} dir Which endpoint cursor to return; Dir.FORWARD for end,
   * Dir.BACKWARD for start.
   * @param {boolean=} opt_reverse Specify to have Dir.BACKWARD return end,
   * Dir.FORWARD return start.
   * @return {!cursors.Cursor}
   */
  getBound: function(dir, opt_reverse) {
    if (opt_reverse)
      return dir == Dir.BACKWARD ? this.end_ : this.start_;
    return dir == Dir.FORWARD ? this.end_ : this.start_;
  },

  /**
   * @return {!cursors.Cursor}
   */
  get start() {
    return this.start_;
  },

  /**
   * @return {!cursors.Cursor}
   */
  get end() {
    return this.end_;
  },

  /**
   * Returns true if this range covers less than a node.
   * @return {boolean}
   */
  isSubNode: function() {
    return this.start.node === this.end.node && this.start.index > -1 &&
        this.end.index > -1;
  },

  /**
   * Makes a Range which has been moved from this range by the given unit and
   * direction.
   * @param {Unit} unit
   * @param {Dir} dir
   * @return {cursors.Range}
   */
  move: function(unit, dir) {
    var newStart = this.start_;
    if (!newStart.node)
      return this;

    var newEnd;
    switch (unit) {
      case Unit.CHARACTER:
        newStart = newStart.move(unit, Movement.DIRECTIONAL, dir);
        newEnd = newStart.move(unit, Movement.DIRECTIONAL, Dir.FORWARD);
        // Character crossed a node; collapses to the end of the node.
        if (newStart.node !== newEnd.node)
          newEnd = new cursors.Cursor(newStart.node, newStart.index + 1);
        break;
      case Unit.WORD:
      case Unit.LINE:
        newStart = newStart.move(unit, Movement.DIRECTIONAL, dir);
        newStart = newStart.move(unit, Movement.BOUND, Dir.BACKWARD);
        newEnd = newStart.move(unit, Movement.BOUND, Dir.FORWARD);
        break;
      case Unit.NODE:
      case Unit.DOM_NODE:
        newStart = newStart.move(unit, Movement.DIRECTIONAL, dir);
        newEnd = newStart;
        break;
      default:
        throw Error('Invalid unit: ' + unit);
    }
    return new cursors.Range(newStart, newEnd);
  },

  /**
   * Select the text contained within this range.
   */
  select: function() {
    var start = this.start.node;
    var end = this.end.node;

    if (!start || !end)
      return;

    // Find the most common root.
    var uniqueAncestors = AutomationUtil.getUniqueAncestors(start, end);
    var mcr = start.root;
    if (uniqueAncestors) {
      var common = uniqueAncestors.pop().parent;
      if (common)
        mcr = common.root;
    }

    if (!mcr || mcr.role == RoleType.DESKTOP)
      return;

    if (mcr === start.root && mcr === end.root) {
      start = start.role == RoleType.INLINE_TEXT_BOX ? start.parent : start;
      end = end.role == RoleType.INLINE_TEXT_BOX ? end.parent : end;

      if (!start || !end)
        return;

      chrome.automation.setDocumentSelection({
        anchorObject: start,
        anchorOffset: this.start.selectionIndex_,
        focusObject: end,
        focusOffset: this.end.selectionIndex_
      });
    }
  },

  /**
   * Returns true if this range has either cursor end on web content.
   * @return {boolean}
   */
  isWebRange: function() {
    return this.isValid() &&
        (this.start.node.root.role != RoleType.DESKTOP ||
         this.end.node.root.role != RoleType.DESKTOP);
  },

  /**
   * Returns whether this range has valid start and end cursors.
   * @return {boolean}
   */
  isValid: function() {
    return this.start.isValid() && this.end.isValid();
  }
};
});  // goog.scope
