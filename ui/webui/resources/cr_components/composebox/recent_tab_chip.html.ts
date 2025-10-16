// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RecentTabChipElement} from './recent_tab_chip.js';

export function getHtml(this: RecentTabChipElement) {
  // clang-format off
  return this.recentTab_ ? html`<!--_html_template_start_-->
  <cr-button id="recentTabButton"
      @click="${this.addTabContext_}"
      ?disabled="${this.inputsDisabled_}"
      title="${this.recentTab_.title}"
      aria-label="${this.i18n('askAboutThisTabAriaLabel',
          this.recentTab_.title)}">
    <div class="button-content">
      <composebox-tab-favicon
          class="favicon"
          .url="${this.recentTab_.url?.url}">
      </composebox-tab-favicon>
      <span class="recent-tab-button-text">
        ${this.i18n('askAboutThisTab')}
      </span>
    </div>
  </cr-button>
<!--_html_template_end_-->` : '';
  //clang-format on
}
