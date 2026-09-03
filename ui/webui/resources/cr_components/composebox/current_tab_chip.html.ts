// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CurrentTabChipElement} from './current_tab_chip.js';

export function getHtml(this: CurrentTabChipElement) {
  // clang-format off
  return html`${this.currentTab ? html`<!--_html_template_start_-->
  <cr-button id="currentTabButton"
      @click="${this.onCurrentTabButtonClick_}"
      title="${this.getCurrentTabChipTitle_()}"
      aria-label="${this.i18n('askAboutTabAriaLabel',
          this.getCurrentTabChipTitle_())}">
    <span class="current-tab-button-text">
      ${this.i18n('askAboutTab')}
    </span>
  </cr-button>
<!--_html_template_end_-->` : ''}`;
  //clang-format on
}
