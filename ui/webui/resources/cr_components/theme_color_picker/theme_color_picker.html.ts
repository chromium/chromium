// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ThemeColorPickerElement} from './theme_color_picker.js';

export function getHtml(this: ThemeColorPickerElement) {
  // clang-format off
  return html`
<!-- TODO(crbug.com/40881996): Make grid adaptive. -->
<cr-grid columns="${this.columns}" role="radiogroup"
    aria-label="${this.i18n('colorsContainerLabel')}">
  <cr-theme-color
      id="defaultColor"
      .backgroundColor="${this.defaultColor_.background}"
      .baseColor="${this.defaultColor_.base}"
      .foregroundColor="${this.defaultColor_.foreground}"
      title="${this.i18n('defaultColorName')}"
      aria-label="${this.i18n('defaultColorName')}"
      role="radio"
      ?checked="${this.isDefaultColorSelected_}"
      aria-checked="${this.isDefaultColorSelected_}"
      tabindex="${this.tabIndex_(this.isDefaultColorSelected_)}"
      @click="${this.onDefaultColorClick_}">
  </cr-theme-color>
  <cr-theme-color
      id="greyDefaultColor"
      .backgroundColor="${this.greyDefaultColor_.background}"
      .baseColor="${this.greyDefaultColor_.base}"
      .foregroundColor="${this.greyDefaultColor_.foreground}"
      title="${this.i18n('greyDefaultColorName')}"
      aria-label="${this.i18n('greyDefaultColorName')}"
      role="radio"
      ?checked="${this.isGreyDefaultColorSelected_}"
      aria-checked="${this.isGreyDefaultColorSelected_}"
      tabindex="${this.tabIndex_(this.isGreyDefaultColorSelected_)}"
      @click="${this.onGreyDefaultColorClick_}">
  </cr-theme-color>
  ${this.colors_.map((item, index) => html`
    <cr-theme-color
        class="chrome-color"
        .backgroundColor="${item.background}"
        .baseColor="${item.base}"
        .foregroundColor="${item.foreground}"
        title="${item.name}"
        aria-label="${item.name}"
        role="radio"
        ?checked="${this.isChromeColorSelected_(item.seed, item.variant)}"
        aria-checked=
            "${this.isChromeColorSelected_(item.seed, item.variant)}"
        tabindex="${this.chromeColorTabIndex_(item.seed, item.variant)}"
        data-index="${index}"
        @click="${this.onChromeColorClick_}">
    </cr-theme-color>
  `)}
  <div id="customColorContainer"
      title="${this.i18n('colorPickerLabel')}"
      aria-label="${this.i18n('colorPickerLabel')}"
      role="radio"
      aria-checked="${this.isCustomColorSelected_}"
      tabindex="${this.tabIndex_(this.isCustomColorSelected_)}"
      @click="${this.onCustomColorClick_}">
    <cr-theme-color
        id="customColor"
        .backgroundColor="${this.customColor_.background}"
        .foregroundColor="${this.customColor_.foreground}"
        background-color-hidden
        ?checked="${this.isCustomColorSelected_}">
    </cr-theme-color>
    <div id="colorPickerIcon"></div>
  </div>
</cr-grid>

<cr-theme-hue-slider-dialog id="hueSlider"
    @selected-hue-changed="${this.onSelectedHueChanged_}">
</cr-theme-hue-slider-dialog>

${this.showManagedDialog_ ? html`
  <managed-dialog @close="${this.onManagedDialogClosed_}"
      title="${this.i18n('managedColorsTitle')}"
      body="${this.i18n('managedColorsBody')}">
  </managed-dialog>
` : ''}`;
  // clang-format on
}
