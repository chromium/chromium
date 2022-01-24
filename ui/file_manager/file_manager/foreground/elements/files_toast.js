// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/shared_style_css.m.js';
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_toast/cr_toast.js';

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

/**
 * @typedef {{
 *   text:string,
 *   callback:(function()|undefined)
 * }}
 */
let FilesToastAction;

/**
 * @typedef {{
 *   text:string,
 *   action:(FilesToastAction|undefined)
 * }}
 */
let FilesToastData;

/**
 * Files Toast.
 *
 * The toast is shown at the bottom-right in LTR, bottom-left in RTL. Usage:
 *
 * toast.show('Toast without action.');
 * toast.show('Toast with action', {text: 'Action', callback:function(){}});
 * toast.hide();
 */
export const FilesToast = Polymer({
  _template: html`{__html_template__}`,

  is: 'files-toast',

  properties: {
    visible: {
      type: Boolean,
      readOnly: true,
      value: false,
    },
  },

  created() {
    /**
     * @private {?FilesToastAction}
     */
    this.action_ = null;

    /**
     * @private {!Array<!FilesToastData>}
     */
    this.queue_ = [];
  },

  attached() {
    this.$.container.ontransitionend = this.onTransitionEnd_.bind(this);
  },

  /**
   * Shows toast. If a toast is already shown, add the toast to the pending
   * queue. It will shown later when other toasts have completed.
   *
   * @param {string} text Text of toast.
   * @param {FilesToastAction=} opt_action Action. The |Action.callback| is
   *     called if the user taps or clicks the action button.
   */
  show(text, opt_action) {
    if (this.visible) {
      this.queue_.push({text: text, action: opt_action});
      return;
    }

    this._setVisible(true);

    this.$.text.innerText = text;
    this.action_ = opt_action || null;
    if (this.action_) {
      this.$.text.setAttribute('style', 'margin-inline-end: 0');
      this.$.action.innerText = this.action_.text;
      this.$.action.hidden = false;
    } else {
      this.$.text.removeAttribute('style');
      this.$.action.innerText = '';
      this.$.action.hidden = true;
    }

    this.$.container.show();
  },

  /**
   * Handles action button tap/click.
   *
   * @private
   */
  onActionTapped_() {
    if (this.action_ && this.action_.callback) {
      this.action_.callback();
      this.hide();
    }
  },

  /**
   * Handles the <cr-toast> transitionend event. On a hide transition, show
   * the next queued toast if any.
   *
   * @private
   */
  onTransitionEnd_() {
    const hide = !this.$.container.open;

    if (hide && this.visible) {
      this._setVisible(false);

      if (this.queue_.length > 0) {
        const next = this.queue_.shift();
        setTimeout(this.show.bind(this), 0, next.text, next.action);
      }
    }
  },

  /**
   * Hides toast if visible.
   */
  hide() {
    if (this.visible) {
      this.$.container.hide();
    }
  }
});

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_toast.js
