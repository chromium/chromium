// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
/**
 * @fileoverview A bubble for displaying in-product help. This is a WIP, do not
 * use.
 */
import {assertNotReached} from '//resources/js/assert_ts.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './help_bubble.html.js';
import {HelpBubblePosition} from './help_bubble.mojom-webui.js';

const ANCHOR_HIGHLIGHT_CLASS = 'help-anchor-highlight';

export interface HelpBubbleElement {
  open: boolean;
  _setOpen(open: boolean): void;
}

export class HelpBubbleElement extends PolymerElement {
  static get is() {
    return 'help-bubble';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
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

  anchorId: string;
  body: string;
  position: HelpBubblePosition;

  /**
   * HTMLElement corresponding to |this.anchorId|.
   */
  private anchorElement_: HTMLElement|null;

  /**
   * Shows the bubble.
   */
  show() {
    if (!this.anchorElement_) {
      this.anchorElement_ = this.findAnchorElement_();
      if (!this.anchorElement_) {
        assertNotReached(
            'Tried to show a help bubble but couldn\'t find element with id ' +
            this.anchorId);
      }
    }
    // Reset the aria-hidden attribute as screen readers need to access the
    // contents of an opened bubble.
    this.removeAttribute('aria-hidden');
    this.updatePosition_();
    this.setAnchorHighlight_(true);
    this._setOpen(true);
  }

  /**
   * Hides the bubble and ensures that screen readers cannot its contents
   * while hidden.
   */
  hide() {
    this.setAttribute('aria-hidden', 'true');
    this.setAnchorHighlight_(false);
    this._setOpen(false);
  }

  /**
   * Retrieves the current anchor element, if set and the bubble is showing,
   * otherwise null.
   */
  getAnchorElement(): HTMLElement|null {
    return this.open ? this.anchorElement_ : null;
  }

  /**
   * Returns the element that this help is anchored to. It is either the element
   * given by |this.anchorId|, or the immediate parent of the help.
   */
  private findAnchorElement_(): HTMLElement|null {
    const parentElement: HTMLElement|null = this.parentElement;
    if (!this.anchorId || !parentElement) {
      return null;
    }
    return parentElement.querySelector<HTMLElement>(`#${this.anchorId}`)!;
  }

  /**
   * Sets the bubble position, as relative to that of the anchor element and
   * |this.position|.
   */
  private updatePosition_() {
    if (!this.anchorElement_) {
      return;
    }
    // Inclusive of 8px visible arrow and 8px margin.
    const offset = 16;
    const parentRect = this.offsetParent!.getBoundingClientRect();
    const anchorRect = this.anchorElement_.getBoundingClientRect();
    const anchorLeft = anchorRect.left - parentRect.left;
    const anchorTop = anchorRect.top - parentRect.top;

    let helpLeft: string = '';
    let helpTop: string = '';
    let helpTransform: string = '';

    switch (this.position) {
      case HelpBubblePosition.ABOVE:
        // Anchor the help bubble to the top center of the anchor element.
        helpTop = `${anchorTop - offset}px`;
        helpLeft = `${anchorLeft + anchorRect.width / 2}px`;
        // Horizontally center the help bubble.
        helpTransform = 'translate(-50%, -100%)';
        break;
      case HelpBubblePosition.BELOW:
        // Anchor the help bubble to the bottom center of the anchor element.
        helpTop = `${anchorTop + anchorRect.height + offset}px`;
        helpLeft = `${anchorLeft + anchorRect.width / 2}px`;
        // Horizontally center the help bubble.
        helpTransform = 'translateX(-50%)';
        break;
      case HelpBubblePosition.LEFT:
        // Anchor the help bubble to the center left of the anchor element.
        helpTop = `${anchorTop + anchorRect.height / 2}px`;
        helpLeft = `${anchorLeft - offset}px`;
        // Vertically center the help bubble.
        helpTransform = 'translate(-100%, -50%)';
        break;
      case HelpBubblePosition.RIGHT:
        // Anchor the help bubble to the center right of the anchor element.
        helpTop = `${anchorTop + anchorRect.height / 2}px`;
        helpLeft = `${anchorLeft + anchorRect.width + offset}px`;
        // Vertically center the help bubble.
        helpTransform = 'translateY(-50%)';
        break;
      default:
        assertNotReached();
    }

    this.style.top = helpTop;
    this.style.left = helpLeft;
    this.style.transform = helpTransform;
  }

  /**
   * Styles the anchor element to appear highlighted while the bubble is open,
   * or removes the highlight.
   */
  private setAnchorHighlight_(highlight: boolean) {
    if (!this.anchorElement_) {
      return;
    }
    this.anchorElement_.classList.toggle(ANCHOR_HIGHLIGHT_CLASS, highlight);
  }
}

customElements.define(HelpBubbleElement.is, HelpBubbleElement);

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble': HelpBubbleElement;
  }
}
