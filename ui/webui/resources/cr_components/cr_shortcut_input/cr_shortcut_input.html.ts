// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrShortcutInputElement} from './cr_shortcut_input.js';

export function getHtml(this: CrShortcutInputElement) {
  // clang-format off
  return html`<!--_html_template_start_-->
<div id="main">
  <cr-input id="input" ?readonly="${this.readonly_}"
      aria-label="${this.inputAriaLabel}"
      .placeholder="${this.computePlaceholder_()}"
      ?invalid="${this.getIsInvalid_()}"
      .errorMessage="${this.getErrorString_()}"
      ?disabled="${this.inputDisabled}"
      .inputTabindex="${this.readonly_ ? -1 : 0}"
      .value="${this.computeText_()}">
    <cr-icon-button id="edit" title="$i18n{edit}"
        aria-label="${this.editButtonAriaLabel}"
        slot="suffix" class="icon-edit no-overlap"
        ?disabled="${this.inputDisabled}"
        @click="${this.onEditClick_}">
    </cr-icon-button>
  </cr-input>
</div>
<!--_html_template_end_-->`;
  // clang-format on
}
