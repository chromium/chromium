// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Objects related to incremental search.
 */

goog.provide('ISearch');
goog.provide('ISearchHandler');
goog.provide('ISearchUI');

goog.require('AutomationUtil');
goog.require('ChromeVoxState');
goog.require('Output');
goog.require('constants');
goog.require('cursors.Cursor');

goog.scope(function() {
var AutomationNode = chrome.automation.AutomationNode;
var Dir = constants.Dir;

/**
 * An interface implemented by objects that wish to handle events related to
 * incremental search.
 * @interface
 */
ISearchHandler = function() {};

ISearchHandler.prototype = {
  /**
   * Called when there are no remaining nodes in the document matching
   * search.
   * @param {!AutomationNode} boundaryNode The last node before reaching either
   * the start or end of the document.
   */
  onSearchReachedBoundary: function(boundaryNode) {},

  /**
   * Called when search result node changes.
   * @param {!AutomationNode} node The new search result.
   */
  onSearchResultChanged: function(node) {}
};

/**
 * Controls an incremental search.
 * @param {!AutomationNode} initialNode
 * @constructor
 */
ISearch = function(initialNode) {
  var leaf = AutomationUtil.findNodePre(
                 initialNode, Dir.FORWARD, AutomationPredicate.leaf) ||
      initialNode;

  /** @type {!AutomationNode} @private */
  this.node_ = leaf;

  /**
   * This tracks the id of a search that is in progress.
   * @type {number} @private
   */
  this.pendingSearchId_ = 0;

  // Global exports.
  /** Exported for this background script. */
  cvox.ChromeVox = chrome.extension.getBackgroundPage()['cvox']['ChromeVox'];
};

ISearch.prototype = {
  /**
   * @param {!ISearchHandler} handler
   */
  set handler(handler) {
    this.handler_ = handler;
  },

  /**
   * Performs a search.
   * @param {string} searchStr
   * @param {Dir} dir
   */
  search: function(searchStr, dir) {
    searchStr = searchStr.toLocaleLowerCase();
    // Keep things responsive by chunking cursor moves up into discrete
    // blocks. We can, if needed, simulate getting interrupted by the enter key.
    var currentSearchId = ++this.pendingSearchId_;
    var move = function(curNode) {
      var cur = cursors.Cursor.fromNode(curNode);
      var prev = cur;
      cur = cur.move(cursors.Unit.DOM_NODE, cursors.Movement.DIRECTIONAL, dir);
      if (prev.equals(cur)) {
        this.handler_.onSearchReachedBoundary(this.node_);
        return;
      }

      if (cur.getText().toLocaleLowerCase().indexOf(searchStr) != -1) {
        this.node_ = cur.node;
        this.handler_.onSearchResultChanged(this.node_);
        return;
      }
      if (this.pendingSearchId_ == currentSearchId)
        window.setTimeout(move.bind(this, cur.node), 0);
    };
    window.setTimeout(move.bind(this, this.node_), 0);
  }
};

/**
 * @param {Element} input
 * @constructor
 * @implements {ISearchHandler}
 */
ISearchUI = function(input) {
  /** @type {ChromeVoxState} */
  this.background_ =
      chrome.extension.getBackgroundPage()['ChromeVoxState']['instance'];
  this.iSearch_ = new ISearch(this.background_.currentRange.start.node);
  this.input_ = input;
  this.dir_ = Dir.FORWARD;
  this.iSearch_.handler = this;

  this.onKeyDown = this.onKeyDown.bind(this);
  this.onTextInput = this.onTextInput.bind(this);

  this.background_['startExcursion']();
  input.addEventListener('keydown', this.onKeyDown, true);
  input.addEventListener('textInput', this.onTextInput, true);
};

/**
 * @param {Element} input
 * @return {ISearchUI}
 */
ISearchUI.get = function(input) {
  if (ISearchUI.instance_)
    ISearchUI.instance_.destroy();
  ISearchUI.instance_ = new ISearchUI(input);
  input.focus();
  return ISearchUI.instance_;
};

ISearchUI.prototype = {
  /**
   * Listens to key down events.
   * @param {Event} evt
   * @return {boolean}
   */
  onKeyDown: function(evt) {
    switch (evt.key) {
      case 'ArrowUp':
        this.dir_ = Dir.BACKWARD;
        break;
      case 'ArrowDown':
        this.dir_ = Dir.FORWARD;
        break;
      case 'Escape':
        this.pendingSearchId_ = 0;
        this.background_['endExcursion']();
        Panel.closeMenusAndRestoreFocus();
        return false;
      case 'Enter':
        this.pendingSearchId_ = 0;
        this.background_['saveExcursion']();
        Panel.closeMenusAndRestoreFocus();
        return false;
      default:
        this.pendingSearchId_ = 0;
        return false;
    }
    this.iSearch_.search(this.input_.value, this.dir_);
    evt.preventDefault();
    evt.stopPropagation();
    return false;
  },

  /**
   * Listens to text input events.
   * @param {Event} evt
   * @return {boolean}
   */
  onTextInput: function(evt) {
    this.iSearch_.search(this.input_.value, this.dir_);
    return true;
  },

  /**
   * @override
   */
  onSearchReachedBoundary: function(boundaryNode) {
    this.output_(boundaryNode);
    cvox.ChromeVox.earcons.playEarcon(cvox.Earcon.WRAP);
  },

  /**
   * @override
   */
  onSearchResultChanged: function(node) {
    this.output_(node);
  },

  /**
   * @param {!AutomationNode} node
   * @private
   */
  output_: function(node) {
    Output.flushNextSpeechUtterance();
    var o =
        new Output()
            .withRichSpeechAndBraille(
                cursors.Range.fromNode(node), null, Output.EventType.NAVIGATE)
            .go();

    this.background_.setCurrentRange(cursors.Range.fromNode(node));
  },

  /** Unregisters event handlers. */
  destroy: function() {
    this.iSearch_.handler_ = null;
    this.iSearch_ = null;
    var input = this.input_;
    this.input_ = null;
    input.removeEventListener('keydown', this.onKeyDown, true);
    input.removeEventListener('textInput', this.onTextInput, true);
  }
};
});  // goog.scope
