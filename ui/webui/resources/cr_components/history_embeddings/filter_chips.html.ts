// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {FilterChipsElement} from './filter_chips.js';

export function getHtml(this: FilterChipsElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<select id="showByGroupSelectMenu" class="md-select"
    aria-label="${this.i18n('historyEmbeddingsShowByLabel')}"
    .value="${this.showResultsByGroup}"
    @change="${this.onShowByGroupSelectMenuChanged_}"
    ?hidden="${!this.enableShowResultsByGroupOption}">
  <option value="false">
    ${this.i18n('historyEmbeddingsShowByDate')}
  </option>
  <option value="true">
    ${this.i18n('historyEmbeddingsShowByGroup')}
  </option>
</select>

<hr ?hidden="${!this.enableShowResultsByGroupOption}"></hr>

<div id="suggestions">
  ${this.suggestions_.map((item, index) => html`
    <cr-chip @click="${this.onSuggestionClick_}" data-index="${index}"
        ?selected="${this.isSuggestionSelected_(item)}"
        chip-aria-label="${item.ariaLabel}">
      <cr-icon icon="cr:check" ?hidden="${!this.isSuggestionSelected_(item)}">
      </cr-icon>
      <span class="suggestion-label">${item.label}</span>
    </cr-chip>
  `)}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
