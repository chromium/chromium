// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import './check_mark_wrapper.js';

import {skColorToRgba} from '//resources/js/color_utils.js';
import {FocusOutlineManager} from '//resources/js/focus_outline_manager.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import type {SkColor} from '//resources/mojo/skia/public/mojom/skcolor.mojom-webui.js';

import {getCss} from './theme_color.css.js';
import {getHtml} from './theme_color.html.js';

export interface ThemeColorElement {
  $: {
    background: SVGElement,
    foreground: SVGElement,
  };
}

export class ThemeColorElement extends CrLitElement {
  static get is() {
    return 'cr-theme-color';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      backgroundColor: {type: Object},
      foregroundColor: {type: Object},
      baseColor: {type: Object},

      checked: {
        type: Boolean,
        reflect: true,
      },

      backgroundColorHidden: {
        type: Boolean,
        reflect: true,
      },
    };
  }

  backgroundColor: SkColor = {value: 0};
  foregroundColor: SkColor = {value: 0};
  baseColor?: SkColor;
  checked: boolean = false;
  backgroundColorHidden: boolean = false;

  override connectedCallback() {
    super.connectedCallback();
    FocusOutlineManager.forDocument(document);
  }

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('foregroundColor')) {
      this.style.setProperty(
          '--cr-theme-color-foreground-color',
          skColorToRgba(this.foregroundColor));
    }

    if (changedProperties.has('foregroundColor')) {
      this.style.setProperty(
          '--cr-theme-color-background-color',
          skColorToRgba(this.backgroundColor));
    }

    if (changedProperties.has('baseColor')) {
      this.style.setProperty(
          '--cr-theme-color-base-color',
          skColorToRgba(this.baseColor || {value: 0}));
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-theme-color': ThemeColorElement;
  }
}

customElements.define(ThemeColorElement.is, ThemeColorElement);
