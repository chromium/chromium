// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

import type {ThemeColorPickerElement} from './theme_color_picker.js';

export function getHtml(this: ThemeColorPickerElement) {
  // clang-format off
  return html`
<!-- TODO(crbug.com/1395210): Make grid adaptive. -->
<cr-grid columns="${this.columns}" role="radiogroup"
    aria-label="${this.i18n('colorsContainerLabel')}">
  ${!this.showMainColor_ ? html`
    <cr-theme-color
        id="defaultColor"
        .backgroundColor="${this.defaultColor_.background}"
        .baseColor="${this.defaultColor_.base}"
        .foregroundColor="${this.defaultColor_.foreground}"
        ?background-color-hidden="${!this.showBackgroundColor_}"
        title="${this.i18n('defaultColorName')}"
        aria-label="${this.i18n('defaultColorName')}"
        role="radio"
        ?checked="${this.isDefaultColorSelected_}"
        ?checkmark-border-hidden="${this.isChromeRefresh2023_}"
        aria-checked="${this.isDefaultColorSelected_}"
        tabindex="${this.tabIndex_(this.isDefaultColorSelected_)}"
        @click="${this.onDefaultColorClick_}"
        basic-color>
    </cr-theme-color>
  ` : ''}
  ${this.isChromeRefresh2023_ ? html`
    <cr-theme-color
        id="greyDefaultColor"
        .backgroundColor="${this.greyDefaultColor_.background}"
        .baseColor="${this.greyDefaultColor_.base}"
        .foregroundColor="${this.greyDefaultColor_.foreground}"
        title="${this.i18n('greyDefaultColorName')}"
        aria-label="${this.i18n('greyDefaultColorName')}"
        role="radio"
        ?checked="${this.isGreyDefaultColorSelected_}"
        ?checkmark-border-hidden="${this.isChromeRefresh2023_}"
        aria-checked="${this.isGreyDefaultColorSelected_}"
        tabindex="${this.tabIndex_(this.isGreyDefaultColorSelected_)}"
        @click="${this.onGreyDefaultColorClick_}"
        basic-color>
    </cr-theme-color>
  ` : ''}
  ${this.showMainColor_ ? html`
    <cr-theme-color
        id="mainColor"
        .foregroundColor="${this.mainColor_}"
        background-color-hidden
        title="${this.i18n('mainColorName')}"
        aria-label="${this.i18n('mainColorName')}"
        role="radio"
        ?checked="${this.isMainColorSelected_}"
        ?checkmark-border-hidden="${this.isChromeRefresh2023_}"
        aria-checked="${this.isMainColorSelected_}"
        tabindex="${this.tabIndex_(this.isMainColorSelected_)}"
        @click="${this.onMainColorClick_}"
        basic-color>
    </cr-theme-color>
  ` : ''}
  ${this.colors_.map((item, index) => html`
    <cr-theme-color
        class="chrome-color"
        .backgroundColor="${item.background}"
        .baseColor="${item.base}"
        .foregroundColor="${item.foreground}"
        ?background-color-hidden="${!this.showBackgroundColor_}"
        title="${item.name}"
        aria-label="${item.name}"
        role="radio"
        ?checked="${this.isChromeColorSelected_(item.seed, item.variant)}"
        ?checkmark-border-hidden="${this.isChromeRefresh2023_}"
        aria-checked=
            "${this.isChromeColorSelected_(item.seed, item.variant)}"
        tabindex="${this.chromeColorTabIndex_(item.seed, item.variant)}"
        data-index="${index}"
        @click="${this.onChromeColorClick_}"
        basic-color>
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
        ?background-color-hidden="${!this.showCustomColorBackgroundColor_}"
        ?checked="${this.isCustomColorSelected_}"
        ?checkmark-border-hidden="${this.isChromeRefresh2023_}"
        basic-color>
    </cr-theme-color>
    <div id="colorPickerIcon"></div>
    <input id="colorPicker" type="color" tabindex="-1" aria-hidden="true"
        @change="${this.onCustomColorChange_}">
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
