// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, nothing} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrTextareaElement} from './cr_textarea.js';

export function getHtml(this: CrTextareaElement) {
  return html`
<div id="label" class="cr-form-field-label" ?hidden="${!this.label}"
    aria-hidden="true">
  ${this.label}
</div>
<div id="input-container">
  <div id="hover-layer"></div>
  <textarea id="input" ?autofocus="${this.autofocus}" .rows="${this.rows}"
     .value="${this.internalValue_}" aria-label="${this.label}"
     @input="${this.onInput_}" @focus="${this.onFocus_}"
     @blur="${this.onBlur_}" @change="${this.onInputChange_}"
     ?disabled="${this.disabled}" maxlength="${this.maxlength}"
     ?readonly="${this.readonly}" ?required="${this.required}"
     placeholder="${this.placeholder || nothing}">
  </textarea>
  <div id="underline-base"></div>
  <div id="underline"></div>
</div>
<div id="footerContainer" class="cr-row">
  <div id="firstFooter" aria-live="${this.getFooterAria_()}">
    ${this.firstFooter}
  </div>
  <div id="secondFooter" aria-live="${this.getFooterAria_()}">
    ${this.secondFooter}
  </div>
</div>`;
}
