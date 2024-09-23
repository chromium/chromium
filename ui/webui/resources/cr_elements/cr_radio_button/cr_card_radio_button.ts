// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * 'cr-card-radio-button' is a radio button in the style of a card. A checkmark
 * is displayed in the upper right hand corner if the radio button is selected.
 */
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CrRippleMixin} from '../cr_ripple/cr_ripple_mixin.js';
import '../cr_icon/cr_icon.js';

import {getCss} from './cr_card_radio_button.css.js';
import {getHtml} from './cr_card_radio_button.html.js';
import {CrRadioButtonMixinLit} from './cr_radio_button_mixin_lit.js';

const CrCardRadioButtonElementBase =
    CrRippleMixin(CrRadioButtonMixinLit(CrLitElement));

export interface CrCardRadioButtonElement {
  $: {
    button: HTMLElement,
  };
}

export class CrCardRadioButtonElement extends CrCardRadioButtonElementBase {
  static get is() {
    return 'cr-card-radio-button';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // Overridden from CrRippleMixin
  override createRipple() {
    this.rippleContainer = this.shadowRoot!.querySelector('.disc-wrapper');
    const ripple = super.createRipple();
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle');
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-card-radio-button': CrCardRadioButtonElement;
  }
}

customElements.define(CrCardRadioButtonElement.is, CrCardRadioButtonElement);
