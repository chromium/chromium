// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {CustomizeColorSchemeModeElement} from './customize_color_scheme_mode.js';

export function getHtml(this: CustomizeColorSchemeModeElement) {
  return html`
<segmented-button
    selected="${this.currentMode_.id}"
    group-aria-label="${this.i18n('colorSchemeModeLabel')}"
    @selected-changed="${this.onSelectedChanged_}">
  ${this.colorSchemeModeOptions_.map(item => html`
    <segmented-button-option name="${item.id}" title="${this.i18n(item.id)}">
      <div id="${item.id}" class="cr-icon" slot="prefix-icon"></div>
      ${this.i18n(item.id)}
    </segmented-button-option>
  `)}
</segmented-button>`;
}
