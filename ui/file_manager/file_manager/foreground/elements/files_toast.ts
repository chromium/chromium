// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/ash/common/cr_elements/cr_shared_style.css.js';

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrToastElement} from 'chrome://resources/ash/common/cr_elements/cr_toast/cr_toast.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './files_toast.html.js';

interface FilesToastAction {
  text: string;
  callback?: () => void;
}

interface FilesToastData {
  text: string;
  action?: FilesToastAction;
}

export interface FilesToast {
  $: {
    container: CrToastElement,
    text: HTMLDivElement,
    action: CrButtonElement,
  };
}

/**
 * Files Toast.
 *
 * The toast is shown at the bottom-right in LTR, bottom-left in RTL. Usage:
 *
 * toast.show('Toast without action.');
 * toast.show('Toast with action', {text: 'Action', callback:function(){}});
 * toast.hide();
 */
export class FilesToast extends PolymerElement {
  static get is() {
    return 'files-toast';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      visible: {
        type: Boolean,
        value: false,
      },

    };
  }

  private action_: FilesToastAction|null = null;
  private queue_: FilesToastData[] = [];
  visible: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    this.$.container.ontransitionend = this.onTransitionEnd_.bind(this);
  }

  /**
   * Shows toast. If a toast is already shown, add the toast to the pending
   * queue. It will shown later when other toasts have completed.
   *
   * @param text Text of toast.
   * @param action Action. The |Action.callback| is called if the user taps or
   *     clicks the action button.
   */
  show(text: string, action?: FilesToastAction) {
    if (this.visible) {
      this.queue_.push({text: text, action: action});
      return;
    }

    this.visible = true;

    this.$.text.innerText = text;
    this.action_ = action || null;
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
  }

  /** Handles action button tap/click. */
  protected onActionClicked_() {
    if (this.action_ && this.action_.callback) {
      this.action_.callback();
      this.hide();
    }
  }

  /**
   * Handles the <cr-toast> transitionend event. On a hide transition, show
   * the next queued toast if any.
   */
  private onTransitionEnd_() {
    const hide = !this.$.container.open;

    if (hide && this.visible) {
      this.visible = false;

      if (this.queue_.length > 0) {
        const next = this.queue_.shift()!;
        setTimeout(this.show.bind(this), 0, next.text, next.action);
      }
    }
  }

  /**
   * Hides toast if visible.
   */
  hide() {
    if (this.visible) {
      this.$.container.hide();
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'files-toast': FilesToast;
  }
}

customElements.define(FilesToast.is, FilesToast);
