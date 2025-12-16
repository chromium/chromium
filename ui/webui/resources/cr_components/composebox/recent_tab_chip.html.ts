// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {RecentTabChipElement} from './recent_tab_chip.js';

export function getHtml(this: RecentTabChipElement) {
  // clang-format off
  return this.recentTab ? html`<!--_html_template_start_-->
  <cr-button id="recentTabButton"
      @click="${this.addTabContext_}"
      title="${this.recentTab.title}"
      aria-label="${this.recentTab.showInCurrentTabChip ?
          this.i18n('askAboutThisPageAriaLabel',
          this.recentTab.title) : this.i18n('askAboutThisTabAriaLabel',
          this.recentTab.title)}">
    <div class="button-content">
      <cr-composebox-tab-favicon
          class="favicon"
          .url="${this.recentTab.url?.url}">
      </cr-composebox-tab-favicon>
      <span class="recent-tab-button-text">
        ${this.recentTab.showInCurrentTabChip ? this.i18n('askAboutThisPage') :
            this.i18n('askAboutThisTab')}
      </span>
    </div>
  </cr-button>
<!--_html_template_end_-->` : '';
  //clang-format on
}
