// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_feedback_buttons/cr_feedback_buttons.js';
import '//resources/cr_elements/cr_hidden_style.css.js';
import '//resources/cr_elements/cr_loading_gradient/cr_loading_gradient.js';
import '//resources/cr_elements/cr_shared_vars.css.js';
import '//resources/cr_elements/cr_url_list_item/cr_url_list_item.js';

import {I18nMixin} from '//resources/cr_elements/i18n_mixin.js';
import type {Time} from '//resources/mojo/mojo/public/mojom/base/time.mojom-webui.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import type {DomRepeatEvent} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {HistoryEmbeddingsBrowserProxyImpl} from './browser_proxy.js';
import {getTemplate} from './history_embeddings.html.js';
import type {SearchQuery, SearchResult, SearchResultItem} from './history_embeddings.mojom-webui.js';

function jsDateToMojoDate(date: Date): Time {
  const windowsEpoch = Date.UTC(1601, 0, 1, 0, 0, 0, 0);
  const unixEpoch = Date.UTC(1970, 0, 1, 0, 0, 0, 0);
  const epochDeltaInMs = unixEpoch - windowsEpoch;
  const internalValue = BigInt(date.valueOf() + epochDeltaInMs) * BigInt(1000);
  return {internalValue};
}

export interface HistoryEmbeddingsElement {
  $: {
    heading: HTMLElement,
  };
}

const HistoryEmbeddingsElementBase = I18nMixin(PolymerElement);

export class HistoryEmbeddingsElement extends HistoryEmbeddingsElementBase {
  static get is() {
    return 'cr-history-embeddings';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      loading_: Boolean,
      searchQuery: String,
      timeRangeStart: Object,
    };
  }

  static get observers() {
    return [
      'onSearchQueryChanged_(searchQuery, timeRangeStart)',
    ];
  }

  private browserProxy_ = HistoryEmbeddingsBrowserProxyImpl.getInstance();
  private loading_ = false;
  private searchResult_: SearchResult;
  searchQuery: string;
  timeRangeStart?: Date;

  private getHeadingText_(): string {
    if (this.loading_) {
      return this.i18n('historyEmbeddingsHeadingLoading', this.searchQuery);
    }
    return this.i18n('historyEmbeddingsHeading', this.searchQuery);
  }

  private onMoreActionsClick_(e: DomRepeatEvent<SearchResultItem>) {
    this.dispatchEvent(
        new CustomEvent('more-actions-click', {detail: e.model.item}));
  }

  private onResultClick_(e: DomRepeatEvent<SearchResultItem>) {
    this.dispatchEvent(new CustomEvent('result-click', {detail: e.model.item}));
  }

  private onSearchQueryChanged_() {
    this.loading_ = true;
    const query: SearchQuery = {
      query: this.searchQuery,
      timeRangeStart:
          this.timeRangeStart ? jsDateToMojoDate(this.timeRangeStart) : null,
    };
    this.browserProxy_.search(query).then((result) => {
      this.searchResult_ = result;
      this.loading_ = false;
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-history-embeddings': HistoryEmbeddingsElement;
  }
}

customElements.define(HistoryEmbeddingsElement.is, HistoryEmbeddingsElement);
