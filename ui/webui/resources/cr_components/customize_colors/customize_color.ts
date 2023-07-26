// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './check_mark_wrapper.js';

import {skColorToRgba} from 'chrome://resources/js/color_utils.js';
import {FocusOutlineManager} from 'chrome://resources/js/focus_outline_manager.js';
import {SkColor} from 'chrome://resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './customize_color.html.js';

export interface CustomizeColorElement {
  $: {
    background: Element,
    foreground: Element,
  };
}

export class CustomizeColorElement extends PolymerElement {
  static get is() {
    return 'customize-color';
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

  public backgroundColor: SkColor;
  public foregroundColor: SkColor;
  public baseColor: SkColor;
  public checked: boolean;
  public backgroundColorHidden: boolean;

  override connectedCallback() {
    super.connectedCallback();
    FocusOutlineManager.forDocument(document);
  }

  private onColorChange_() {
    this.updateStyles({
      '--customize-color-foreground-color':
          skColorToRgba(this.foregroundColor ?? 0),
      '--customize-color-background-color':
          skColorToRgba(this.backgroundColor ?? 0),
      '--customize-color-base-color': skColorToRgba(this.baseColor ?? 0),
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'customize-color': CustomizeColorElement;
  }
}

customElements.define(CustomizeColorElement.is, CustomizeColorElement);
