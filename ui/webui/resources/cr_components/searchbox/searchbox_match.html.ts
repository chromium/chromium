// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxMatchElement} from './searchbox_match.js';

export function getHtml(this: SearchboxMatchElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div class="container" aria-hidden="true">
  <div id="focus-indicator" class="${this.getFocusIndicatorCssClass_()}"></div>
  <cr-searchbox-icon id="icon" .match="${this.match}"></cr-searchbox-icon>
  <div id="text-container">
    <span id="tail-suggest-prefix" ?hidden="${!this.tailSuggestPrefix_}">
      <span id="prefix">${this.tailSuggestPrefix_}</span>
      <!-- This is equivalent to AutocompleteMatch::kEllipsis which is
           prepended to the match content in other surfaces-->
      <span id="ellipsis">...&nbsp;</span>
    </span>
    <!-- When a thumbnail is in the searchbox all results should have an
         ellipsis prepended to the suggestion. -->
    <span id="ellipsis" ?hidden="${!this.showEllipsis}">...&nbsp;</span>
    <span id="suggestion">
      <span id="contents" .innerHTML="${this.contentsHtml_}"></span>
      <span id="separator" class="dim">${this.separatorText_}</span>
      <span id="description" .innerHTML="${this.descriptionHtml_}"></span>
    </span>
  </div>
  <div aria-hidden="true">
    ${this.match.keywordChipHint ? html`
      <div id="actions-focus-border">
        <cr-searchbox-action id="keyword"
            class="${this.getKeywordCssClass_()}"
            hint="${this.match.keywordChipHint}"
            icon-path="//resources/images/icon_search.svg"
            aria-label="${this.match.keywordChipA11y}"
            @execute-action="${this.onActivateKeyword_}"
            tabindex="1">
        </cr-searchbox-action>
      </div>
    ` : ''}
  </div>
  <div id="actions-container" class="actions container" aria-hidden="true">
    ${this.match.actions.map((item, index) => html`
      <div id="actions-focus-border">
        <cr-searchbox-action id="action"
            class="${this.getActionCssClass_(index)}"
            hint="${item.hint}"
            suggestion-contents="${item.suggestionContents}"
            icon-path="${item.iconPath}"
            aria-label="${item.a11yLabel}"
            action-index="${index}"
            @execute-action="${this.onExecuteAction_}" tabindex="2">
        </cr-searchbox-action>
      </div>
    `)}
  </div>
  <cr-icon-button id="remove"
      class="action-icon icon-clear ${this.getRemoveCssClass_()}"
      tabindex="3"
      aria-label="${this.removeButtonAriaLabel_}"
      title="${this.removeButtonTitle_}"
      ?hidden="${!this.match.supportsDeletion}"
      @click="${this.onRemoveButtonClick_}"
      @mousedown="${this.onRemoveButtonMouseDown_}">
  </cr-icon-button>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
