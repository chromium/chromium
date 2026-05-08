// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ComposeboxInputElement} from './composebox_input.js';

export function getHtml(this: ComposeboxInputElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
      <div id="textContainer" part="text-container">
        <div id="iconContainer" part="icon-container">
          <div id="aimIcon"></div>
        </div>
        <div id="inputWrapper">
          ${!this.disableCaretColorAnimation ? html`
            <div id="mirror" part="mirror" aria-hidden="true"></div>
            <div id="caret"></div>
          ` : ''}
          <textarea
            aria-expanded="${this.showDropdown}" aria-controls="matches"
            role="combobox" autocomplete="off" id="input"
            type="search" spellcheck="false"
            placeholder="${this.inputPlaceholder}"
            part="input"
            .value="${this.input}"
            @click="${this.onInputClick_}"
            @keydown="${this.onInputKeydown_}"
            @keyup="${this.onInputKeyup_}"
            @input="${this.onInputInput_}"
            @focusin="${this.onInputFocusin_}"
            @focus="${this.onInputFocus_}"
            @blur="${this.onInputBlur_}"></textarea>
          ${this.showSmartComposeInlineHint_() ? html`
            <div id="smartCompose" part="smart-compose">
              <!-- Comments in between spans to eliminate spacing between
                   spans -->
              <span id="invisibleText">${this.input}</span><!--
              --><span id="ghostText">${this.smartComposeInlineHint}</span><!--
              --><span id="tabChip">${this.i18n('composeboxSmartComposeTabTitle')}</span>
            </div>
          `: ''}
        </div>
      </div>
      <!-- A seperate container is needed for the submit button so the
      expand/collapse animation can be applied without affecting the submit
      button enabled/disabled state. -->
      <div id="cancelContainer" class="icon-fade" part="cancel">
        <cr-icon-button
            class="action-icon icon-clear"
            id="cancelIcon"
            part="action-icon cancel-icon"
            title="${this.cancelButtonTitle}"
            @click="${this.onCancelClick_}"
            ?disabled="${this.isCollapsible && !this.submitEnabled}">
        </cr-icon-button>
      </div>
<!--_html_template_end_-->`;
  // clang-format on
}
