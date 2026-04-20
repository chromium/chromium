// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './searchbox_input.js';

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';
import {getHtml as getDropdownHtml} from './searchbox_searchbox_dropdown.html.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="inputWrapper" @focusout="${this.onInputWrapperFocusout}"
    @keydown="${this.onInputWrapperKeydown}">
  <cr-searchbox-input id="input"
      exportparts="searchbox-input"
      ?dropdown-is-visible="${this.dropdownIsVisible}"
      input-aria-live="${this.inputAriaLive}"
      ?multi-line-enabled="${this.multiLineEnabled}"
      placeholder-text="${this.computePlaceholderText_(this.placeholderText)}"
      searchbox-aria-description="${this.searchboxAriaDescription}"
      searchbox-icon="${this.searchboxIcon_}"
      .selectedMatch="${this.selectedMatch}"
      ?input-has-matches="${this.hasMatches()}"
      @focusin="${this.onInputFocusin_}"
      @searchbox-input-text-updated="${this.onSearchboxInputTextUpdated_}"
      @input-focus-changed="${this.onInputFocusChanged}">
    ${this.shouldShowVoiceLens_(this.searchboxVoiceSearchEnabled_) ? html`
      <div slot="action-buttons" class="searchbox-icon-button-container voice">
        <button id="voiceSearchButton" class="searchbox-icon-button"
            @click="${this.onVoiceSearchClick_}"
            title="${this.i18n('voiceSearchButtonLabel')}">
        </button>
      </div>
    `: ''}
    ${this.shouldShowVoiceLens_(this.searchboxLensSearchEnabled_) ? html`
      <div slot="action-buttons" class="searchbox-icon-button-container lens">
        <button id="lensSearchButton" class="searchbox-icon-button lens"
            @click="${this.onLensSearchClick_}"
            title="${this.i18n('lensSearchButtonLabel')}">
        </button>
      </div>
    ` : ''}
  </cr-searchbox-input>
  <div class="dropdownContainer">
    ${getDropdownHtml.bind(this)()}
  </div>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
