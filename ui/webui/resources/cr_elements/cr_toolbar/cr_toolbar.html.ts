// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {CrToolbarElement} from './cr_toolbar.js';
import {nothing} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: CrToolbarElement) {
  return html`
<div id="leftContent">
  <div id="leftSpacer">
    ${this.showMenu ? html`
      <cr-icon-button id="menuButton" class="no-overlap"
          iron-icon="cr20:menu" @click="${this.onMenuClick_}"
          aria-label="${this.menuLabel || nothing}"
          title="${this.menuLabel}">
      </cr-icon-button>` : ''}
    <slot name="product-logo">
      <picture>
        <source media="(prefers-color-scheme: dark)"
            srcset="//resources/images/chrome_logo_dark.svg">
        <img id="product-logo"
            srcset="chrome://theme/current-channel-logo@1x 1x,
                    chrome://theme/current-channel-logo@2x 2x"
            role="presentation">
      </picture>
    </slot>
    <h1>${this.pageName}</h1>
  </div>
</div>

<div id="centeredContent" ?hidden="${!this.showSearch}">
  <cr-toolbar-search-field id="search" ?narrow="${this.narrow}"
      label="${this.searchPrompt}" clear-label="${this.clearLabel}"
      ?spinner-active="${this.spinnerActive}"
      ?showing-search="${this.showingSearch_}"
      @showing-search-changed="${this.onShowingSearchChanged_}"
      ?autofocus="${this.autofocus}" icon-override="${this.searchIconOverride}">
  </cr-toolbar-search-field>
  <iron-media-query query="(max-width: ${this.narrowThreshold}px)"
      ?query-matches="${this.narrow}"
      @query-matches-changed="${this.onQueryMatchesChanged_}">
  </iron-media-query>
</div>

<div id="rightContent">
  <div id="rightSpacer">
    <slot></slot>
  </div>
</div>`;
}
