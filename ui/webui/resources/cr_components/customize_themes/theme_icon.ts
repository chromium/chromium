// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../../cr_elements/cr_shared_vars.css.js';

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './theme_icon.html.js';

/**
 * Represents a theme. Displayed as a circle with each half colored based on
 * the custom CSS properties |--cr-theme-icon-frame-color| and
 * |--cr-theme-icon-active-tab-color|. Can be selected by setting the
 * |selected| attribute.
 */
export class ThemeIconElement extends PolymerElement {
  static get is() {
    return 'cr-theme-icon';
  }

  static get template() {
    return getTemplate();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-theme-icon': ThemeIconElement;
  }
}

customElements.define(ThemeIconElement.is, ThemeIconElement);
