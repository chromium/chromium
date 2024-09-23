// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CrRippleMixin} from '../cr_ripple/cr_ripple_mixin.js';

import {getCss} from './cr_radio_button.css.js';
import {getHtml} from './cr_radio_button.html.js';
import {CrRadioButtonMixinLit} from './cr_radio_button_mixin_lit.js';

const CrRadioButtonElementBase =
    CrRippleMixin(CrRadioButtonMixinLit(CrLitElement));

export interface CrRadioButtonElement {
  $: {
    button: HTMLElement,
  };
}

export class CrRadioButtonElement extends CrRadioButtonElementBase {
  static get is() {
    return 'cr-radio-button';
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
    'cr-radio-button': CrRadioButtonElement;
  }
}

customElements.define(CrRadioButtonElement.is, CrRadioButtonElement);
