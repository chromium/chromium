// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './history_clusters_shared_style.css.js';
import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './horizontal_carousel.html.js';

/**
 * @fileoverview This file provides a custom element displaying a horizontal
 * carousel for the carousel elements.
 */

const SCROLL_INTERVAL = 80;

declare global {
  interface HTMLElementTagNameMap {
    'horizontal-carousel': HorizontalCarouselElement;
  }
}

const HorizontalCarouselElementBase = I18nMixin(PolymerElement);

export interface HorizontalCarouselElement {
  $: {
    carouselBackButton: HTMLElement,
    carouselContainer: HTMLElement,
    carouselForwardButton: HTMLElement,
  };
}

export class HorizontalCarouselElement extends HorizontalCarouselElementBase {
  static get is() {
    return 'horizontal-carousel';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      showForwardButton_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },

      showBackButton_: {
        type: Boolean,
        value: false,
        reflectToAttribute: true,
      },
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  private resizeObserver_: ResizeObserver|null = null;
  private showBackButton_: boolean;
  private showForwardButton_: boolean;

  //============================================================================
  // Overridden methods
  //============================================================================

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver_ = new ResizeObserver(() => {
      this.setShowCarouselButtons_();
    });

    this.resizeObserver_.observe(this.$.carouselContainer);
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

  private onCarouselBackClick_() {
    this.$.carouselContainer.scrollLeft -= SCROLL_INTERVAL;
    if (this.$.carouselContainer.scrollLeft <= 0) {
      this.showBackButton_ = false;
    }
    this.showForwardButton_ = true;
  }

  private onCarouselForwardClick_() {
    this.$.carouselContainer.scrollLeft += SCROLL_INTERVAL;
    if (Math.round(this.$.carouselContainer.scrollLeft) +
            this.$.carouselContainer.clientWidth >=
        this.$.carouselContainer.scrollWidth) {
      this.showForwardButton_ = false;
    }
    this.showBackButton_ = true;
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
      if (this.$.carouselContainer.scrollLeft <= 0) {
        this.showBackButton_ = false;
      }
      this.showForwardButton_ = false;
    }
  }
}

customElements.define(HorizontalCarouselElement.is, HorizontalCarouselElement);
