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
        page-url="${this.pageUrl}"
        default-icon="${this.computeDefaultIcon_()}"
        ?in-keyword-mode="${this.inKeywordMode_()}"
        in-searchbox part="icon" aria-hidden="true">
    </cr-searchbox-icon>
    <slot name="thumbnail"></slot>
    ${this.inKeywordMode_() ?
      html`<span id="keyword">${
          this.inputKeywordModel!.displayText}</span>` : ''}
    <!-- Target of the input's aria-activedescendant while a selection is
         active. It distracts screen readers from the input's value, which
         previews the selection, so the notification of the selection's label
         wins over the value change. Must share the input's shadow root for
         element reflection to work. It is an option of a listbox the combobox
         controls because that is the one kind of active descendant VoiceOver
         accepts as keyboard focus after a key press: told that focus is on a
         plain group, it re-anchors keyboard focus on the web area, which blurs
         the input and closes the popup, whatever the group's position or
         focusability. Hidden, hence out of the accessibility tree, while there
         is no selection (see setSelectionA11yLabel()). -->
    <div id="selectionListbox" role="listbox" hidden>
      <div id="selectionAnnouncement" role="option" aria-selected="true"></div>
    </div>
    <div id="mirrorContainer">
      <div id="mirror" part="mirror" aria-hidden="true"
          ?hidden="${!this.urlEmphasisEnabled}"></div>
      ${this.multiLineEnabled ? html`
        <textarea id="input" autocomplete="off"
            part="searchbox-input"
            spellcheck="false" role="combobox"
            aria-expanded="${this.dropdownIsVisible}"
            aria-controls="selectionListbox"
            aria-description="${this.searchboxAriaDescription}"
            placeholder="${this.computePlaceholderText_()}"
            @blur="${this.onInputBlur_}"
            @focus="${this.onInputFocus_}"
            @copy="${this.onInputCopy_}"
            @cut="${this.onInputCut_}"
            @input="${this.onInputInput_}"
            @keydown="${this.onInputKeydown_}"
            @keyup="${this.onInputKeyup_}"
            @mousedown="${this.onInputMousedown_}"
            @paste="${this.onInputPaste_}"
            @scroll="${this.onInputScroll_}"></textarea>
      ` : html`
        <input id="input" class="truncate" type="search" autocomplete="off"
            part="searchbox-input"
            spellcheck="false" role="combobox"
            aria-expanded="${this.dropdownIsVisible}"
            aria-controls="selectionListbox"
            aria-description="${this.searchboxAriaDescription}"
            placeholder="${this.computePlaceholderText_()}"
            @blur="${this.onInputBlur_}"
            @focus="${this.onInputFocus_}"
            @copy="${this.onInputCopy_}"
            @cut="${this.onInputCut_}"
            @input="${this.onInputInput_}"
            @keydown="${this.onInputKeydown_}"
            @keyup="${this.onInputKeyup_}"
            @mousedown="${this.onInputMousedown_}"
            @paste="${this.onInputPaste_}"
            @scroll="${this.onInputScroll_}">
      `}
    </div>
    <span id="ellipsisIndicator" aria-hidden="true">...</span>
    <slot name="action-buttons"></slot>
    <slot name="compose-button"></slot>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
