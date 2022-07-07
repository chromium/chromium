// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A bubble for displaying in-product help. These are created
 * dynamically by HelpBubbleMixin, and their API should be considered an
 * implementation detail and subject to change (you should not add them to your
 * components directly).
 */
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';

import {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import {assert, assertNotReached} from '//resources/js/assert_ts.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './help_bubble.html.js';
import {HelpBubblePosition} from './help_bubble.mojom-webui.js';

const ANCHOR_HIGHLIGHT_CLASS = 'help-anchor-highlight';

export type HelpBubbleDismissedEventDetail = {
  anchorId: string,
  fromActionButton: boolean,
  buttonIndex?: number,
};

export interface HelpBubbleElement {
  $: {
    body: HTMLElement,
    close: CrIconButtonElement,
  };
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
      anchorId: {
        type: String,
        value: '',
        reflectToAttribute: true,
      },
      body: String,
      closeText: String,
    };
  }

  anchorId: string;
  body: string;
  closeText: string;
  position: HelpBubblePosition;

  /**
   * HTMLElement corresponding to |this.anchorId|.
   */
  private anchorElement_: HTMLElement|null = null;

  /**
   * Shows the bubble.
   */
  show() {
    this.anchorElement_ =
        this.parentElement!.querySelector<HTMLElement>(`#${this.anchorId}`)!;
    assert(
        this.anchorElement_,
        'Tried to show a help bubble but couldn\'t find element with id ' +
            this.anchorId);

    // Reset the aria-hidden attribute as screen readers need to access the
    // contents of an opened bubble.
    this.style.display = 'block';
    this.removeAttribute('aria-hidden');
    this.updatePosition_();
    this.setAnchorHighlight_(true);
  }

  /**
   * Hides the bubble and ensures that screen readers cannot its contents
   * while hidden.
   */
  hide() {
    this.style.display = 'none';
    this.setAttribute('aria-hidden', 'true');
    this.setAnchorHighlight_(false);
    this.anchorElement_ = null;
  }

  /**
   * Retrieves the current anchor element, if set and the bubble is showing,
   * otherwise null.
   */
  getAnchorElement(): HTMLElement|null {
    return this.anchorElement_;
  }

  private dismiss_() {
    assert(this.anchorId);
    this.dispatchEvent(new CustomEvent('help-bubble-dismissed', {
      detail: {
        anchorId: this.anchorId,
        fromActionButton: false,
      },
    }));
  }

  /**
   * Sets the bubble position, as relative to that of the anchor element and
   * |this.position|.
   */
  private updatePosition_() {
    assert(this.anchorElement_);

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
    assert(this.anchorElement_);
    this.anchorElement_.classList.toggle(ANCHOR_HIGHLIGHT_CLASS, highlight);
  }
}

customElements.define(HelpBubbleElement.is, HelpBubbleElement);

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble': HelpBubbleElement;
  }
  interface HTMLElementEventMap {
    'help-bubble-dismissed': CustomEvent<HelpBubbleDismissedEventDetail>;
  }
}
