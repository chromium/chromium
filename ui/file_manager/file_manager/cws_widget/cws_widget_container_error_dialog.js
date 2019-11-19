// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class CWSWidgetContainerErrorDialog extends cr.ui.dialogs.BaseDialog {
  /**
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   */
  constructor(parentNode) {
    super(parentNode);
  }

  /**
   * Whether the dialog is showm.
   * @return {boolean}
   */
  shown() {
    return this.container.classList.contains('shown');
  }

  /**
   * One-time initialization of DOM.
   * @protected
   * @override
   */
  initDom() {
    super.initDom();
    this.frame.classList.add('cws-widget-error-dialog-frame');
    const img = this.document_.createElement('div');
    img.className = 'cws-widget-error-dialog-img';
    this.frame.insertBefore(img, this.text);

    this.title.hidden = true;
    this.closeButton.hidden = true;
    this.cancelButton.hidden = true;
    this.text.classList.add('cws-widget-error-dialog-text');

    // Don't allow OK button to lose focus, in order to prevent webview content
    // from stealing focus.
    // BaseDialog keeps focus by removing all other focusable elements from tab
    // order (by setting their tabIndex to -1). This doesn't work for webviews
    // because the webview embedder cannot access the webview DOM tree, and thus
    // fails to remove elements in the webview from tab order.
    this.okButton.addEventListener('blur', this.refocusOkButton_.bind(this));
  }

  /**
   * Focuses OK button.
   * @private
   */
  refocusOkButton_() {
    if (this.shown()) {
      this.okButton.focus();
    }
  }
}
