// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A widget hosting an HTML <select> element.
 * In most cases, the browser's native key-driven usage model works for user
 * interaction and manipulation of a <select>. However, on platforms like Mac
 * OS X where <select> elements get their own renderer, users can still interact
 * with <select> elements via a ChromeVox overlay/context widget.
 */

goog.provide('cvox.SelectWidget');


goog.require('cvox.OverlayWidget');


/**
 * @param {Node} node The select node.
 * @constructor
 * @extends {cvox.OverlayWidget}
 */
cvox.SelectWidget = function(node) {
  goog.base(this, '');
  this.selectNode_ = node;
};
goog.inherits(cvox.SelectWidget, cvox.OverlayWidget);


/**
 * @override
 */
cvox.SelectWidget.prototype.show = function() {
  goog.base(this, 'show');
  var container = document.createElement('div');
  container.setAttribute('role', 'menu');
  for (var i = 0, item = null; item = this.selectNode_.options[i]; i++) {
    var newItem = document.createElement('p');
    newItem.innerHTML = item.innerHTML;
    newItem.id = i;
    newItem.setAttribute('role', 'menuitem');
    container.appendChild(newItem);
  }
  this.host_.appendChild(container);
  var currentSelection = this.selectNode_.selectedIndex;
  if (typeof(currentSelection) == 'number') {
    cvox.ChromeVox.syncToNode(container.children[currentSelection], true);
  }
};


/**
 * @override
 */
cvox.SelectWidget.prototype.hide = function(opt_noSync) {
  var evt = document.createEvent('Event');
  evt.initEvent('change', false, false);
  this.selectNode_.dispatchEvent(evt);
  goog.base(this, 'hide', true);
};


/**
 * @override
 */
cvox.SelectWidget.prototype.onNavigate = function() {
  var self = this;
  cvox.ChromeVoxEventSuspender.withSuspendedEvents(function() {
    var selectedIndex =
        cvox.ChromeVox.navigationManager.getCurrentNode().parentNode.id;
    self.selectNode_.selectedIndex = selectedIndex;
  })();
};


/**
 * @override
 */
cvox.SelectWidget.prototype.getNameMsg = function() {
  return ['role_menu'];
};
