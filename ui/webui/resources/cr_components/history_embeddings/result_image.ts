// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchResultItem} from './history_embeddings.mojom-webui.js';
import {getCss} from './result_image.css.js';
import {getHtml} from './result_image.html.js';

export class HistoryEmbeddingsResultImageElement extends CrLitElement {
  static get is() {
    return 'cr-history-embeddings-result-image';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      searchResult: {type: Object},
    };
  }

  searchResult: SearchResultItem;
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-history-embeddings-result-image': HistoryEmbeddingsResultImageElement;
  }
}

customElements.define(
    HistoryEmbeddingsResultImageElement.is,
    HistoryEmbeddingsResultImageElement);
