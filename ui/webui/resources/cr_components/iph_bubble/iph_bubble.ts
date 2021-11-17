// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview A bubble for displaying in-product help. This is a WIP, do not
 * use.
 */
import {assert} from '//resources/js/assert.m.js';
import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const ANCHOR_HIGHLIGHT_CLASS = 'iph-anchor-highlight';

enum Position {
  Above = 'above',
  Below = 'below',
  Left = 'left',
  Right = 'right',
}

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
      anchorId: {type: String, value: ''},

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

      /**
       * Determines position relative to the anchor element, and where the
       * bubble's arrow points. Must be one of 'above', 'below', 'left' or
       * 'right'. Required.
       */
      position: {type: Position, value: Position.Above},
    };
  }

  anchorId: string;
  position: Position;

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
    this.updatePosition_();
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
   * Sets the bubble position, as relative to that of the anchor element and
   * |this.position|.
   */
  private updatePosition_() {
    assert(Object.values(Position).includes(this.position));
    if (!this.anchorElement_) {
      return;
    }
    // Inclusive of 8px visible arrow and 8px margin.
    const offset = 16;
    const parentRect = this.offsetParent!.getBoundingClientRect();
    const anchorRect = this.anchorElement_.getBoundingClientRect();
    const anchorLeft = anchorRect.left - parentRect.left;
    const anchorTop = anchorRect.top - parentRect.top;
    let iphLeft: string, iphTop: string, iphTransform: string;
    switch (this.position) {
      case Position.Above:
        // Anchor the iph bubble to the top center of the anchor element.
        iphTop = `${anchorTop - offset}px`;
        iphLeft = `${anchorLeft + anchorRect.width / 2}px`;
        // Horizontally center the iph bubble.
        iphTransform = `translate(-50%, -100%)`;
        break;
      case Position.Below:
        // Anchor the iph bubble to the bottom center of the anchor element.
        iphTop = `${anchorTop + anchorRect.height + offset}px`;
        iphLeft = `${anchorLeft + anchorRect.width / 2}px`;
        // Horizontally center the iph bubble.
        iphTransform = `translateX(-50%)`;
        break;
      case Position.Left:
        // Anchor the iph bubble to the center left of the anchor element.
        iphTop = `${anchorTop + anchorRect.height / 2}px`;
        iphLeft = `${anchorLeft - offset}px`;
        // Vertically center the iph bubble.
        iphTransform = `translate(-100%, -50%)`;
        break;
      case Position.Right:
        // Anchor the iph bubble to the center right of the anchor element.
        iphTop = `${anchorTop + anchorRect.height / 2}px`;
        iphLeft = `${anchorLeft + anchorRect.width + offset}px`;
        // Vertically center the iph bubble.
        iphTransform = `translateY(-50%)`;
        break;
    }
    this.style.top = iphTop;
    this.style.left = iphLeft;
    this.style.transform = iphTransform;
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
