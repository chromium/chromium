// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Processes events related to editing text and emits the
 * appropriate spoken and braille feedback.
 */

goog.provide('editing.TextEditHandler');

goog.require('AutomationTreeWalker');
goog.require('AutomationUtil');
goog.require('Output');
goog.require('Output.EventType');
goog.require('cursors.Cursor');
goog.require('cursors.Range');
goog.require('cvox.ChromeVoxEditableTextBase');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Cursor = cursors.Cursor;
var Dir = AutomationUtil.Dir;
var EventType = chrome.automation.EventType;
var Range = cursors.Range;
var RoleType = chrome.automation.RoleType;
var Movement = cursors.Movement;
var Unit = cursors.Unit;

/**
 * A handler for automation events in a focused text field or editable root
 * such as a |contenteditable| subtree.
 * @constructor
 * @param {!AutomationNode} node
 */
editing.TextEditHandler = function(node) {
  /** @const {!AutomationNode} @private */
  this.node_ = node;
};

editing.TextEditHandler.prototype = {
  /** @return {!AutomationNode} */
  get node() {
    return this.node_;
  },

  /**
   * Receives the following kinds of events when the node provided to the
   * constructor is focuse: |focus|, |textChanged|, |textSelectionChanged| and
   * |valueChanged|.
   * An implementation of this method should emit the appropritate braille and
   * spoken feedback for the event.
   * @param {!AutomationEvent} evt
   */
  onEvent: goog.abstractMethod,
};

/**
 * A |TextEditHandler| suitable for text fields.
 * @constructor
 * @param {!AutomationNode} node A node with the role of |textField|
 * @extends {editing.TextEditHandler}
 */
function TextFieldTextEditHandler(node) {
  editing.TextEditHandler.call(this, node);
  /** @type {AutomationEditableText} @private */
  this.editableText_ = new AutomationEditableText(node);
}

TextFieldTextEditHandler.prototype = {
  __proto__: editing.TextEditHandler.prototype,

  /** @override */
  onEvent: function(evt) {
    if (evt.type !== EventType.TEXT_CHANGED &&
        evt.type !== EventType.TEXT_SELECTION_CHANGED &&
        evt.type !== EventType.VALUE_CHANGED && evt.type !== EventType.FOCUS)
      return;
    if (!evt.target.state.focused || !evt.target.state.editable ||
        evt.target != this.node_)
      return;

    this.editableText_.onUpdate();
  },
};

/**
 * A |ChromeVoxEditableTextBase| that implements text editing feedback
 * for automation tree text fields.
 * @constructor
 * @param {!AutomationNode} node
 * @extends {cvox.ChromeVoxEditableTextBase}
 */
function AutomationEditableText(node) {
  if (!node.state.editable)
    throw Error('Node must have editable state set to true.');
  var start = node.textSelStart;
  var end = node.textSelEnd;
  cvox.ChromeVoxEditableTextBase.call(
      this, node.value, Math.min(start, end), Math.max(start, end),
      node.state.protected /**password*/, cvox.ChromeVox.tts);
  /** @override */
  this.multiline = node.state.multiline || false;
  /** @type {!AutomationNode} @private */
  this.node_ = node;
}

AutomationEditableText.prototype = {
  __proto__: cvox.ChromeVoxEditableTextBase.prototype,

  /**
   * Called when the text field has been updated.
   */
  onUpdate: function() {
    var newValue = this.node_.value;
    var textChangeEvent = new cvox.TextChangeEvent(
        newValue, this.node_.textSelStart, this.node_.textSelEnd,
        true /* triggered by user */);
    this.changed(textChangeEvent);
    this.outputBraille_();
  },

  /** @override */
  getLineIndex: function(charIndex) {
    if (!this.multiline)
      return 0;
    var breaks = this.node_.lineBreaks || [];
    var index = 0;
    while (index < breaks.length && breaks[index] <= charIndex)
      ++index;
    return index;
  },

  /** @override */
  getLineStart: function(lineIndex) {
    if (!this.multiline || lineIndex == 0)
      return 0;
    var breaks = this.getLineBreaks_();
    return breaks[lineIndex - 1] || this.node_.value.length;
  },

  /** @override */
  getLineEnd: function(lineIndex) {
    var breaks = this.getLineBreaks_();
    var value = this.node_.value;
    if (lineIndex >= breaks.length)
      return value.length;
    var end = breaks[lineIndex];
    // A hard newline shouldn't be considered part of the line itself.
    if (0 < end && value[end - 1] == '\n')
      return end - 1;
    return end;
  },

  /**
   * @return {Array<number>}
   * @private
   */
  getLineBreaks_: function() {
    // node.lineBreaks is undefined when the multiline field has no line
    // breaks.
    return this.node_.lineBreaks || [];
  },

  /** @private */
  outputBraille_: function() {
    var isFirstLine = false;  // First line in a multiline field.
    var output = new Output();
    var range;
    if (this.multiline) {
      var lineIndex = this.getLineIndex(this.start);
      if (lineIndex == 0) {
        isFirstLine = true;
        output.formatForBraille('$name', this.node_);
      }
      range = new Range(
          new Cursor(this.node_, this.getLineStart(lineIndex)),
          new Cursor(this.node_, this.getLineEnd(lineIndex)));
    } else {
      range = Range.fromNode(this.node_);
    }
    output.withBraille(range, null, Output.EventType.NAVIGATE);
    if (isFirstLine)
      output.formatForBraille('@tag_textarea');
    output.go();
  }
};

/**
 * @param {!AutomationNode} node The root editable node, i.e. the root of a
 *     contenteditable subtree or a text field.
 * @return {editing.TextEditHandler}
 */
editing.TextEditHandler.createForNode = function(node) {
  var rootFocusedEditable = null;
  var testNode = node;

  do {
    if (testNode.state.focused && testNode.state.editable)
      rootFocusedEditable = testNode;
    testNode = testNode.parent;
  } while (testNode);

  if (rootFocusedEditable)
    return new TextFieldTextEditHandler(rootFocusedEditable);

  return null;
};
});
