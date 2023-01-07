// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './history_clusters_shared_style.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {RelatedSearchAction, SearchQuery} from './history_clusters.mojom-webui.js';
import {MetricsProxyImpl} from './metrics_proxy.js';
import {getTemplate} from './search_query.html.js';

/**
 * @fileoverview This file provides a custom element displaying a search query.
 */

declare global {
  interface HTMLElementTagNameMap {
    'search-query': SearchQueryElement;
  }
}

class SearchQueryElement extends PolymerElement {
  static get is() {
    return 'search-query';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * The index of the search query pill.
       */
      index: {
        type: Number,
        value: -1,  // Initialized to an invalid value.
      },

      /**
       * The search query to display.
       */
      searchQuery: Object,
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  index: number;
  searchQuery: SearchQuery;

  //============================================================================
  // Event handlers
  //============================================================================

  private onAuxClick_() {
    MetricsProxyImpl.getInstance().recordRelatedSearchAction(
        RelatedSearchAction.kClicked, this.index);

    // Notify the parent <history-cluster> element of this event.
    this.dispatchEvent(new CustomEvent('related-search-clicked', {
      bubbles: true,
      composed: true,
    }));
  }

  private onClick_(event: MouseEvent) {
    event.preventDefault();  // Prevent default browser action (navigation).

    // To record metrics.
    this.onAuxClick_();

    this.openUrl_(event);
  }

  private onKeydown_(e: KeyboardEvent) {
    // To be consistent with <history-list>, only handle Enter, and not Space.
    if (e.key !== 'Enter') {
      return;
    }

    // To record metrics.
    this.onAuxClick_();

    this.openUrl_(e);
  }

  private openUrl_(event: MouseEvent|KeyboardEvent) {
    BrowserProxyImpl.getInstance().handler.openHistoryCluster(
        this.searchQuery.url, {
          middleButton: false,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        });
  }
}

customElements.define(SearchQueryElement.is, SearchQueryElement);
