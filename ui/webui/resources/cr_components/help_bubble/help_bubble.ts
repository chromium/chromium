// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A bubble for displaying in-product help. These are created
 * dynamically by HelpBubbleMixin, and their API should be considered an
 * implementation detail and subject to change (you should not add them to your
 * components directly).
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.m.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import 'chrome://resources/cr_elements/icons.m.js';

import {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.m.js';
import {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.m.js';
import {assert, assertNotReached} from '//resources/js/assert_ts.js';
import {isWindows} from '//resources/js/cr.m.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {EventTracker} from 'chrome://resources/js/event_tracker.m.js';

import {getTemplate} from './help_bubble.html.js';
import {HelpBubblePosition} from './help_bubble.mojom-webui.js';

const ANCHOR_HIGHLIGHT_CLASS = 'help-anchor-highlight';

const ACTION_BUTTON_ID_PREFIX = 'action-button-';

export const HELP_BUBBLE_DISMISSED_EVENT = 'help-bubble-dismissed';

export type HelpBubbleDismissedEvent = CustomEvent<{
  anchorId: string,
  fromActionButton: boolean,
  buttonIndex?: number,
}>;

export interface HelpBubbleElement {
  $: {
    body: HTMLElement,
    buttons: HTMLElement,
    close: CrIconButtonElement,
    main: HTMLElement,
    title: HTMLElement,
    topContainer: HTMLElement,
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
      closeText: String,
    };
  }

  anchorId: string;
  bodyText: string;
  titleText: string;
  closeText: string;
  position: HelpBubblePosition;
  buttons: string[] = [];
  defaultButtonIndex: number;

  /**
   * HTMLElement corresponding to |this.anchorId|.
   */
  private anchorElement_: HTMLElement|null = null;

  private buttonEventTracker_: EventTracker = new EventTracker();

  /**
   * Shows the bubble.
   */
  show() {
    // If there is no title, the body element should be in the top container
    // with the close button, else it should be in the main container.
    if (this.titleText) {
      this.$.title.style.display = 'block';
      this.$.title.innerText = this.titleText;
      this.$.main.appendChild(this.$.body);
    } else {
      this.$.title.style.display = 'none';
      this.$.topContainer.appendChild(this.$.body);
    }
    this.$.body.innerText = this.bodyText;

    this.addButtons_();

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
   * Hides the bubble, clears out its contents, and ensures that screen readers
   * ignore it while hidden.
   *
   * TODO(dfried): We are moving towards formalizing help bubbles as single-use;
   * in which case most of this tear-down logic can be removed since the entire
   * bubble will go away on hide.
   */
  hide() {
    this.removeButtons_();
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

  /**
   * Returns the button with the given `buttonIndex`, or null if not found.
   */
  getButtonForTesting(buttonIndex: number): CrButtonElement|null {
    for (const button of this.$.buttons.children) {
      if (button.id === ACTION_BUTTON_ID_PREFIX + buttonIndex) {
        return button as CrButtonElement;
      }
    }
    return null;
  }

  /**
   * Returns whether the default button is leading (true on Windows) vs trailing
   * (all other platforms).
   */
  static isDefaultButtonLeading(): boolean {
    return isWindows;
  }

  private dismiss_() {
    assert(this.anchorId, 'Dismiss: expected help bubble to have an anchor.');
    this.dispatchEvent(new CustomEvent(HELP_BUBBLE_DISMISSED_EVENT, {
      detail: {
        anchorId: this.anchorId,
        fromActionButton: false,
      },
    }));
  }

  private onButtonClicked_(buttonIndex: number, _e: Event) {
    assert(
        this.anchorId,
        'Action button clicked: expected help bubble to have an anchor.');
    this.dispatchEvent(new CustomEvent(HELP_BUBBLE_DISMISSED_EVENT, {
      detail: {
        anchorId: this.anchorId,
        fromActionButton: true,
        buttonIndex: buttonIndex,
      },
    }));
  }

  /**
   * Removes button elements and listeners, if any are present.
   */
  private removeButtons_() {
    while (this.$.buttons.firstChild) {
      this.buttonEventTracker_.remove(this.$.buttons.firstChild, 'click');
      this.$.buttons.removeChild(this.$.buttons.firstChild);
    }
  }

  /**
   * Adds any buttons required by `this.buttons` with their on-click listeners.
   */
  private addButtons_() {
    assert(
        !this.$.buttons.firstChild,
        'Add buttons: expected button list to be empty.');

    // If there are no buttons to add, hide the container and return.
    if (!this.buttons.length) {
      return;
    }

    let defaultButton: HTMLElement|null = null;
    for (let i: number = 0; i < this.buttons.length; ++i) {
      const button = document.createElement('cr-button');
      button.innerText = this.buttons[i];
      button.id = ACTION_BUTTON_ID_PREFIX + i;
      this.buttonEventTracker_.add(
          button, 'click', this.onButtonClicked_.bind(this, i));
      if (i === this.defaultButtonIndex) {
        defaultButton = button;
        // Default button should always be first in tab order.
        button.tabIndex = 1;
        button.classList.add('default-button');
      } else {
        // Tab index for non-default buttons starts at 2, since default button
        // gets 1.
        button.tabIndex = i + 2;
        this.$.buttons.appendChild(button);
      }
    }

    // Place the default button in the correct order; either leading or
    // trailing based on platform.
    if (defaultButton) {
      if (HelpBubbleElement.isDefaultButtonLeading() &&
          this.$.buttons.firstChild) {
        this.$.buttons.insertBefore(defaultButton, this.$.buttons.firstChild);
      } else {
        this.$.buttons.appendChild(defaultButton);
      }
    }
  }

  /**
   * Sets the bubble position, as relative to that of the anchor element and
   * |this.position|.
   */
  private updatePosition_() {
    assert(
        this.anchorElement_, 'Update position: expected valid anchor element.');

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
    assert(
        this.anchorElement_,
        'Set anchor highlight: expected valid anchor element.');
    this.anchorElement_.classList.toggle(ANCHOR_HIGHLIGHT_CLASS, highlight);
  }
}

customElements.define(HelpBubbleElement.is, HelpBubbleElement);

declare global {
  interface HTMLElementTagNameMap {
    'help-bubble': HelpBubbleElement;
  }
  interface HTMLElementEventMap {
    [HELP_BUBBLE_DISMISSED_EVENT]: HelpBubbleDismissedEvent;
  }
}
