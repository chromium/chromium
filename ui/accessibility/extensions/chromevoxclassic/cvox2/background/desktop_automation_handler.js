// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Handles automation from a desktop automation node.
 */

goog.provide('DesktopAutomationHandler');

goog.require('AutomationObjectConstructorInstaller');
goog.require('BaseAutomationHandler');
goog.require('ChromeVoxState');
goog.require('editing.TextEditHandler');

goog.scope(function() {
var AutomationEvent = chrome.automation.AutomationEvent;
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;
var EventType = chrome.automation.EventType;
var RoleType = chrome.automation.RoleType;

/**
 * @param {!AutomationNode} node
 * @constructor
 * @extends {BaseAutomationHandler}
 */
DesktopAutomationHandler = function(node) {
  BaseAutomationHandler.call(this, node);

  /**
   * The object that speaks changes to an editable text field.
   * @type {editing.TextEditHandler}
   */
  this.textEditHandler_ = null;

  /**
   * The last time we handled a value changed event.
   * @type {!Date}
   * @private
   */
  this.lastValueChanged_ = new Date(0);

  var e = EventType;
  this.addListener_(e.ACTIVEDESCENDANTCHANGED, this.onActiveDescendantChanged);
  this.addListener_(e.ALERT, this.onAlert);
  this.addListener_(e.ARIA_ATTRIBUTE_CHANGED, this.onEventIfInRange);
  this.addListener_(e.CHECKED_STATE_CHANGED, this.onEventIfInRange);
  this.addListener_(e.FOCUS, this.onFocus);
  this.addListener_(e.HOVER, this.onHover);
  this.addListener_(e.LOAD_COMPLETE, this.onLoadComplete);
  this.addListener_(e.MENU_END, this.onMenuEnd);
  this.addListener_(e.MENU_LIST_ITEM_SELECTED, this.onEventIfSelected);
  this.addListener_(e.MENU_START, this.onMenuStart);
  this.addListener_(e.SCROLL_POSITION_CHANGED, this.onScrollPositionChanged);
  this.addListener_(e.SELECTION, this.onSelection);
  this.addListener_(e.TEXT_CHANGED, this.onTextChanged);
  this.addListener_(e.TEXT_SELECTION_CHANGED, this.onTextSelectionChanged);
  this.addListener_(e.VALUE_CHANGED, this.onValueChanged);

  AutomationObjectConstructorInstaller.init(node, function() {
    chrome.automation.getFocus(
        (function(focus) {
          if (ChromeVoxState.instance.mode != ChromeVoxMode.FORCE_NEXT)
            return;

          if (focus) {
            this.onFocus(
                new chrome.automation.AutomationEvent(EventType.FOCUS, focus));
          }
        }).bind(this));
  }.bind(this));
};

/**
 * Time to wait until processing more value changed events.
 * @const {number}
 */
DesktopAutomationHandler.VMIN_VALUE_CHANGE_DELAY_MS = 500;

DesktopAutomationHandler.prototype = {
  __proto__: BaseAutomationHandler.prototype,

  /** @override */
  willHandleEvent_: function(evt) {
    return !cvox.ChromeVox.isActive;
  },

  /**
   * Provides all feedback once ChromeVox's focus changes.
   * @param {!AutomationEvent} evt
   */
  onEventDefault: function(evt) {
    var node = evt.target;
    if (!node)
      return;

    var prevRange = ChromeVoxState.instance.currentRange;

    ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(node));

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (node.root.role != RoleType.DESKTOP &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC) {
      if (cvox.ChromeVox.isChromeOS)
        chrome.accessibilityPrivate.setFocusRing([]);
      return;
    }

    // Don't output if focused node hasn't changed.
    if (prevRange && evt.type == 'focus' &&
        ChromeVoxState.instance.currentRange.equals(prevRange))
      return;

    var output = new Output();
    output.withRichSpeech(
        ChromeVoxState.instance.currentRange, prevRange, evt.type);
    if (!this.textEditHandler_) {
      output.withBraille(
          ChromeVoxState.instance.currentRange, prevRange, evt.type);
    } else {
      // Delegate event handling to the text edit handler for braille.
      this.textEditHandler_.onEvent(evt);
    }
    output.go();
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onEventIfInRange: function(evt) {
    // TODO(dtseng): Consider the end of the current range as well.
    if (AutomationUtil.isDescendantOf(
            ChromeVoxState.instance.currentRange.start.node, evt.target) ||
        evt.target.state.focused)
      this.onEventDefault(evt);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onEventIfSelected: function(evt) {
    if (evt.target.state.selected)
      this.onEventDefault(evt);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onEventWithFlushedOutput: function(evt) {
    Output.flushNextSpeechUtterance();
    this.onEventDefault(evt);
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onHover: function(evt) {
    if (ChromeVoxState.instance.currentRange &&
        evt.target == ChromeVoxState.instance.currentRange.start.node)
      return;
    Output.flushNextSpeechUtterance();
    this.onEventDefault(evt);
  },

  /**
   * Makes an announcement without changing focus.
   * @param {!AutomationEvent} evt
   */
  onActiveDescendantChanged: function(evt) {
    if (!evt.target.activeDescendant)
      return;
    this.onEventDefault(new chrome.automation.AutomationEvent(
        EventType.FOCUS, evt.target.activeDescendant));
  },

  /**
   * Makes an announcement without changing focus.
   * @param {!AutomationEvent} evt
   */
  onAlert: function(evt) {
    var node = evt.target;
    if (!node)
      return;

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (node.root.role != RoleType.DESKTOP &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC) {
      return;
    }

    var range = cursors.Range.fromNode(node);

    new Output().withSpeechAndBraille(range, null, evt.type).go();
  },

  /**
   * Provides all feedback once a focus event fires.
   * @param {!AutomationEvent} evt
   */
  onFocus: function(evt) {
    // Invalidate any previous editable text handler state.
    this.textEditHandler_ = null;

    var node = evt.target;

    // Discard focus events on embeddedObject and client nodes.
    if (node.role == RoleType.EMBEDDED_OBJECT || node.role == RoleType.CLIENT)
      return;

    this.createTextEditHandlerIfNeeded_(evt.target);

    // Since we queue output mostly for live regions support and there isn't a
    // reliable way to know if this focus event resulted from a user's explicit
    // action, only flush when the focused node is not web content.
    if (node.root.role == RoleType.DESKTOP)
      Output.flushNextSpeechUtterance();

    this.onEventDefault(
        new chrome.automation.AutomationEvent(EventType.FOCUS, node));
  },

  /**
   * Provides all feedback once a load complete event fires.
   * @param {!AutomationEvent} evt
   */
  onLoadComplete: function(evt) {
    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.DESKTOP &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    chrome.automation.getFocus(function(focus) {
      if (!focus || !AutomationUtil.isDescendantOf(focus, evt.target))
        return;

      // If initial focus was already placed on this page (e.g. if a user starts
      // tabbing before load complete), then don't move ChromeVox's position on
      // the page.
      if (ChromeVoxState.instance.currentRange &&
          ChromeVoxState.instance.currentRange.start.node.root == focus.root)
        return;

      var o = new Output();
      if (focus.role == RoleType.ROOT_WEB_AREA) {
        // Restore to previous position.
        var url = focus.docUrl;
        url = url.substring(0, url.indexOf('#')) || url;
        var pos = cvox.ChromeVox.position[url];
        if (pos) {
          focus = AutomationUtil.hitTest(focus.root, pos) || focus;
          if (focus != focus.root)
            o.format('$name', focus.root);
        }
      }
      ChromeVoxState.instance.setCurrentRange(cursors.Range.fromNode(focus));
      o.withRichSpeechAndBraille(
           ChromeVoxState.instance.currentRange, null, evt.type)
          .go();
    }.bind(this));
  },

  /**
   * Provides all feedback once a text changed event fires.
   * @param {!AutomationEvent} evt
   */
  onTextChanged: function(evt) {
    if (evt.target.state.editable)
      this.onEditableChanged_(evt);
  },

  /**
   * Provides all feedback once a text selection changed event fires.
   * @param {!AutomationEvent} evt
   */
  onTextSelectionChanged: function(evt) {
    if (evt.target.state.editable)
      this.onEditableChanged_(evt);
  },

  /**
   * Provides all feedback once a change event in a text field fires.
   * @param {!AutomationEvent} evt
   * @private
   */
  onEditableChanged_: function(evt) {
    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.DESKTOP &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    if (!evt.target.state.focused)
      return;

    if (!ChromeVoxState.instance.currentRange) {
      this.onEventDefault(evt);
      ChromeVoxState.instance.setCurrentRange(
          cursors.Range.fromNode(evt.target));
    }

    this.createTextEditHandlerIfNeeded_(evt.target);
    // TODO(plundblad): This can currently be null for contenteditables.
    // Clean up when it can't.
    if (this.textEditHandler_)
      this.textEditHandler_.onEvent(evt);
  },

  /**
   * Provides all feedback once a value changed event fires.
   * @param {!AutomationEvent} evt
   */
  onValueChanged: function(evt) {
    // Delegate to the edit text handler if this is an editable.
    if (evt.target.state.editable) {
      this.onEditableChanged_(evt);
      return;
    }

    // Don't process nodes inside of web content if ChromeVox Next is inactive.
    if (evt.target.root.role != RoleType.DESKTOP &&
        ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    var t = evt.target;
    if (t.state.focused || t.root.role == RoleType.DESKTOP ||
        AutomationUtil.isDescendantOf(
            ChromeVoxState.instance.currentRange.start.node, t)) {
      if (new Date() - this.lastValueChanged_ <=
          DesktopAutomationHandler.VMIN_VALUE_CHANGE_DELAY_MS)
        return;

      this.lastValueChanged_ = new Date();

      new Output().format('$value', evt.target).go();
    }
  },

  /**
   * Handle updating the active indicator when the document scrolls.
   * @param {!AutomationEvent} evt
   */
  onScrollPositionChanged: function(evt) {
    if (ChromeVoxState.instance.mode === ChromeVoxMode.CLASSIC)
      return;

    var currentRange = ChromeVoxState.instance.currentRange;
    if (currentRange && currentRange.isValid())
      new Output().withLocation(currentRange, null, evt.type).go();
  },

  /**
   * @param {!AutomationEvent} evt
   */
  onSelection: function(evt) {
    chrome.automation.getFocus(function(focus) {
      // Some cases (e.g. in overview mode), require overriding the assumption
      // that focus is an ancestor of a selection target.
      var override = evt.target.role == RoleType.MENU_ITEM ||
          (evt.target.root == focus.root &&
           focus.root.role == RoleType.DESKTOP);
      Output.flushNextSpeechUtterance();
      if (override || AutomationUtil.isDescendantOf(evt.target, focus))
        this.onEventDefault(evt);
    }.bind(this));
  },

  /**
   * Provides all feedback once a menu start event fires.
   * @param {!AutomationEvent} evt
   */
  onMenuStart: function(evt) {
    ChromeVoxState.instance.startExcursion();
    this.onEventDefault(evt);
  },

  /**
   * Provides all feedback once a menu end event fires.
   * @param {!AutomationEvent} evt
   */
  onMenuEnd: function(evt) {
    this.onEventDefault(evt);
    ChromeVoxState.instance.endExcursion();
  },

  /**
   * Create an editable text handler for the given node if needed.
   * @param {!AutomationNode} node
   */
  createTextEditHandlerIfNeeded_: function(node) {
    if (!this.textEditHandler_ || this.textEditHandler_.node !== node) {
      this.textEditHandler_ = editing.TextEditHandler.createForNode(node);
    }
  }
};

/**
 * Initializes global state for DesktopAutomationHandler.
 * @private
 */
DesktopAutomationHandler.init_ = function() {
  if (cvox.ChromeVox.isMac)
    return;
  chrome.automation.getDesktop(function(desktop) {
    ChromeVoxState.desktopAutomationHandler =
        new DesktopAutomationHandler(desktop);
  });
};

DesktopAutomationHandler.init_();
});  // goog.scope
