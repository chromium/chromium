// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrToolbarSearchFieldElement} from './cr_toolbar_search_field.js';

export function getHtml(this: CrToolbarSearchFieldElement) {
  // clang-format off
  return html`
<div id="background"></div>
<div id="stateBackground"></div>
<div id="content">
  ${this.shouldShowSpinner_() ? html`
    <div class="spinner"></div>` : ''}
    <cr-icon-button id="icon" iron-icon="${this.iconOverride || 'cr:search'}"
        title="${this.label}" tabindex="${this.getIconTabIndex_()}"
        aria-hidden="${this.getIconAriaHidden_()}" suppress-rtl-flip
        @click="${this.onSearchIconClicked_}" ?disabled="${this.disabled}">
  </cr-icon-button>
  <div id="searchTerm">
    <label id="prompt" for="searchInput" aria-hidden="true">
      ${this.label}
    </label>
    <input id="searchInput"
        aria-labelledby="prompt"
        aria-description="${this.inputAriaDescription}"
        autocapitalize="off"
        autocomplete="off"
        type="search"
        @beforeinput="${this.onSearchTermNativeBeforeInput}"
        @input="${this.onSearchTermNativeInput}"
        @search="${this.onSearchTermSearch}"
        @keydown="${this.onSearchTermKeydown_}"
        @focus="${this.onInputFocus_}"
        @blur="${this.onInputBlur_}"
        ?autofocus="${this.autofocus}"
        spellcheck="false"
        ?disabled="${this.disabled}">
  </div>
  ${this.hasSearchText ? html`
    <cr-icon-button id="clearSearch" iron-icon="cr:cancel"
        title="${this.clearLabel}" @click="${this.clearSearch_}"
        ?disabled="${this.disabled}"></cr-icon-button>` : ''}
</div>`;
  // clang-format on
}
