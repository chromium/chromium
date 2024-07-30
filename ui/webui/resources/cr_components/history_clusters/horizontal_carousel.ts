// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/icons_lit.html.js';

import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './horizontal_carousel.css.js';
import {getHtml} from './horizontal_carousel.html.js';

/**
 * @fileoverview This file provides a custom element displaying a horizontal
 * carousel for the carousel elements.
 */

declare global {
  interface HTMLElementTagNameMap {
    'horizontal-carousel': HorizontalCarouselElement;
  }
}

export interface HorizontalCarouselElement {
  $: {
    backButton: HTMLElement,
    carouselContainer: HTMLElement,
    forwardButton: HTMLElement,
  };
}

export class HorizontalCarouselElement extends CrLitElement {
  static get is() {
    return 'horizontal-carousel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      showForwardButton_: {
        type: Boolean,
        reflect: true,
      },

      showBackButton_: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  private resizeObserver_: ResizeObserver|null = null;
  private eventTracker_: EventTracker = new EventTracker();
  protected showBackButton_: boolean = false;
  protected showForwardButton_: boolean = false;

  //============================================================================
  // Overridden methods
  //============================================================================

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver_ = new ResizeObserver(() => {
      this.setShowCarouselButtons_();
    });

    this.resizeObserver_.observe(this.$.carouselContainer);
    this.eventTracker_.add(this, 'keyup', this.onTabFocus_.bind(this));
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.unobserve(this.$.carouselContainer);
      this.resizeObserver_ = null;
    }
  }
  //============================================================================
  // Event handlers
  //============================================================================

  protected onCarouselBackClick_() {
    const targetPosition = this.calculateTargetPosition_(-1);
    this.$.carouselContainer.scrollTo(
        {left: targetPosition, behavior: 'smooth'});
    this.showBackButton_ = targetPosition > 0;
    this.showForwardButton_ = true;
  }

  protected onCarouselForwardClick_() {
    const targetPosition = this.calculateTargetPosition_(1);
    this.$.carouselContainer.scrollTo(
        {left: targetPosition, behavior: 'smooth'});
    this.showForwardButton_ =
        targetPosition + this.$.carouselContainer.clientWidth <
        this.$.carouselContainer.scrollWidth;
    this.showBackButton_ = true;
  }

  private onTabFocus_(event: KeyboardEvent) {
    const element = event.target as HTMLElement;
    if (event.code === 'Tab') {
      // -2px as offsetLeft includes padding
      this.$.carouselContainer.scrollTo(
          {left: element.offsetLeft - 2, behavior: 'smooth'});
    }
  }

  //============================================================================
  // Helper methods
  //============================================================================

  private setShowCarouselButtons_() {
    if (Math.round(this.$.carouselContainer.scrollLeft) +
            this.$.carouselContainer.clientWidth <
        this.$.carouselContainer.scrollWidth) {
      // On shrinking the window, the forward button should show up again.
      this.showForwardButton_ = true;
    } else {
      // On expanding the window, the back and forward buttons should disappear.
      this.showBackButton_ = this.$.carouselContainer.scrollLeft > 0;
      this.showForwardButton_ = false;
    }
  }

  private calculateTargetPosition_(direction: number) {
    const offset = this.$.carouselContainer.clientWidth / 2 * direction;
    const targetPosition =
        Math.floor(this.$.carouselContainer.scrollLeft + offset);
    return Math.max(
        0,
        Math.min(
            targetPosition,
            this.$.carouselContainer.scrollWidth -
                this.$.carouselContainer.clientWidth));
  }
}

customElements.define(HorizontalCarouselElement.is, HorizontalCarouselElement);
