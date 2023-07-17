// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_grid/cr_grid.js';
import './customize_color.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {LIGHT_BASELINE_GREY_COLOR, LIGHT_DEFAULT_COLOR} from './color_utils.js';
import {getTemplate} from './customize_colors.html.js';

export class CustomizeColorsElement extends PolymerElement {
  static get is() {
    return 'customize-colors';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      defaultColor_: {
        type: Object,
        value: LIGHT_DEFAULT_COLOR,
      },
      greyDefaultColor_: {
        type: Object,
        value: LIGHT_BASELINE_GREY_COLOR,
      },
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-colors': CustomizeColorsElement;
  }
}

customElements.define(CustomizeColorsElement.is, CustomizeColorsElement);
