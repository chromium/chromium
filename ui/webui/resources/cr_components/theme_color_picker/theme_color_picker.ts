// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './theme_hue_slider_dialog.js';
import './theme_color.js';
import '//resources/cr_elements/cr_grid/cr_grid.js';
import '//resources/cr_components/managed_dialog/managed_dialog.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {skColorToRgba} from '//resources/js/color_utils.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import type {BrowserColorVariant} from '//resources/mojo/ui/base/mojom/themes.mojom-webui.js';

import {ThemeColorPickerBrowserProxy} from './browser_proxy.js';
import {EMPTY_COLOR} from './color_utils.js';
import type {Color, SelectedColor} from './color_utils.js';
import {ColorType, DARK_BASELINE_BLUE_COLOR, DARK_BASELINE_GREY_COLOR, LIGHT_BASELINE_BLUE_COLOR, LIGHT_BASELINE_GREY_COLOR} from './color_utils.js';
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
      defaultColor_: {type: Object, state: true},
      greyDefaultColor_: {type: Object, state: true},
      colors_: {type: Array, state: true},
      theme_: {type: Object, state: true},
      selectedColor_: {type: Object, state: true},
      isDefaultColorSelected_: {type: Boolean, state: true},
      isGreyDefaultColorSelected_: {type: Boolean, state: true},
      isCustomColorSelected_: {type: Boolean, state: true},
      customColor_: {type: Object, state: true},
      showManagedDialog_: {type: Boolean, state: true},
      columns: {type: Number},
    };
  }

  protected defaultColor_: Color = EMPTY_COLOR;
  protected greyDefaultColor_: Color = EMPTY_COLOR;
  protected colors_: ChromeColor[] = [];
  private theme_?: Theme;
  protected selectedColor_: SelectedColor = {type: ColorType.NONE};
  protected isDefaultColorSelected_: boolean = false;
  protected isGreyDefaultColorSelected_: boolean = false;
  protected isCustomColorSelected_: boolean = false;
  protected customColor_: Color = EMPTY_COLOR;
  private setThemeListenerId_: number|null = null;

  protected showManagedDialog_: boolean = false;
  columns: number = 4;

  private handler_: ThemeColorPickerHandlerRemote =
      ThemeColorPickerBrowserProxy.getInstance().handler;

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
      return this.theme_.isDarkMode ? DARK_BASELINE_BLUE_COLOR :
                                      LIGHT_BASELINE_BLUE_COLOR;
  }

  private computeGreyDefaultColor_(): Color {
    assert(this.theme_);
    return this.theme_.isDarkMode ? DARK_BASELINE_GREY_COLOR :
                                    LIGHT_BASELINE_GREY_COLOR;
  }

  private computeSelectedColor_(): SelectedColor {
    if (!this.colors_ || !this.theme_) {
      return {type: ColorType.NONE};
    }
    if (this.theme_.isGreyBaseline) {
      return {type: ColorType.GREY};
    }
    if (!this.theme_.foregroundColor) {
      return {type: ColorType.DEFAULT};
    }
    if (this.theme_.backgroundImageMainColor &&
        this.theme_.backgroundImageMainColor!.value ===
            this.theme_.seedColor.value) {
        return {type: ColorType.CUSTOM};
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

    this.$.hueSlider.showAt(this.$.customColorContainer);
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
    this.$.hueSlider.selectedHue = this.theme_.seedColorHue;
  }

  private async updateColors_() {
    assert(this.theme_);
    this.colors_ =
        (await this.handler_.getChromeColors(this.theme_.isDarkMode)).colors;
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
