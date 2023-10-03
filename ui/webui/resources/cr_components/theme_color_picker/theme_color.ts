// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './check_mark_wrapper.js';

import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './theme_color.html.js';

export interface ThemeColorElement {
  $: {
    background: Element,
    foreground: Element,
  };
}

export class ThemeColorElement extends PolymerElement {
  static get is() {
    return 'cr-theme-color';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      backgroundColor: {
        type: Object,
        value: 0,
        observer: 'onColorChange_',
      },
      foregroundColor: {
        type: Object,
        value: 0,
        observer: 'onColorChange_',
      },
      baseColor: {
        type: Object,
        value: 0,
        observer: 'onColorChange_',
      },
      checked: {
        type: Boolean,
        reflectToAttribute: true,
      },
      backgroundColorHidden: {
        type: Boolean,
        reflectToAttribute: true,
      },
    };
  }

  backgroundColor: SkColor;
  foregroundColor: SkColor;
  baseColor: SkColor;
  checked: boolean;
  backgroundColorHidden: boolean;

  override connectedCallback() {
    super.connectedCallback();
    FocusOutlineManager.forDocument(document);
  }

  private onColorChange_() {
    this.updateStyles({
      '--cr-theme-color-foreground-color':
          skColorToRgba(this.foregroundColor ?? 0),
      '--cr-theme-color-background-color':
          skColorToRgba(this.backgroundColor ?? 0),
      '--cr-theme-color-base-color': skColorToRgba(this.baseColor ?? 0),
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-theme-color': ThemeColorElement;
  }
}

customElements.define(ThemeColorElement.is, ThemeColorElement);
