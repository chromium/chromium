// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
import './file_thumbnail.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxFile} from './common.js';
import {getCss} from './file_carousel.css.js';
import {getHtml} from './file_carousel.html.js';


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
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-composebox-file-carousel': ComposeboxFileCarouselElement;
  }
}

customElements.define(
    ComposeboxFileCarouselElement.is, ComposeboxFileCarouselElement);
