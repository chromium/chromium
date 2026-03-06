// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {SearchboxInputElement} from './searchbox_input.js';

export function getHtml(this: SearchboxInputElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="inputInnerContainer" part="input-inner-container">
    <slot name="contextual-entrypoint"></slot>
    <cr-searchbox-icon id="icon" .match="${this.selectedMatch}"
        default-icon="${this.searchboxIcon}" in-searchbox part="icon">
    </cr-searchbox-icon>
    <slot name="thumbnail"></slot>
    ${this.multiLineEnabled ? html`
      <textarea id="input" autocomplete="off"
          part="searchbox-input"
          spellcheck="false" aria-live="${this.inputAriaLive}" role="combobox"
          aria-expanded="${this.dropdownIsVisible}" aria-controls="matches"
          aria-description="${this.searchboxAriaDescription}"
          placeholder="${this.computePlaceholderText_()}"
          @copy="${this.onInputCopy_}"
          @cut="${this.onInputCut_}"
          @input="${this.onInputInput_}"
          @keydown="${this.onInputKeydown_}"
          @keyup="${this.onInputKeyup_}"
          @mousedown="${this.onInputMousedown_}"
          @paste="${this.onInputPaste_}"></textarea>
    ` : html`
      <input id="input" class="truncate" type="search" autocomplete="off"
          part="searchbox-input"
          spellcheck="false" aria-live="${this.inputAriaLive}" role="combobox"
          aria-expanded="${this.dropdownIsVisible}" aria-controls="matches"
          aria-description="${this.searchboxAriaDescription}"
          placeholder="${this.computePlaceholderText_()}"
          @copy="${this.onInputCopy_}"
          @cut="${this.onInputCut_}"
          @input="${this.onInputInput_}"
          @keydown="${this.onInputKeydown_}"
          @keyup="${this.onInputKeyup_}"
          @mousedown="${this.onInputMousedown_}"
          @paste="${this.onInputPaste_}">
    `}
    <slot name="action-buttons"></slot>
    <slot name="compose-button"></slot>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
