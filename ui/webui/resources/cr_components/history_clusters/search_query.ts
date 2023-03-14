// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './history_clusters_shared_style.css.js';

import {PaperRippleBehavior} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';
import {mixinBehaviors, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {BrowserProxyImpl} from './browser_proxy.js';
import {SearchQuery} from './history_cluster_types.mojom-webui.js';
import {RelatedSearchAction} from './history_clusters.mojom-webui.js';
import {MetricsProxyImpl} from './metrics_proxy.js';
import {getTemplate} from './search_query.html.js';

/**
 * @fileoverview This file provides a custom element displaying a search query.
 */

interface SearchQueryElement {
  $: {
    searchQueryLink: HTMLElement,
  };
}

declare global {
  interface HTMLElementTagNameMap {
    'search-query': SearchQueryElement;
  }
}

const SearchQueryElementBase =
    mixinBehaviors([PaperRippleBehavior], PolymerElement) as {
      new (): PolymerElement & PaperRippleBehavior,
    };

class SearchQueryElement extends SearchQueryElementBase {
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

  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _rippleContainer: Element;

  //============================================================================
  // Event handlers
  //============================================================================

  override ready() {
    super.ready();
    if (document.documentElement.hasAttribute('chrome-refresh-2023')) {
      this.addEventListener('pointerdown', this.onPointerDown_.bind(this));
      this.addEventListener('pointercancel', this.onPointerCancel_.bind(this));
    }
  }

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
    BrowserProxyImpl.getInstance().handler.openHistoryCluster(
        this.searchQuery.url, {
          middleButton: false,
          altKey: event.altKey,
          ctrlKey: event.ctrlKey,
          metaKey: event.metaKey,
          shiftKey: event.shiftKey,
        });
  }

  // Overridden from PaperRippleBehavior
  /* eslint-disable-next-line @typescript-eslint/naming-convention */
  override _createRipple() {
    this._rippleContainer = this.$.searchQueryLink;
    const ripple = super._createRipple();
    return ripple;
  }
}

customElements.define(SearchQueryElement.is, SearchQueryElement);
