// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_icon_button/cr_icon_button.js';
import 'chrome://resources/cr_elements/cr_icons.css.js';
import 'chrome://resources/cr_elements/cr_slider/cr_slider.js';

import {CrSliderElement} from 'chrome://resources/cr_elements/cr_slider/cr_slider.js';
import {I18nMixin} from 'chrome://resources/cr_elements/i18n_mixin.js';
import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './theme_hue_slider_dialog.html.js';

const minHue = 0;
const maxHue = 359;

/**
 * Compute a CSS linear-gradient that starts with minHue and ends with maxHue.
 */
const hueDivisions = 6;
const hueGradientParts: string[] = [];
for (let i = 0; i <= hueDivisions; i++) {
  const percentage = i / hueDivisions;
  const hsl = `hsl(${minHue + (maxHue - minHue) * percentage}, 100%, 50%)`;
  hueGradientParts.push(`${hsl} ${percentage * 100}%`);
}
const hueGradient = `linear-gradient(to right, ${hueGradientParts.join(',')})`;

export interface ThemeHueSliderDialogElement {
  $: {
    dialog: HTMLDialogElement,
    slider: CrSliderElement,
  };
}

const ThemeHueSliderDialogElementBase = I18nMixin(PolymerElement);

export class ThemeHueSliderDialogElement extends
    ThemeHueSliderDialogElementBase {
  static get is() {
    return 'cr-theme-hue-slider-dialog';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /* Linear gradient for the background of the slider's track. */
      hueGradient_: {
        type: String,
        value: hueGradient,
      },
      maxHue_: {
        type: Number,
        value: maxHue,
      },
      minHue_: {
        type: Number,
        value: minHue,
      },
      /* The committed value of the slider. */
      selectedHue: {
        type: Number,
        value: minHue,
        observer: 'onSelectedHueChanged_',
      },
      /* The hue value to show in the knob during drag. */
      knobHue_: {
        type: Number,
        value: minHue,
      },
    };
  }

  private hueGradient_: string;
  private maxHue_: number;
  private minHue_: number;
  selectedHue: number;
  private knobHue_: number;
  private boundPointerdown_: (e: PointerEvent) => void;

  constructor() {
    super();

    this.boundPointerdown_ = this.onDocumentPointerdown_.bind(this);
  }

  private onSelectedHueChanged_() {
    this.knobHue_ = this.selectedHue;
  }

  private onCrSliderValueChanged_() {
    this.knobHue_ = this.$.slider.value;
  }

  showAt(anchor: HTMLElement) {
    this.$.dialog.show();

    this.$.dialog.style.left = `${
        anchor.offsetLeft + anchor.offsetWidth - this.$.dialog.offsetWidth}px`;

    // By default, align the dialog below the anchor. If the window is too
    // small, show it above the anchor.
    if (anchor.getBoundingClientRect().bottom + this.$.dialog.offsetHeight >=
        window.innerHeight) {
      this.$.dialog.style.top =
          `${anchor.offsetTop - this.$.dialog.offsetHeight}px`;
    } else {
      this.$.dialog.style.top = `${anchor.offsetTop + anchor.offsetHeight}px`;
    }

    document.addEventListener('pointerdown', this.boundPointerdown_);
  }

  hide() {
    this.$.dialog.close();
    document.removeEventListener('pointerdown', this.boundPointerdown_);
  }

  private onDocumentPointerdown_(e: PointerEvent) {
    if (e.composedPath().includes(this.$.dialog)) {
      return;
    }

    this.hide();
  }

  private updateSelectedHueValue_() {
    this.selectedHue = this.$.slider.value;
    this.dispatchEvent(new CustomEvent(
        'selected-hue-changed', {detail: {selectedHue: this.selectedHue}}));
  }
}

customElements.define(
    ThemeHueSliderDialogElement.is, ThemeHueSliderDialogElement);

declare global {
  interface HTMLElementTagNameMap {
    'cr-theme-hue-slider-dialog': ThemeHueSliderDialogElement;
  }
}
