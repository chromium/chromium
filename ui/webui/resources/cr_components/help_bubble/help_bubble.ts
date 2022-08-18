// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview A bubble for displaying in-product help. These are created
 * dynamically by HelpBubbleMixin, and their API should be considered an
 * implementation detail and subject to change (you should not add them to your
 * components directly).
 */
import 'chrome://resources/cr_elements/cr_button/cr_button.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/hidden_style_css.m.js';
import 'chrome://resources/cr_elements/icons.m.js';

import {CrButtonElement} from '//resources/cr_elements/cr_button/cr_button.js';
import {CrIconButtonElement} from '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import {assert, assertNotReached} from '//resources/js/assert_ts.js';
import {isWindows} from '//resources/js/cr.m.js';
import {DomRepeatEvent, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './help_bubble.html.js';
import {HelpBubbleButtonParams, HelpBubblePosition, Progress} from './help_bubble.mojom-webui.js';

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
    arrow: HTMLElement,
    buttons: HTMLElement,
    close: CrIconButtonElement,
    main: HTMLElement,
    progress: HTMLElement,
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

      position: {
        type: HelpBubblePosition,
        value: HelpBubblePosition.BELOW,
        reflectToAttribute: true,
      },
    };
  }

  anchorId: string;
  bodyText: string;
  titleText: string;
  closeText: string;
  position: HelpBubblePosition;
  buttons: HelpBubbleButtonParams[] = [];
  progress: Progress|null = null;
  infoIcon: string|null = null;
  forceCloseButton: boolean;

  /**
   * HTMLElement corresponding to |this.anchorId|.
   */
  private anchorElement_: HTMLElement|null = null;

  /**
   * Backing data for the dom-repeat that generates progress indicators.
   * The elements are placeholders only.
   */
  private progressData_: void[] = [];

  /**
   * Shows the bubble.
   */
  show() {
    // Set up the progress track.
    if (this.progress) {
      this.progressData_ = new Array(this.progress.total);
    } else {
      this.progressData_ = [];
    }

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
    return this.$.buttons.querySelector<CrButtonElement>(
        `[id="${ACTION_BUTTON_ID_PREFIX + buttonIndex}"]`);
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

  private getProgressClass_(index: number): string {
    return index < this.progress!.current ? 'current-progress' :
                                            'total-progress';
  }

  private shouldShowTitleInTopContainer_(
      progress: Progress|null, titleText: string): boolean {
    return !!titleText && !progress;
  }

  private shouldShowBodyInTopContainer_(
      progress: Progress|null, titleText: string): boolean {
    return !progress && !titleText;
  }

  private shouldShowBodyInMain_(progress: Progress|null, titleText: string):
      boolean {
    return !!progress || !!titleText;
  }

  private shouldShowCloseButton_(
      buttons: HelpBubbleButtonParams[], forceCloseButton: boolean): boolean {
    return buttons.length === 0 || forceCloseButton;
  }

  private shouldShowInfoIcon_(progress: Progress|null, infoIcon: string):
      boolean {
    // TODO(mickeyburks): Info icon needs to be added to HelpBubbleParams
    return !progress && infoIcon !== null && infoIcon !== '';
  }

  private onButtonClick_(e: DomRepeatEvent<HelpBubbleButtonParams>) {
    assert(
        this.anchorId,
        'Action button clicked: expected help bubble to have an anchor.');
    // There is no access to the model index here due to limitations of
    // dom-repeat. However, the index is stored in the node's identifier.
    const index: number = parseInt(
        (e.target as Element).id.substring(ACTION_BUTTON_ID_PREFIX.length));
    this.dispatchEvent(new CustomEvent(HELP_BUBBLE_DISMISSED_EVENT, {
      detail: {
        anchorId: this.anchorId,
        fromActionButton: true,
        buttonIndex: index,
      },
    }));
  }

  private getButtonId_(index: number): string {
    return ACTION_BUTTON_ID_PREFIX + index;
  }

  private getButtonClass_(isDefault: boolean): string {
    return isDefault ? 'default-button' : '';
  }

  private getButtonTabIndex_(index: number, isDefault: boolean): number {
    return isDefault ? 1 : index + 2;
  }

  private buttonSortFunc_(
      button1: HelpBubbleButtonParams,
      button2: HelpBubbleButtonParams): number {
    // Default button is leading on Windows, trailing on other platforms.
    if (button1.isDefault) {
      return isWindows ? -1 : 1;
    }
    if (button2.isDefault) {
      return isWindows ? 1 : -1;
    }
    return 0;
  }

  private getArrowClass_(position: HelpBubblePosition): string {
    switch (position) {
      case HelpBubblePosition.ABOVE:
        return 'above';
      case HelpBubblePosition.BELOW:
        return 'below';
      case HelpBubblePosition.LEFT:
        return 'left';
      case HelpBubblePosition.RIGHT:
        return 'right';
      default:
        assertNotReached('Unknown help bubble position: ' + position);
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
    const parentRect = this.offsetParent!.getBoundingClientRect();
    const anchorRect = this.anchorElement_.getBoundingClientRect();
    const anchorLeft = anchorRect.left - parentRect.left;
    const anchorHorizontalCenter = anchorLeft + anchorRect.width / 2;
    const anchorTop = anchorRect.top - parentRect.top;
    const ARROW_OFFSET = 16;
    const LEFT_MARGIN = 8;
    const HELP_BUBBLE_WIDTH = 362;

    let helpLeft: string = '';
    let helpTop: string = '';

    switch (this.position) {
      case HelpBubblePosition.ABOVE:
        // Anchor the help bubble to the top center of the anchor element.
        helpTop = `${anchorTop - ARROW_OFFSET}px`;
        helpLeft = `${
            Math.max(
                LEFT_MARGIN,
                anchorHorizontalCenter - HELP_BUBBLE_WIDTH / 2)}px`;
        break;
      case HelpBubblePosition.BELOW:
        // Anchor the help bubble to the bottom center of the anchor element.
        helpTop = `${anchorTop + anchorRect.height + ARROW_OFFSET}px`;
        helpLeft = `${
            Math.max(
                LEFT_MARGIN,
                anchorHorizontalCenter - HELP_BUBBLE_WIDTH / 2)}px`;
        break;
      case HelpBubblePosition.LEFT:
        // Anchor the help bubble to the center left of the anchor element.
        helpTop = `${anchorTop + anchorRect.height / 2}px`;
        helpLeft = `${anchorLeft - ARROW_OFFSET}px`;
        break;
      case HelpBubblePosition.RIGHT:
        // Anchor the help bubble to the center right of the anchor element.
        helpTop = `${anchorTop + anchorRect.height / 2}px`;
        helpLeft = `${anchorLeft + anchorRect.width + ARROW_OFFSET}px`;
        break;
      default:
        assertNotReached();
    }

    this.style.top = helpTop;
    this.style.left = helpLeft;
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
