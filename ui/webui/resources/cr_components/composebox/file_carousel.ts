// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './file_thumbnail.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
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
    };
  }

  accessor files: ComposeboxFile[] = [];

  private resizeObserver_: ResizeObserver|null = null;

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
      this.dispatchEvent(new CustomEvent('carousel-resize', {
        bubbles: true,
        composed: true,
        detail: {height: height},
      }));
    }, DEBOUNCE_TIMEOUT_MS));
    this.resizeObserver_.observe(this);
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    if (this.resizeObserver_) {
      this.resizeObserver_.disconnect();
      this.resizeObserver_ = null;
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
