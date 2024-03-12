// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CrTabsElement} from './cr_tabs.js';

export function getHtml(this: CrTabsElement) {
  return html`${this.tabNames.map((item, index) => html`
<div role="tab"
    class="tab ${this.getSelectedClass_(index)}"
    aria-selected="${this.getAriaSelected_(index)}"
    tabindex="${this.getTabindex_(index)}"
    data-index="${index}" @click="${this.onTabClick_}">
  <div class="tab-icon" .style="${this.getIconStyle_(index)}"></div>
  ${item}
  <div class="tab-indicator-background"></div>
  <div class="tab-indicator"></div>
</div>
`)}`;
}
