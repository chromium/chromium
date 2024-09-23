// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrRippleMixin} from '//resources/cr_elements/cr_ripple/cr_ripple_mixin.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import type {SearchQuery} from './history_cluster_types.mojom-webui.js';
import {RelatedSearchAction} from './history_clusters.mojom-webui.js';
import {MetricsProxyImpl} from './metrics_proxy.js';
import {getCss} from './search_query.css.js';
import {getHtml} from './search_query.html.js';

/**
 * @fileoverview This file provides a custom element displaying a search query.
 */

export interface SearchQueryElement {
  $: {
    searchQueryLink: HTMLElement,
  };
}

const SearchQueryElementBase = CrRippleMixin(CrLitElement);

export class SearchQueryElement extends SearchQueryElementBase {
  static get is() {
    return 'search-query';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /**
       * The index of the search query pill.
       */
      index: {type: Number},

      /**
       * The search query to display.
       */
      searchQuery: {type: Object},
    };
  }

  //============================================================================
  // Properties
  //============================================================================

  index: number = -1;  // Initialized to an invalid value.
  searchQuery?: SearchQuery;

  //============================================================================
  // Event handlers
  //============================================================================

  override firstUpdated() {
    this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
    this.addEventListener('pointercancel', this.onPointerCancel_.bind(this));
  }

  protected onAuxClick_() {
    MetricsProxyImpl.getInstance().recordRelatedSearchAction(
        RelatedSearchAction.kClicked, this.index);

    // Notify the parent <history-cluster> element of this event.
    this.fire('related-search-clicked');
  }

  protected onClick_(event: MouseEvent) {
    event.preventDefault();  // Prevent default browser action (navigation).

    // To record metrics.
    this.onAuxClick_();

    this.openUrl_(event);
  }

  protected onKeydown_(e: KeyboardEvent) {
    // Disable ripple on Space.
    this.noink = e.key === ' ';

    // To be consistent with <history-list>, only handle Enter, and not Space.
    if (e.key !== 'Enter') {
      return;
    }

    this.getRipple().uiDownAction();

    // To record metrics.
    this.onAuxClick_();

    this.openUrl_(e);

    setTimeout(() => this.getRipple().uiUpAction(), 100);
  }

  private onPointerDown_() {
    // Ensure ripple is visible.
    this.noink = false;
    this.ensureRipple();
  }

  private onPointerCancel_() {
    this.getRipple().clear();
  }

  private openUrl_(event: MouseEvent|KeyboardEvent) {
    assert(this.searchQuery);
    BrowserProxyImpl.getInstance().handler.openHistoryCluster(
        this.searchQuery.url, {
          middleButton: false,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        });
  }

  // Overridden from CrRippleMixin
  override createRipple() {
    this.rippleContainer = this.$.searchQueryLink;
    const ripple = super.createRipple();
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'search-query': SearchQueryElement;
  }
}

customElements.define(SearchQueryElement.is, SearchQueryElement);
