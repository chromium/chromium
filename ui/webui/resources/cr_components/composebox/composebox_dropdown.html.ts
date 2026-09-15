// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxDropdownElement} from './composebox_dropdown.js';

export function getHtml(this: ComposeboxDropdownElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
    ${this.richImageSuggestionsEnabled ? html`
      ${this.groupIds_().map(groupId => html`
        ${this.hasHeaderForGroup_(groupId) ? html`
          <div class="header" id="header_${groupId}"
              tabindex="-1"
              @mousedown="${this.onHeaderMousedown_}">
            ${this.headerForGroup_(groupId)}
          </div>
        ` : ''}
        <div class="matches ${this.renderTypeClassForGroup_(groupId)}">
          ${this.matchesForGroup_(groupId).map(({match, index}) => html`
            <div part="match-wrapper" style="--match-wrapper-index: ${index};">
              <cr-composebox-match
                  id="match${index}"
                  aria-label="${this.computeAriaLabel_(match)}"
                  exportparts="match-text-container, match-container,
                      match-icon-container, match-focus-indicator,
                      match-icon, match-remove-button"
                  style="--loading-bar-animation-delay: ${index + 1};"
                  tabindex="0"
                  role="option"
                  .match="${match}"
                  .matchIndex="${index}"
                  .toolMode="${this.toolMode}"
                  .overrideClampLineNum="${this.overrideClampLineNum}"
                  .richImageSuggestionsEnabled="${
                      this.richImageSuggestionsEnabled}"
                  ?selected="${this.isSelected_(index)}"
                  ?is-last="${this.isLastMatch_(index)}"
                  ?hidden="${this.isMatchHidden_(index)}">
              </cr-composebox-match>
            </div>
          `)}
        </div>
      `)}
    ` : html`
      <div class="matches vertical">
        ${this.result?.matches.map((item, index) => html`
          <div part="match-wrapper" style="--match-wrapper-index: ${index};">
            <cr-composebox-match
                id="match${index}"
                aria-label="${this.computeAriaLabel_(item)}"
                exportparts="match-text-container, match-container,
                    match-icon-container, match-focus-indicator,
                    match-icon, match-remove-button"
                style="--loading-bar-animation-delay: ${index + 1};"
                tabindex="0"
                role="option"
                .match="${item}"
                .matchIndex="${index}"
                .toolMode="${this.toolMode}"
                .overrideClampLineNum="${this.overrideClampLineNum}"
                ?selected="${this.isSelected_(index)}"
                ?is-last="${this.isLastMatch_(index)}"
                ?hidden="${this.isMatchHidden_(index)}">
            </cr-composebox-match>
          </div>
        `)}
      </div>
    `}
  <!--_html_template_end_-->`;
  // clang-format on
}
