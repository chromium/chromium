// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview A bubble for displaying in-product help. This is a WIP, do not
 * use.
 */
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

export interface IPHBubbleElement {
  open: boolean;
  _setOpen(open: boolean): void;
}

/** @polymer */
export class IPHBubbleElement extends PolymerElement {
  static get is() {
    return 'iph-bubble';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      /**
       * The main promo text. Required.
       */
      body: {type: String, value: ''},

      /**
       * Readonly variable tracking whether the bubble is currently displayed.
       */
      open: {
        readOnly: true,
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  /**
   * Shows the bubble.
   */
  show() {
    // Reset the aria-hidden attribute as screen readers need to access the
    // contents of an opened bubble.
    this.removeAttribute('aria-hidden');
    this._setOpen(true);
  }

  /**
   * Hides the bubble and ensures that screen readers cannot its contents
   * while hidden.
   */
  hide() {
    this.setAttribute('aria-hidden', 'true');
    this._setOpen(false);
  }
}
customElements.define(IPHBubbleElement.is, IPHBubbleElement);
