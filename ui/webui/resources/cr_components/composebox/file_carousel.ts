// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './file_thumbnail.js';

import {CrLitElement, type PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {UnguessableToken} from '//resources/mojo/mojo/public/mojom/base/unguessable_token.mojom-webui.js';

import type {ComposeboxFile} from './common.js';
import {getCss} from './file_carousel.css.js';
import {getHtml} from './file_carousel.html.js';

const DEBOUNCE_TIMEOUT_MS: number = 20;
const CAROUSEL_HEIGHT_PADDING = 18;

function debounce(context: Object, func: () => void, delay: number) {
  let timeout: number;
  return function(...args: []) {
    clearTimeout(timeout);
    timeout = setTimeout(() => func.apply(context, args), delay);
  };
}

export class ComposeboxFileCarouselElement extends CrLitElement {
  static get is() {
    return 'cr-composebox-file-carousel';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      files: {type: Array},
      enableScrolling: {
        type: Boolean,
        reflect: true,
      },
      gradientTopHidden_: {
        type: Boolean,
        reflect: true,
        attribute: 'gradient-top-hidden',
      },
      gradientBottomHidden_: {
        type: Boolean,
        reflect: true,
        attribute: 'gradient-bottom-hidden',
      },
    };
  }

  accessor files: ComposeboxFile[] = [];
  accessor enableScrolling: boolean = false;

  private accessor gradientTopHidden_: boolean = false;
  private accessor gradientBottomHidden_: boolean = false;

  private resizeObserver_: ResizeObserver|null = null;
  private mutationObserver_: MutationObserver|null = null;
  private fileCarouselContainer_: HTMLElement|null = null;

  getThumbnailElementByUuid(uuid: UnguessableToken): HTMLElement|null {
    const thumbnails =
        this.shadowRoot.querySelectorAll('cr-composebox-file-thumbnail');
    for (const thumbnail of thumbnails) {
      if (thumbnail.file.uuid === uuid) {
        return thumbnail;
      }
    }
    return null;
  }

  override connectedCallback() {
    super.connectedCallback();
    this.resizeObserver_ = new ResizeObserver(debounce(this, () => {
      const height =
          this.clientHeight ? this.clientHeight + CAROUSEL_HEIGHT_PADDING : 0;
      this.fire('carousel-resize', {height: height});
    }, DEBOUNCE_TIMEOUT_MS));
    this.resizeObserver_.observe(this);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
    }
    if (this.mutationObserver_) {
      this.mutationObserver_.disconnect();
      this.mutationObserver_ = null;
    }
    if (this.fileCarouselContainer_) {
      this.fileCarouselContainer_.removeEventListener(
          'scroll', this.toggleGradient);
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('enableScrolling')) {
      this.initializeOrUpdateObservers_();
    }

    // Scroll to the bottom when files are added.
    if (this.enableScrolling && changedProperties.has('files')) {
      const oldFiles: ComposeboxFile[]|undefined =
          changedProperties.get('files');
      // `oldFiles` is undefined on the component's first render. We only want
      // to scroll when files are added after the component has been rendered.
      if (oldFiles && this.files.length > oldFiles.length) {
        this.scrollToBottom();
      }
    }
  }

  private async initializeOrUpdateObservers_() {
    if (this.mutationObserver_) {
      this.mutationObserver_.disconnect();
      this.mutationObserver_ = null;
    }

    if (this.fileCarouselContainer_) {
      this.fileCarouselContainer_.removeEventListener(
          'scroll', this.toggleGradient);
    }

    if (!this.enableScrolling) {
      return;
    }

    await this.updateComplete;
    this.fileCarouselContainer_ =
        this.shadowRoot.querySelector('.file-carousel-container');
    if (this.fileCarouselContainer_) {
      this.toggleGradient = this.toggleGradient.bind(this);
      this.fileCarouselContainer_.addEventListener('scroll', this.toggleGradient);
      this.mutationObserver_ = new MutationObserver(() => {
        this.toggleGradient();
      });
      this.mutationObserver_.observe(
          this.fileCarouselContainer_, {childList: true});
      this.toggleGradient();
    }
  }

  private toggleGradient() {
    if (!this.fileCarouselContainer_) {
      return;
    }

    const el = this.fileCarouselContainer_;
    const hasScrollbar = el.scrollHeight > el.clientHeight;
    if (!hasScrollbar) {
      this.gradientTopHidden_ = true;
      this.gradientBottomHidden_ = true;
      return;
    }

    const isScrolledToTop = el.scrollTop === 0;
    this.gradientTopHidden_ = isScrolledToTop;

    // Epsilon comparison to handle non-integer values.
    const isScrolledToBottom =
        Math.abs(el.scrollHeight - el.clientHeight - el.scrollTop) < 1;
    this.gradientBottomHidden_ = isScrolledToBottom;
  }

  private scrollToBottom() {
    if (this.fileCarouselContainer_) {
      this.fileCarouselContainer_.scrollTo({
        top: this.fileCarouselContainer_.scrollHeight,
        behavior: 'smooth',
      });
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-file-carousel': ComposeboxFileCarouselElement;
  }
}

customElements.define(
    ComposeboxFileCarouselElement.is, ComposeboxFileCarouselElement);
