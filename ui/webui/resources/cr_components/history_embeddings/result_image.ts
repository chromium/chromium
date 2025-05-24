// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_auto_img/cr_auto_img.js';

import {PageImageServiceBrowserProxy} from '//resources/cr_components/page_image_service/browser_proxy.js';
import {ClientId as PageImageServiceClientId} from '//resources/cr_components/page_image_service/page_image_service.mojom-webui.js';
import {assert} from '//resources/js/assert.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

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
      hasImage: {type: Boolean, reflect: true},
      imageUrl_: {type: String},
      inSidePanel: {type: Boolean, reflect: true},
      searchResult: {type: Object},
    };
  }

  accessor hasImage: boolean = false;
  protected accessor imageUrl_: string|null = null;
  accessor inSidePanel: boolean = false;
  accessor searchResult: SearchResultItem|null = null;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (!loadTimeData.getBoolean('enableHistoryEmbeddingsImages')) {
      return;
    }

    if (changedProperties.has('searchResult')) {
      // Reset image state while it is being fetched.
      this.imageUrl_ = null;
      this.hasImage = false;

      if (this.searchResult?.isUrlKnownToSync) {
        this.fetchImageForSearchResult_();
      }
    }
  }

  private async fetchImageForSearchResult_() {
    assert(this.searchResult);
    const searchResultUrl = this.searchResult.url;
    const {result} =
        await PageImageServiceBrowserProxy.getInstance()
            .handler.getPageImageUrl(
                PageImageServiceClientId.HistoryEmbeddings, searchResultUrl,
                {suggestImages: true, optimizationGuideImages: true});
    if (result && searchResultUrl === this.searchResult.url) {
      this.imageUrl_ = result.imageUrl.url;
      this.hasImage = true;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-history-embeddings-result-image': HistoryEmbeddingsResultImageElement;
  }
}

customElements.define(
    HistoryEmbeddingsResultImageElement.is,
    HistoryEmbeddingsResultImageElement);
