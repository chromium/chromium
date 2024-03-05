// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CrPaperRippleMixin} from '../cr_paper_ripple_mixin.js';

import {getCss} from './cr_radio_button.css.js';
import {getHtml} from './cr_radio_button.html.js';
import {CrRadioButtonMixinLit} from './cr_radio_button_mixin_lit.js';

const CrRadioButtonElementBase =
    CrPaperRippleMixin(CrRadioButtonMixinLit(CrLitElement));

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

  static override get properties() {
    return {
      hideLabel_: {type: Boolean},
    };
  }

  protected hideLabel_: boolean = true;

  override willUpdate(changedProperties: PropertyValues<this>) {
    super.willUpdate(changedProperties);
    if (changedProperties.has('label')) {
      this.hideLabel_ = !this.label;
    }
  }

  // Overridden from CrRadioButtonMixin
  override getPaperRipple() {
    return this.getRipple();
  }

  // Overridden from CrPaperRippleMixin
  override createRipple() {
    this.rippleContainer = this.shadowRoot!.querySelector('.disc-wrapper');
    const ripple = super.createRipple();
    ripple.id = 'ink';
    ripple.setAttribute('recenters', '');
    ripple.classList.add('circle', 'toggle-ink');
    return ripple;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-radio-button': CrRadioButtonElement;
  }
}

customElements.define(CrRadioButtonElement.is, CrRadioButtonElement);
