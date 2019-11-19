// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A search Widget presenting a list of nodes with the ability
 * to sync selection when chosen.
 */

goog.provide('cvox.NodeSearchWidget');

goog.require('cvox.ChromeVox');
goog.require('cvox.DomUtil');
goog.require('cvox.SearchWidget');


/**
 * @constructor
 * @param {string} typeMsg A message id identifying the type of items
 * contained in the list.
 * @param {?function(Array<Node>)} predicate A predicate; if null, no predicate
 * applies.
 * @extends {cvox.SearchWidget}
 */
cvox.NodeSearchWidget = function(typeMsg, predicate) {
  this.typeMsg_ = typeMsg;
  this.predicate_ = predicate;
  goog.base(this);
};
goog.inherits(cvox.NodeSearchWidget, cvox.SearchWidget);


/**
 * @override
 */
cvox.NodeSearchWidget.prototype.getNameMsg = function() {
  return ['choice_widget_name', [Msgs.getMsg(this.typeMsg_)]];
};


/**
 * @override
 */
cvox.NodeSearchWidget.prototype.getHelpMsg = function() {
  return 'choice_widget_help';
};


/**
 * @override
 */
cvox.NodeSearchWidget.prototype.getPredicate = function() {
  return this.predicate_;
};


/**
 * Shows a list generated dynamic satisfying some predicate.
 * @param {string} typeMsg The message id of the type contained in nodes.
 * @param {function(Array<Node>)} predicate The predicate.
 * @return {cvox.NodeSearchWidget} The widget.
 */
cvox.NodeSearchWidget.create = function(typeMsg, predicate) {
  return new cvox.NodeSearchWidget(typeMsg, predicate);
};
