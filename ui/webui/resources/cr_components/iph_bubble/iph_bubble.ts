// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview A bubble for displaying in-product help. This is a WIP, do not
 * use.
 */
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const ANCHOR_HIGHLIGHT_CLASS = 'iph-anchor-highlight';

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
       * The id of the element that the iph is anchored to. This element
       * must be a sibling of the iph. If this property is not set,
       * then the iph will be anchored to the parent node containing it.
       */
      anchorId: {
        type: String,
        value: '',
      },

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

  anchorId?: string;

  /**
   * HTMLElement corresponding to |this.anchorId|.
   */
  private anchorElement_?: HTMLElement;

  /**
   * Shows the bubble.
   */
  show() {
    if (!this.anchorElement_) {
      this.anchorElement_ = this.findAnchorElement_();
    }
    // Reset the aria-hidden attribute as screen readers need to access the
    // contents of an opened bubble.
    this.removeAttribute('aria-hidden');
    this.highlightAnchor_();
    this._setOpen(true);
  }

  /**
   * Hides the bubble and ensures that screen readers cannot its contents
   * while hidden.
   */
  hide() {
    this.setAttribute('aria-hidden', 'true');
    this.unhighlightAnchor_();
    this._setOpen(false);
  }

  /**
   * Returns the element that this iph is anchored to. It is either the element
   * given by |this.anchorId|, or the immediate parent of the iph.
   */
  private findAnchorElement_(): HTMLElement {
    const parentNode: any = this.parentNode;
    if (this.anchorId) {
      return parentNode.querySelector(`#${this.anchorId}`);
    } else if (parentNode.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
      return parentNode.host;
    } else {
      return parentNode;
    }
  }

  /**
   * Styles the anchor element to appear highlighted while the bubble is open.
   */
  private highlightAnchor_() {
    if (!this.anchorElement_) {
      return;
    }
    this.anchorElement_.classList.add(ANCHOR_HIGHLIGHT_CLASS);
  }

  /**
   * Resets the anchor element to its original styling while the bubble is
   * closed.
   */
  private unhighlightAnchor_() {
    if (!this.anchorElement_) {
      return;
    }
    this.anchorElement_.classList.remove(ANCHOR_HIGHLIGHT_CLASS);
  }
}
customElements.define(IPHBubbleElement.is, IPHBubbleElement);
