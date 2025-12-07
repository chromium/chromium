// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ClusterElement} from './cluster.js';

export function getHtml(this: ClusterElement) {
  return html`
<div id="container" @visit-clicked="${this.onVisitClicked_}"
    @open-all-visits="${this.onOpenAllVisits_}"
    @hide-all-visits="${this.onHideAllVisits_}"
    @remove-all-visits="${this.onRemoveAllVisits_}"
    @hide-visit="${this.onHideVisit_}"
    @remove-visit="${this.onRemoveVisit_}">
  <div class="label-row">
    <span id="label" class="truncate"></span>
    <img is="cr-auto-img" auto-src="${this.imageUrl_}">
    <div class="debug-info">${this.debugInfo_()}</div>
    <div class="timestamp-and-menu">
      <div class="timestamp">${this.timestamp_()}</div>
      <cluster-menu></cluster-menu>
    </div>
  </div>
  ${this.visits_().map(item => html`<url-visit .visit="${item}"
      .query="${this.query}"
      .fromPersistence="${this.cluster!.fromPersistence}">
    </url-visit>`)}
  <div id="related-searches-divider" ?hidden="${this.hideRelatedSearches_()}">
  </div>
  <horizontal-carousel id="related-searches"
      ?hidden="${this.hideRelatedSearches_()}"
      role="list" aria-label="${this.i18n('relatedSearchesHeader')}"
      @related-search-clicked="${this.onRelatedSearchClicked_}"
      @pointerdown="${this.clearSelection_}"
      ?in-side-panel="${this.inSidePanel}">
    ${this.relatedSearches_.map((item, index) => html`<search-query
        .searchQuery="${item}" .index="${index}" role="listitem">
      </search-query>`)}
  </horizontal-carousel>
</div>`;
}
