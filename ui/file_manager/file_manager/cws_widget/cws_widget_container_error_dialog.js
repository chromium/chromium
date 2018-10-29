// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @param {HTMLElement} parentNode Node to be parent for this dialog.
 * @constructor
 * @extends {cr.ui.dialogs.BaseDialog}
 */
function CWSWidgetContainerErrorDialog(parentNode) {
  cr.ui.dialogs.BaseDialog.call(this, parentNode);
}

CWSWidgetContainerErrorDialog.prototype = {
  __proto__: cr.ui.dialogs.BaseDialog.prototype
};

/**
 * Whether the dialog is showm.
 * @return {boolean}
 */
CWSWidgetContainerErrorDialog.prototype.shown = function() {
  return this.container_.classList.contains('shown');
};

/**
 * One-time initialization of DOM.
 * @protected
 */
CWSWidgetContainerErrorDialog.prototype.initDom_ = function() {
  cr.ui.dialogs.BaseDialog.prototype.initDom_.call(this);
  this.frame_.classList.add('cws-widget-error-dialog-frame');
  var img = this.document_.createElement('div');
  img.className = 'cws-widget-error-dialog-img';
  this.frame_.insertBefore(img, this.text_);

  this.title_.hidden = true;
  this.closeButton_.hidden = true;
  this.cancelButton_.hidden = true;
  this.text_.classList.add('cws-widget-error-dialog-text');

  // Don't allow OK button to lose focus, in order to prevent webview content
  // from stealing focus.
  // BaseDialog keeps focus by removing all other focusable elements from tab
  // order (by setting their tabIndex to -1). This doesn't work for webviews
  // because the webview embedder cannot access the webview DOM tree, and thus
  // fails to remove elements in the webview from tab order.
  this.okButton_.addEventListener('blur', this.refocusOkButton_.bind(this));
};

/**
 * Focuses OK button.
 * @private
 */
CWSWidgetContainerErrorDialog.prototype.refocusOkButton_ = function() {
  if (this.shown())
    this.okButton_.focus();
};
