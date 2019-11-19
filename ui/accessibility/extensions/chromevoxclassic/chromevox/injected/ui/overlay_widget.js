// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A widget hosting an HTML snippet.
 */

goog.provide('cvox.OverlayWidget');

goog.require('cvox.DomUtil');
goog.require('cvox.SearchWidget');


/**
 * @param {string} snippet The HTML snippet to render.
 * @constructor
 * @extends {cvox.SearchWidget}
 */
cvox.OverlayWidget = function(snippet) {
  goog.base(this);
  this.snippet_ = snippet;
};
goog.inherits(cvox.OverlayWidget, cvox.SearchWidget);


/**
 * @override
 */
cvox.OverlayWidget.prototype.show = function() {
  goog.base(this, 'show');
  var host = document.createElement('DIV');
  host.innerHTML = this.snippet_;

  // Position the overlay over the current ChromeVox selection.
  var hitPoint = cvox.DomUtil.elementToPoint(
      cvox.ChromeVox.navigationManager.getCurrentNode());
  host.style.position = 'absolute';
  host.style.left = String(hitPoint.x);
  host.style.top = String(hitPoint.y);

  document.body.appendChild(host);
  cvox.ChromeVox.navigationManager.updateSelToArbitraryNode(host);
  this.host_ = host;
};


/**
 * @override
 */
cvox.OverlayWidget.prototype.hide = function(opt_noSync) {
  this.host_.remove();
  goog.base(this, 'hide');
};


/**
 * @override
 */
cvox.OverlayWidget.prototype.onKeyDown = function(evt) {
  // Allow the base class to handle all keys first.
  goog.base(this, 'onKeyDown', evt);

  // Do not interfere with any key that exits the widget.
  if (evt.keyCode == 13 || evt.keyCode == 27) { // Enter or escape.
    return true;
  }

  // Bound navigation within the snippet for any other key.
  var r = cvox.ChromeVox.navigationManager.isReversed();
  if (!cvox.DomUtil.isDescendantOfNode(
      cvox.ChromeVox.navigationManager.getCurrentNode(), this.host_)) {
    if (r) {
      cvox.ChromeVox.navigationManager.syncToBeginning();
    } else {
      cvox.ChromeVox.navigationManager.updateSelToArbitraryNode(this.host_);
    }
    this.onNavigate();
    cvox.ChromeVox.navigationManager.speakDescriptionArray(
        cvox.ChromeVox.navigationManager.getDescription(),
        cvox.QueueMode.FLUSH, null);
  }
  return false;
};
