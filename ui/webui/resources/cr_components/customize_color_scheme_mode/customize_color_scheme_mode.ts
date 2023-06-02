// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_segmented_button/cr_segmented_button.js';
import 'chrome://resources/cr_elements/cr_segmented_button/cr_segmented_button_option.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_color_scheme_mode.html.js';

interface ColorSchemeModeOption {
  id: string;
  name: string;
}

const colorSchemeModeOptions: ColorSchemeModeOption[] = [
  {
    id: 'lightMode',
    name: 'Light',
  },
  {
    id: 'darkMode',
    name: 'Dark',
  },
  {
    id: 'systemMode',
    name: 'System',
  },
];

export class CustomizeColorSchemeModeElement extends PolymerElement {
  static get is() {
    return 'customize-color-scheme-mode';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      currentMode_: {
        type: String,
        value: colorSchemeModeOptions[0],
      },
      colorSchemeModeOptions_: {
        type: Object,
        value: colorSchemeModeOptions,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-color-scheme-mode': CustomizeColorSchemeModeElement;
  }
}

customElements.define(
    CustomizeColorSchemeModeElement.is, CustomizeColorSchemeModeElement);
