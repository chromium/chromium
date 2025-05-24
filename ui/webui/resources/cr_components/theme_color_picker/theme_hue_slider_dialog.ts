// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/cr_icon_button/cr_icon_button.js';
import '//resources/cr_elements/cr_slider/cr_slider.js';

import type {CrSliderElement} from '//resources/cr_elements/cr_slider/cr_slider.js';
import {I18nMixinLit} from '//resources/cr_elements/i18n_mixin_lit.js';
import {EventTracker} from '//resources/js/event_tracker.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './theme_hue_slider_dialog.css.js';
import {getHtml} from './theme_hue_slider_dialog.html.js';

const minHue = 0;
const maxHue = 359;

/**
 * Compute a CSS linear-gradient that starts with minHue and ends with maxHue.
 */

function computeHueGradient(): string {
  const hueDivisions = 6;
  const hueGradientParts: string[] = [];
  for (let i = 0; i <= hueDivisions; i++) {
    const percentage = i / hueDivisions;
    const hsl = `hsl(${minHue + (maxHue - minHue) * percentage}, 100%, 50%)`;
    hueGradientParts.push(`${hsl} ${percentage * 100}%`);
  }
  return hueGradientParts.join(',');
}

export interface ThemeHueSliderDialogElement {
  $: {
    contentsWrapper: HTMLElement,
    dialog: HTMLDialogElement,
    slider: CrSliderElement,
  };
}

const ThemeHueSliderDialogElementBase = I18nMixinLit(CrLitElement);

export class ThemeHueSliderDialogElement extends
    ThemeHueSliderDialogElementBase {
  static get is() {
    return 'cr-theme-hue-slider-dialog';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      /* Linear gradient for the background of the slider's track. */
      hueGradient_: {type: String, state: true},

      maxHue_: {type: Number, state: true},
      minHue_: {type: Number, state: true},

      /* The committed value of the slider. */
      selectedHue: {type: Number},

      /* The hue value to show in the knob during drag. */
      knobHue_: {type: Number, state: true},
    };
  }

  protected accessor hueGradient_: string = computeHueGradient();
  protected accessor maxHue_: number = maxHue;
  protected accessor minHue_: number = minHue;
  accessor selectedHue: number = minHue;
  protected accessor knobHue_: number = minHue;
  private eventTracker_: EventTracker = new EventTracker();

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);

    if (changedProperties.has('selectedHue')) {
      this.knobHue_ = this.selectedHue;
    }
  }

  protected onCrSliderValueChanged_() {
    this.knobHue_ = this.$.slider.value;
  }

  showAt(anchor: HTMLElement) {
    this.$.dialog.showModal();

    const anchorBoundingClientRect = anchor.getBoundingClientRect();
    this.$.dialog.style.left =
        `${anchorBoundingClientRect.right - this.$.dialog.offsetWidth}px`;

    // By default, align the dialog below the anchor. If the window is too
    // small, show it above the anchor.
    if (anchorBoundingClientRect.bottom + this.$.dialog.offsetHeight >=
        window.innerHeight) {
      this.$.dialog.style.top =
          `${anchorBoundingClientRect.top - this.$.dialog.offsetHeight}px`;
    } else {
      this.$.dialog.style.top = `${anchorBoundingClientRect.bottom}px`;
    }

    this.eventTracker_.add(
        this.$.dialog, 'pointerdown', this.onPointerdown_.bind(this));
  }

  hide() {
    this.$.dialog.close();
    this.eventTracker_.removeAll();
  }

  private onPointerdown_(e: PointerEvent) {
    if (e.button !== 0 || e.composedPath()[0]! !== this.$.dialog) {
      return;
    }

    this.hide();
  }

  protected updateSelectedHueValue_() {
    this.selectedHue = this.$.slider.value;
    this.fire('selected-hue-changed', {selectedHue: this.selectedHue});
  }
}

customElements.define(
    ThemeHueSliderDialogElement.is, ThemeHueSliderDialogElement);

declare global {
  interface HTMLElementTagNameMap {
    'cr-theme-hue-slider-dialog': ThemeHueSliderDialogElement;
  }
}
