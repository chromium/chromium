// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxElement} from './searchbox.js';
import {getHtml as getContextualEntrypointHtml} from './searchbox_contextual_entrypoint.html.js';
import {getHtml as getDropdownHtml} from './searchbox_searchbox_dropdown.html.js';
import {getHtml as getRecentTabChipHtml} from './searchbox_recent_tab_chip.html.js';

export function getHtml(this: SearchboxElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="inputWrapper" @focusout="${this.onInputWrapperFocusout_}"
    @keydown="${this.onInputWrapperKeydown_}"
    @dragenter="${this.dragAndDropHandler?.handleDragEnter}"
    @dragover="${this.dragAndDropHandler?.handleDragOver}"
    @dragleave="${this.dragAndDropHandler?.handleDragLeave}"
    @drop="${this.dragAndDropHandler?.handleDrop}">
  ${this.ntpRealboxNextEnabled ?
    html`
      <search-animated-glow animation-state="${this.animationState}" part="animated-glow">
      </search-animated-glow>
    ` : ''}
  <div id="inputInnerContainer">
    ${this.ntpRealboxNextEnabled && this.useCompactLayout_() ? html`
      <div class="contextualEntrypointContainer contextualEntrypointContainerCompact">
        ${getContextualEntrypointHtml.bind(this)()}
      </div>
    ` : ''}
    <cr-searchbox-icon id="icon" .match="${this.selectedMatch_}"
        default-icon="${this.searchboxIcon_}" in-searchbox>
    </cr-searchbox-icon>
    ${this.showThumbnail ? html`
      <div id="thumbnailContainer">
        <cr-searchbox-thumbnail id="thumbnail"
            thumbnail-url="${this.thumbnailUrl_}"
            ?is-deletable="${this.isThumbnailDeletable_}"
            @remove-thumbnail-click="${this.onRemoveThumbnailClick_}"
            role="button" aria-label="${this.i18n('searchboxThumbnailLabel')}"
            tabindex="${this.getThumbnailTabindex_()}">
        </cr-searchbox-thumbnail>
      </div>
    ` : ''}
    ${this.multiLineEnabled ? html`
      <textarea id="input" autocomplete="off"
          part="searchbox-input"
          spellcheck="false" aria-live="${this.inputAriaLive_}" role="combobox"
          aria-expanded="${this.dropdownIsVisible}" aria-controls="matches"
          aria-description="${this.searchboxAriaDescription}"
          placeholder="${this.computePlaceholderText_(this.placeholderText)}"
          @copy="${this.onInputCutCopy_}"
          @cut="${this.onInputCutCopy_}" @focus="${this.onInputFocus_}"
          @focusout="${this.onInputFocusout_}"
          @input="${this.onInputInput_}" @keydown="${this.onInputKeydown_}"
          @keyup="${this.onInputKeyup_}" @mousedown="${this.onInputMouseDown_}"
          @paste="${this.onInputPaste_}"></textarea>
    ` : html`
      <input id="input" class="truncate" type="search" autocomplete="off"
          part="searchbox-input"
          spellcheck="false" aria-live="${this.inputAriaLive_}" role="combobox"
          aria-expanded="${this.dropdownIsVisible}" aria-controls="matches"
          aria-description="${this.searchboxAriaDescription}"
          placeholder="${this.computePlaceholderText_(this.placeholderText)}"
          @copy="${this.onInputCutCopy_}"
          @cut="${this.onInputCutCopy_}" @focus="${this.onInputFocus_}"
          @focusout="${this.onInputFocusout_}"
          @input="${this.onInputInput_}" @keydown="${this.onInputKeydown_}"
          @keyup="${this.onInputKeyup_}" @mousedown="${this.onInputMouseDown_}"
          @paste="${this.onInputPaste_}">
      </input>
    `}
    ${!this.ntpRealboxNextEnabled || this.useCompactLayout_() ? html`
      ${this.searchboxVoiceSearchEnabled_ ? html`
        <div class="searchbox-icon-button-container voice">
          <button id="voiceSearchButton" class="searchbox-icon-button"
              @click="${this.onVoiceSearchClick_}"
              title="${this.i18n('voiceSearchButtonLabel')}">
          </button>
        </div>
      ` : ''}
      ${this.searchboxLensSearchEnabled_ ? html`
        <div class="searchbox-icon-button-container lens">
          <button id="lensSearchButton" class="searchbox-icon-button lens"
              @click="${this.onLensSearchClick_}"
              title="${this.i18n('lensSearchButtonLabel')}">
          </button>
        </div>
      ` : ''}
    ` : ''}
    ${this.composeButtonEnabled ? html`
      <cr-searchbox-compose-button id="composeButton"
          @compose-click="${this.onComposeButtonClick_}">
      </cr-searchbox-compose-button>
    ` : ''}
  </div>
  ${this.ntpRealboxNextEnabled ? html`
    ${this.useCompactLayout_() ? html`
      <div class="dropdownContainer">
        ${getDropdownHtml.bind(this)()}
        ${getRecentTabChipHtml.bind(this)()}
      </div>
    ` : html`
      <div id="inputInnerBottomContainer">
        <div class="contextualEntrypointContainer">
          ${getContextualEntrypointHtml.bind(this)()}
          ${this.dropdownIsVisible ?
              html`<div class="carousel-divider"></div>` : ''}
          ${getDropdownHtml.bind(this)()}
        </div>
        ${this.searchboxVoiceSearchEnabled_ ? html`
          <div class="searchbox-icon-button-container voice">
            <button id="voiceSearchButton" class="searchbox-icon-button"
                @click="${this.onVoiceSearchClick_}"
                title="${this.i18n('voiceSearchButtonLabel')}">
            </button>
          </div>
        ` : ''}
        ${this.searchboxLensSearchEnabled_ ? html`
          <div class="searchbox-icon-button-container lens">
            <button id="lensSearchButton" class="searchbox-icon-button lens"
                @click="${this.onLensSearchClick_}"
                title="${this.i18n('lensSearchButtonLabel')}">
            </button>
          </div>
        ` : ''}
      </div>
    `}
  ` : getDropdownHtml.bind(this)()}
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
