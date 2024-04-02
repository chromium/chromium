// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './theme_hue_slider_dialog.js';
import './theme_color.js';
import '//resources/cr_elements/cr_grid/cr_grid.js';
import '//resources/cr_components/managed_dialog/managed_dialog.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {hexColorToSkColor, skColorToRgba} from '//resources/js/color_utils.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {BrowserColorVariant} from '//resources/mojo/ui/base/mojom/themes.mojom-webui.js';

import {ThemeColorPickerBrowserProxy} from './browser_proxy.js';
import {EMPTY_COLOR} from './color_utils.js';
import type {Color, SelectedColor} from './color_utils.js';
import {ColorType, DARK_BASELINE_BLUE_COLOR, DARK_BASELINE_GREY_COLOR, DARK_DEFAULT_COLOR, LIGHT_BASELINE_BLUE_COLOR, LIGHT_BASELINE_GREY_COLOR, LIGHT_DEFAULT_COLOR} from './color_utils.js';
import type {ThemeColorElement} from './theme_color.js';
import {getCss} from './theme_color_picker.css.js';
import {getHtml} from './theme_color_picker.html.js';
import type {ChromeColor, Theme, ThemeColorPickerHandlerRemote} from './theme_color_picker.mojom-webui.js';
import type {ThemeHueSliderDialogElement} from './theme_hue_slider_dialog.js';

const ThemeColorPickerElementBase = I18nMixinLit(CrLitElement);

export interface ThemeColorPickerElement {
  $: {
    customColorContainer: HTMLElement,
    customColor: ThemeColorElement,
    colorPicker: HTMLInputElement,
    colorPickerIcon: HTMLElement,
    hueSlider: ThemeHueSliderDialogElement,
  };
}

export class ThemeColorPickerElement extends ThemeColorPickerElementBase {
  static get is() {
    return 'cr-theme-color-picker';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      defaultColor_: {type: Object},
      greyDefaultColor_: {type: Object},
      mainColor_: {type: Object},
      colors_: {type: Array},
      theme_: {type: Object},
      selectedColor_: {type: Object},
      isDefaultColorSelected_: {type: Boolean},
      isGreyDefaultColorSelected_: {type: Boolean},
      isMainColorSelected_: {type: Boolean},
      isCustomColorSelected_: {type: Boolean},
      customColor_: {type: Object},
      showManagedDialog_: {type: Boolean},
      showBackgroundColor_: {type: Boolean},
      showCustomColorBackgroundColor_: {type: Boolean},
      showMainColor_: {type: Boolean},
      isChromeRefresh2023_: {type: Boolean},
      columns: {type: Number},
    };
  }

  protected defaultColor_: Color = EMPTY_COLOR;
  protected mainColor_: SkColor|null = null;
  protected greyDefaultColor_: Color = EMPTY_COLOR;
  protected colors_: ChromeColor[] = [];
  private theme_?: Theme;
  protected selectedColor_: SelectedColor = {type: ColorType.NONE};
  protected isDefaultColorSelected_: boolean = false;
  protected isGreyDefaultColorSelected_: boolean = false;
  protected isMainColorSelected_: boolean = false;
  protected isCustomColorSelected_: boolean = false;
  protected customColor_: Color;
  private setThemeListenerId_: number|null = null;

  protected showManagedDialog_: boolean = false;
  protected showBackgroundColor_: boolean = false;
  protected showCustomColorBackgroundColor_: boolean = false;
  protected showMainColor_: boolean = false;
  protected isChromeRefresh2023_: boolean =
      document.documentElement.hasAttribute('chrome-refresh-2023');
  protected columns: number = 4;

  private handler_: ThemeColorPickerHandlerRemote =
      ThemeColorPickerBrowserProxy.getInstance().handler;

  constructor() {
    super();

    this.customColor_ =
        document.documentElement.hasAttribute('chrome-refresh-2023') ?
        EMPTY_COLOR :
        {
          background: {value: 0xffffffff},
          foreground: {value: 0xfff1f3f4},
        };
  }

  override connectedCallback() {
    super.connectedCallback();
    this.setThemeListenerId_ =
        ThemeColorPickerBrowserProxy.getInstance()
            .callbackRouter.setTheme.addListener((theme: Theme) => {
              this.theme_ = theme;
            });
    this.handler_.updateTheme();
  }

  override disconnectedCallback() {
    super.disconnectedCallback();
    ThemeColorPickerBrowserProxy.getInstance().callbackRouter.removeListener(
        this.setThemeListenerId_!);
  }

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;
    if (changedPrivateProperties.has('theme_')) {
      this.defaultColor_ = this.computeDefaultColor_();
      this.greyDefaultColor_ = this.computeGreyDefaultColor_();
      this.mainColor_ = this.computeMainColor_();
      this.showBackgroundColor_ = this.computeShowBackgroundColor_();
      this.showCustomColorBackgroundColor_ =
          this.computeShowCustomColorBackgroundColor_();
      this.showMainColor_ = this.computeShowMainColor_();
      this.updateColors_();
    }

    if (changedPrivateProperties.has('theme_') ||
        changedPrivateProperties.has('colors_')) {
      this.selectedColor_ = this.computeSelectedColor_();
    }

    if (changedPrivateProperties.has('selectedColor_')) {
      this.isDefaultColorSelected_ = this.computeIsDefaultColorSelected_();
      this.isGreyDefaultColorSelected_ =
          this.computeIsGreyDefaultColorSelected_();
      this.isMainColorSelected_ = this.computeIsMainColorSelected_();
      this.isCustomColorSelected_ = this.computeIsCustomColorSelected_();
    }
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    const changedPrivateProperties =
        changedProperties as Map<PropertyKey, unknown>;

    if (changedPrivateProperties.has('colors_') ||
        changedPrivateProperties.has('theme_') ||
        changedPrivateProperties.has('isCustomColorSelected_')) {
      this.updateCustomColor_();
    }
  }

  private computeDefaultColor_(): Color {
    assert(this.theme_);
    if (this.isChromeRefresh2023_) {
      return this.theme_.isDarkMode ? DARK_BASELINE_BLUE_COLOR :
                                      LIGHT_BASELINE_BLUE_COLOR;
    }
    return this.theme_.isDarkMode ? DARK_DEFAULT_COLOR : LIGHT_DEFAULT_COLOR;
  }

  private computeGreyDefaultColor_(): Color {
    assert(this.theme_);
    return this.theme_.isDarkMode ? DARK_BASELINE_GREY_COLOR :
                                    LIGHT_BASELINE_GREY_COLOR;
  }

  private computeMainColor_(): SkColor|null {
    assert(this.theme_);
    return this.theme_.backgroundImageMainColor || null;
  }

  private computeSelectedColor_(): SelectedColor {
    if (!this.colors_ || !this.theme_) {
      return {type: ColorType.NONE};
    }
    if (this.isChromeRefresh2023_ && this.theme_.isGreyBaseline) {
      return {type: ColorType.GREY};
    }
    if (!this.theme_.foregroundColor) {
      return {type: ColorType.DEFAULT};
    }
    if (this.theme_.backgroundImageMainColor &&
        this.theme_.backgroundImageMainColor!.value ===
            this.theme_.seedColor.value) {
      if (this.isChromeRefresh2023_) {
        return {type: ColorType.CUSTOM};
      }
      return {type: ColorType.MAIN};
    }
    if (this.colors_.find(
            (color: ChromeColor) =>
                color.seed.value === this.theme_!.seedColor.value &&
                color.variant === this.theme_!.browserColorVariant)) {
      return {
        type: ColorType.CHROME,
        chromeColor: this.theme_.seedColor,
        variant: this.theme_.browserColorVariant,
      };
    }
    return {type: ColorType.CUSTOM};
  }

  private computeIsDefaultColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.DEFAULT;
  }

  private computeIsGreyDefaultColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.GREY;
  }

  private computeIsMainColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.MAIN;
  }

  private computeIsCustomColorSelected_(): boolean {
    return this.selectedColor_.type === ColorType.CUSTOM;
  }

  protected isChromeColorSelected_(
      color: SkColor, variant: BrowserColorVariant): boolean {
    return this.selectedColor_.type === ColorType.CHROME &&
        this.selectedColor_.chromeColor!.value === color.value &&
        this.selectedColor_.variant === variant;
  }

  protected chromeColorTabIndex_(color: SkColor, variant: BrowserColorVariant):
      string {
    return this.selectedColor_.type === ColorType.CHROME &&
            this.selectedColor_.chromeColor!.value === color.value &&
            this.selectedColor_.variant === variant ?
        '0' :
        '-1';
  }

  protected tabIndex_(selected: boolean): string {
    return selected ? '0' : '-1';
  }

  private themeHasBackgroundImage_(): boolean {
    return !!this.theme_ && !!this.theme_.hasBackgroundImage;
  }

  private computeShowMainColor_(): boolean {
    return !this.isChromeRefresh2023_ && !!this.theme_ &&
        !!this.theme_.backgroundImageMainColor;
  }

  private computeShowBackgroundColor_(): boolean {
    return this.isChromeRefresh2023_ || !this.themeHasBackgroundImage_();
  }

  private computeShowCustomColorBackgroundColor_(): boolean {
    return !this.isChromeRefresh2023_ && !this.themeHasBackgroundImage_();
  }

  protected onDefaultColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    this.handler_.setDefaultColor();
  }

  protected onGreyDefaultColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    this.handler_.setGreyDefaultColor();
  }

  protected onMainColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    this.handler_.setSeedColor(
        this.theme_!.backgroundImageMainColor!, BrowserColorVariant.kTonalSpot);
  }

  protected onChromeColorClick_(e: Event) {
    if (this.handleClickForManagedColors_()) {
      return;
    }

    const index =
        Number.parseInt((e.target as HTMLElement).dataset['index']!, 10);
    const color = this.colors_[index]!;
    this.handler_.setSeedColor(color.seed, color.variant);
  }

  protected onCustomColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }

    if (this.isChromeRefresh2023_) {
      this.$.hueSlider.showAt(this.$.customColorContainer);
    } else {
      this.$.colorPicker.focus();
      this.$.colorPicker.click();
    }
  }

  protected onCustomColorChange_(e: Event) {
    this.handler_.setSeedColor(
        hexColorToSkColor((e.target as HTMLInputElement).value),
        BrowserColorVariant.kTonalSpot);
  }

  protected onSelectedHueChanged_() {
    const selectedHue = this.$.hueSlider.selectedHue;
    if (this.theme_ && this.theme_.seedColorHue === selectedHue) {
      return;
    }

    ThemeColorPickerBrowserProxy.getInstance().handler.setSeedColorFromHue(
        selectedHue);
  }

  private updateCustomColor_() {
    // We only change the custom color when theme updates to a new custom color
    // so that the picked color persists while clicking on other color circles.
    if (!this.isCustomColorSelected_) {
      return;
    }
    assert(this.theme_);
    this.customColor_ = {
      background: this.theme_.backgroundColor,
      foreground: this.theme_.foregroundColor!,
    };
    this.$.colorPickerIcon.style.setProperty(
        'background-color', skColorToRgba(this.theme_.colorPickerIconColor));
    if (this.isChromeRefresh2023_) {
      this.$.hueSlider.selectedHue = this.theme_.seedColorHue;
    }
  }

  private async updateColors_() {
    assert(this.theme_);
    this.colors_ =
        (await this.handler_.getChromeColors(this.theme_.isDarkMode, false))
            .colors;
  }

  protected onManagedDialogClosed_() {
    this.showManagedDialog_ = false;
  }

  private handleClickForManagedColors_(): boolean {
    if (!this.theme_ || !this.theme_.colorsManagedByPolicy) {
      return false;
    }
    this.showManagedDialog_ = true;
    return true;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-theme-color-picker': ThemeColorPickerElement;
  }
}

customElements.define(ThemeColorPickerElement.is, ThemeColorPickerElement);
