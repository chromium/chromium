// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../icons.html.js';
import '../cr_icon/cr_icon.js';

import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {CrRadioButtonMixinLit} from '../cr_radio_button/cr_radio_button_mixin_lit.js';
import {CrRippleMixin} from '../cr_ripple/cr_ripple_mixin.js';

import {getCss} from './cr_segmented_button_option.css.js';
import {getHtml} from './cr_segmented_button_option.html.js';

const CrSegmentedButtonOptionElementBase =
    CrRippleMixin(CrRadioButtonMixinLit(CrLitElement));

export class CrSegmentedButtonOptionElement extends
    CrSegmentedButtonOptionElementBase {
  static get is() {
    return 'cr-segmented-button-option';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // Overridden from CrRippleMixin
  override createRipple() {
    this.rippleContainer = this.shadowRoot.querySelector('#button');
    return super.createRipple();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-segmented-button-option': CrSegmentedButtonOptionElement;
  }
}

customElements.define(
    CrSegmentedButtonOptionElement.is, CrSegmentedButtonOptionElement);
