// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './segmented_button.js';
import './segmented_button_option.js';

import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {assert} from '//resources/js/assert.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CustomizeColorSchemeModeBrowserProxy} from './browser_proxy.js';
import {getCss} from './customize_color_scheme_mode.css.js';
import {getHtml} from './customize_color_scheme_mode.html.js';
import type {CustomizeColorSchemeModeClientCallbackRouter, CustomizeColorSchemeModeHandlerInterface} from './customize_color_scheme_mode.mojom-webui.js';
import {ColorSchemeMode} from './customize_color_scheme_mode.mojom-webui.js';

export interface ColorSchemeModeOption {
  id: string;
  value: ColorSchemeMode;
}

export const colorSchemeModeOptions: ColorSchemeModeOption[] = [
  {
    id: 'lightMode',
    value: ColorSchemeMode.kLight,
  },
  {
    id: 'darkMode',
    value: ColorSchemeMode.kDark,
  },
  {
    id: 'systemMode',
    value: ColorSchemeMode.kSystem,
  },
];

const CustomizeColorSchemeModeElementBase = I18nMixinLit(CrLitElement);

export class CustomizeColorSchemeModeElement extends
    CustomizeColorSchemeModeElementBase {
  static get is() {
    return 'customize-color-scheme-mode';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      currentMode_: {type: Object, state: true},
      colorSchemeModeOptions_: {type: Array, state: true},
    };
  }

  protected currentMode_: ColorSchemeModeOption = colorSchemeModeOptions[0]!;
  protected readonly colorSchemeModeOptions_: ColorSchemeModeOption[] =
      colorSchemeModeOptions;

  private handler_: CustomizeColorSchemeModeHandlerInterface =
      CustomizeColorSchemeModeBrowserProxy.getInstance().handler;
  private callbackRouter_: CustomizeColorSchemeModeClientCallbackRouter =
      CustomizeColorSchemeModeBrowserProxy.getInstance().callbackRouter;
  private setColorSchemeModeListenerId_: number|null = null;

  override connectedCallback() {
    super.connectedCallback();

    this.setColorSchemeModeListenerId_ =
        this.callbackRouter_.setColorSchemeMode.addListener(
            (colorSchemeMode: ColorSchemeMode) => {
              const currentMode = colorSchemeModeOptions.find(
                  (mode) => colorSchemeMode === mode.value);
              assert(!!currentMode);
              this.currentMode_ = currentMode;
            });
    this.handler_.initializeColorSchemeMode();
  }

  override disconnectedCallback() {
    assert(this.setColorSchemeModeListenerId_);
    this.callbackRouter_.removeListener(this.setColorSchemeModeListenerId_);
    this.setColorSchemeModeListenerId_ = null;
    super.disconnectedCallback();
  }

  protected onSelectedChanged_(e: CustomEvent<{value: string}>) {
    if (!!this.currentMode_ && e.detail.value === this.currentMode_.id) {
      return;
    }
    const selected = colorSchemeModeOptions.find((option) => {
      return option.id === e.detail.value;
    });
    this.handler_.setColorSchemeMode(
        selected ? selected.value : ColorSchemeMode.kSystem);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-color-scheme-mode': CustomizeColorSchemeModeElement;
  }
}

customElements.define(
    CustomizeColorSchemeModeElement.is, CustomizeColorSchemeModeElement);
