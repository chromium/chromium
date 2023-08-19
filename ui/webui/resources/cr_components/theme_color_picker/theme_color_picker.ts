// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './theme_hue_slider_dialog.js';
import './theme_color.js';
import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import 'chrome://resources/cr_components/managed_dialog/managed_dialog.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {hexColorToSkColor, skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {BrowserColorVariant} from 'chrome://resources/mojo/ui/base/mojom/themes.mojom-webui.js';
import {DomRepeat, PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {ThemeColorPickerBrowserProxy} from './browser_proxy.js';
import {Color, ColorType, DARK_BASELINE_BLUE_COLOR, DARK_BASELINE_GREY_COLOR, DARK_DEFAULT_COLOR, LIGHT_BASELINE_BLUE_COLOR, LIGHT_BASELINE_GREY_COLOR, LIGHT_DEFAULT_COLOR, SelectedColor} from './color_utils.js';
import {ThemeColorElement} from './theme_color.js';
import {getTemplate} from './theme_color_picker.html.js';
import {ChromeColor, Theme, ThemeColorPickerHandlerRemote} from './theme_color_picker.mojom-webui.js';
import {ThemeHueSliderDialogElement} from './theme_hue_slider_dialog.js';

const ThemeColorPickerElementBase = I18nMixin(PolymerElement);

export interface ThemeColorPickerElement {
  $: {
    chromeColors: DomRepeat,
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

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      defaultColor_: {
        type: Object,
        computed: 'computeDefaultColor_(theme_)',
      },
      greyDefaultColor_: {
        type: Object,
        computed: 'computeGreyDefaultColor_(theme_)',
      },
      mainColor_: {
        type: Object,
        computed: 'computeMainColor_(theme_)',
      },
      colors_: Array,
      theme_: Object,
      selectedColor_: {
        type: Object,
        computed: 'computeSelectedColor_(theme_, colors_)',
      },
      isDefaultColorSelected_: {
        type: Boolean,
        computed: 'computeIsDefaultColorSelected_(selectedColor_)',
      },
      isGreyDefaultColorSelected_: {
        type: Boolean,
        computed: 'computeIsGreyDefaultColorSelected_(selectedColor_)',
      },
      isMainColorSelected_: {
        type: Boolean,
        computed: 'computeIsMainColorSelected_(selectedColor_)',
      },
      isCustomColorSelected_: {
        type: Boolean,
        computed: 'computeIsCustomColorSelected_(selectedColor_)',
      },
      customColor_: {
        type: Object,
        value: () =>
            document.documentElement.hasAttribute('chrome-refresh-2023') ?
            {} :
            {
              background: {value: 0xffffffff},
              foreground: {value: 0xfff1f3f4},
            },
      },
      showManagedDialog_: Boolean,
      showBackgroundColor_: {
        type: Boolean,
        computed: 'computeShowBackgroundColor_(theme_)',
      },
      showCustomColorBackgroundColor_: {
        type: Boolean,
        computed: 'computeShowCustomColorBackgroundColor_(theme_)',
      },
      showMainColor_: {
        type: Boolean,
        computed: 'computeShowMainColor_(theme_)',
      },
      isChromeRefresh2023_: {
        type: Boolean,
        value: () =>
            document.documentElement.hasAttribute('chrome-refresh-2023'),
      },
      columns: {
        type: Number,
        value: 4,
      },
    };
  }

  static get observers() {
    return [
      'updateCustomColor_(colors_, theme_, isCustomColorSelected_)',
      'updateColors_(theme_)',
    ];
  }

  private colors_: ChromeColor[];
  private theme_: Theme;
  private selectedColor_: SelectedColor;
  private isCustomColorSelected_: boolean;
  private customColor_: Color|undefined;
  private setThemeListenerId_: number|null = null;
  private showManagedDialog_: boolean;
  private isChromeRefresh2023_: boolean;

  private handler_: ThemeColorPickerHandlerRemote;

  override connectedCallback() {
    super.connectedCallback();
    this.handler_ = ThemeColorPickerBrowserProxy.getInstance().handler;
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

  private computeDefaultColor_(): Color {
    if (this.isChromeRefresh2023_) {
      return this.theme_.isDarkMode ? DARK_BASELINE_BLUE_COLOR :
                                      LIGHT_BASELINE_BLUE_COLOR;
    }
    return this.theme_.isDarkMode ? DARK_DEFAULT_COLOR : LIGHT_DEFAULT_COLOR;
  }

  private computeGreyDefaultColor_(): Color {
    return this.theme_.isDarkMode ? DARK_BASELINE_GREY_COLOR :
                                    LIGHT_BASELINE_GREY_COLOR;
  }

  private computeMainColor_(): SkColor|undefined {
    return this.theme_ && this.theme_.backgroundImageMainColor;
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
                color.seed.value === this.theme_.seedColor.value &&
                color.variant === this.theme_.browserColorVariant)) {
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

  private isChromeColorSelected_(color: SkColor, variant: BrowserColorVariant):
      boolean {
    return this.selectedColor_.type === ColorType.CHROME &&
        this.selectedColor_.chromeColor!.value === color.value &&
        this.selectedColor_.variant === variant;
  }

  private boolToString_(value: boolean): string {
    return value ? 'true' : 'false';
  }

  private getChromeColorCheckedStatus_(
      color: SkColor, variant: BrowserColorVariant): string {
    return this.boolToString_(this.isChromeColorSelected_(color, variant));
  }

  private chromeColorTabIndex_(color: SkColor, variant: BrowserColorVariant):
      string {
    return this.selectedColor_.type === ColorType.CHROME &&
            this.selectedColor_.chromeColor!.value === color.value &&
            this.selectedColor_.variant === variant ?
        '0' :
        '-1';
  }

  private tabIndex_(selected: boolean): string {
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

  private onDefaultColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    this.handler_.setDefaultColor();
  }

  private onGreyDefaultColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    this.handler_.setGreyDefaultColor();
  }

  private onMainColorClick_() {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    this.handler_.setSeedColor(
        this.theme_!.backgroundImageMainColor!, BrowserColorVariant.kTonalSpot);
  }

  private onChromeColorClick_(e: Event) {
    if (this.handleClickForManagedColors_()) {
      return;
    }
    const color = this.$.chromeColors.itemForElement(e.target as HTMLElement);
    this.handler_.setSeedColor(color.seed, color.variant);
  }

  private onCustomColorClick_() {
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

  private onCustomColorChange_(e: Event) {
    this.handler_.setSeedColor(
        hexColorToSkColor((e.target as HTMLInputElement).value),
        BrowserColorVariant.kTonalSpot);
  }

  private onSelectedHueChanged_() {
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
    this.colors_ =
        (await this.handler_.getChromeColors(this.theme_.isDarkMode, false))
            .colors;
  }

  private onManagedDialogClosed_() {
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
