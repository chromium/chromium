// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '//resources/cr_elements/icons_lit.html.js';
import '//resources/cr_elements/cr_icon/cr_icon.js';

import {CrRadioButtonMixinLit} from '//resources/cr_elements/cr_radio_button/cr_radio_button_mixin_lit.js';
import {CrRippleMixin} from '//resources/cr_elements/cr_ripple/cr_ripple_mixin.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './segmented_button_option.css.js';
import {getHtml} from './segmented_button_option.html.js';

const SegmentedButtonElementBase =
    CrRippleMixin(CrRadioButtonMixinLit(CrLitElement));

export class SegmentedButtonOptionElement extends
    SegmentedButtonElementBase {
  static get is() {
    return 'segmented-button-option';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  // Overridden from CrRippleMixin
  override createRipple() {
    this.rippleContainer = this.shadowRoot!.querySelector('#button');
    return super.createRipple();
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'segmented-button-option': SegmentedButtonOptionElement;
  }
}

customElements.define(
    SegmentedButtonOptionElement.is, SegmentedButtonOptionElement);
