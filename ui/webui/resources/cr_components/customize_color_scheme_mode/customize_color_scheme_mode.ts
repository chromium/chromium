// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_segmented_button/cr_segmented_button.js';
import 'chrome://resources/cr_elements/cr_segmented_button/cr_segmented_button_option.js';

import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {assert} from 'chrome://resources/js/assert_ts.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {CustomizeColorSchemeModeBrowserProxy} from './browser_proxy.js';
import {getTemplate} from './customize_color_scheme_mode.html.js';
import {ColorSchemeMode, CustomizeColorSchemeModeClientCallbackRouter, CustomizeColorSchemeModeHandlerInterface} from './customize_color_scheme_mode.mojom-webui.js';

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

const CustomizeColorSchemeModeElementBase = I18nMixin(PolymerElement);

export class CustomizeColorSchemeModeElement extends
    CustomizeColorSchemeModeElementBase {
  static get is() {
    return 'customize-color-scheme-mode';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      currentMode_: {
        type: Object,
        value: colorSchemeModeOptions[0],
      },
      colorSchemeModeOptions_: {
        type: Object,
        value: colorSchemeModeOptions,
      },
    };
  }

  private currentMode_: ColorSchemeModeOption|undefined;

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
              this.currentMode_ = colorSchemeModeOptions.find(
                  (mode) => colorSchemeMode === mode.value);
            });
    this.handler_.initializeColorSchemeMode();
  }

  override disconnectedCallback() {
    assert(this.setColorSchemeModeListenerId_);
    this.callbackRouter_.removeListener(this.setColorSchemeModeListenerId_);
    super.disconnectedCallback();
  }

  private onSelectedChanged_(e: CustomEvent<{value: string}>) {
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
